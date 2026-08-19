/*
 * Myco Mini — application Zigbee (add-on R23 / ZBOSS). Un seul endpoint
 * combinant Temperature, Humidity, Illuminance, Power Config (batterie) et
 * Soil Moisture (cluster custom, cf zb_soil_moisture_defs.h) — pattern
 * repris du projet b-parasite (github.com/rbaron/b-parasite), qui fait un
 * capteur de plante Zigbee quasi identique.
 *
 * Role Router (pas sleepy end device) pour cette première mise en service —
 * plus simple, pas de tuning de poll interval. À revoir si déploiement
 * sur pile CR2032 (passer en End Device + zigbee_configure_sleepy_behavior).
 */

#include "zigbee_app.h"

#include <math.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <addons/zcl/zb_zcl_temp_measurement_addons.h>
#include <zb_nrf_platform.h>
#include <zboss_api.h>
#include <zcl/zb_zcl_power_config.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <osif/zb_transceiver.h>

#include "device_config.h"
#include "zb_endpoint_defs.h"
#include "zb_soil_moisture_defs.h"

LOG_MODULE_REGISTER(myco_zigbee, LOG_LEVEL_INF);

/* ── Périphériques (soil_pwr/adc_channel_setup deja faits par main()) ──── */
static const struct device *sht4x  = DEVICE_DT_GET(DT_NODELABEL(sht4x));
static const struct device *bh1750 = DEVICE_DT_GET(DT_NODELABEL(bh1750));

static const struct adc_dt_spec adc_soil =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static const struct adc_dt_spec adc_batt =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

/* Calibration sol + intervalle de report : lus une fois au boot depuis la
 * config persistee (device_config.c), modifiable via le mode config. */
static const struct device_config *g_cfg;

/* ── Attributs ZCL supplémentaires (pas de struct addon ZBOSS pour
 * humidité/illuminance/sol — définis nous-mêmes, comme b-parasite). ──── */
typedef struct {
	zb_uint16_t rel_humidity;
	zb_uint16_t min_val;
	zb_uint16_t max_val;
} myco_rel_humidity_attrs_t;

typedef struct {
	zb_uint8_t voltage;
	zb_uint8_t percentage;
	zb_uint8_t quantity;
	enum zb_zcl_power_config_battery_size_e size;
	/* Champs "optionnels" (NULL accepte par la macro EXT) mais que
	 * fill_alarm_state_bitmap() deref quand meme en interne dans cette
	 * version de ZBOSS (precompilee, pas de source dispo pour verifier
	 * exactement lesquels) — bus fault garanti sur n'importe lequel resté
	 * NULL. Tous alloues, mis a 0 (= pas de seuil / alarme jamais
	 * declenchee pour ce test). */
	zb_uint8_t rated_voltage;
	zb_uint8_t alarm_mask;
	zb_uint8_t voltage_min_threshold;
	zb_uint8_t voltage_threshold1;
	zb_uint8_t voltage_threshold2;
	zb_uint8_t voltage_threshold3;
	zb_uint8_t percentage_min_threshold;
	zb_uint8_t percentage_threshold1;
	zb_uint8_t percentage_threshold2;
	zb_uint8_t percentage_threshold3;
	zb_uint32_t alarm_state;
} myco_batt_attrs_t;

typedef struct {
	zb_uint16_t percentage;
} myco_soil_moisture_attrs_t;

typedef struct {
	zb_uint16_t log_lux;
} myco_illuminance_attrs_t;

struct zb_device_ctx {
	zb_zcl_basic_attrs_t basic_attr;
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_temp_measurement_attrs_t temp_measure_attrs;
	myco_rel_humidity_attrs_t rel_humidity_attrs;
	myco_batt_attrs_t batt_attrs;
	myco_soil_moisture_attrs_t soil_moisture_attrs;
	myco_illuminance_attrs_t illuminance_attrs;
};

static struct zb_device_ctx dev_ctx;

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(
	identify_attr_list,
	&dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST(
	basic_attr_list,
	&dev_ctx.basic_attr.zcl_version,
	&dev_ctx.basic_attr.power_source);

ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(
	temp_measurement_attr_list,
	&dev_ctx.temp_measure_attrs.measure_value,
	&dev_ctx.temp_measure_attrs.min_measure_value,
	&dev_ctx.temp_measure_attrs.max_measure_value,
	&dev_ctx.temp_measure_attrs.tolerance);

ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(
	rel_humi_attr_list,
	&dev_ctx.rel_humidity_attrs.rel_humidity,
	&dev_ctx.rel_humidity_attrs.min_val,
	&dev_ctx.rel_humidity_attrs.max_val);

/* bat_num : token attendu par la macro EXT, cf.
 * devzone.nordicsemi.com/f/nordic-q-a/85315 */
#define bat_num
ZB_ZCL_DECLARE_POWER_CONFIG_BATTERY_ATTRIB_LIST_EXT(
	batt_attr_list,
	&dev_ctx.batt_attrs.voltage,
	/*size=*/&dev_ctx.batt_attrs.size,
	/*quantity=*/&dev_ctx.batt_attrs.quantity,
	/*rated_voltage=*/&dev_ctx.batt_attrs.rated_voltage,
	/*alarm_mask=*/&dev_ctx.batt_attrs.alarm_mask,
	/*voltage_min_threshold=*/&dev_ctx.batt_attrs.voltage_min_threshold,
	/*percentage_remaining=*/&dev_ctx.batt_attrs.percentage,
	/*threshold1=*/&dev_ctx.batt_attrs.voltage_threshold1,
	/*threshold2=*/&dev_ctx.batt_attrs.voltage_threshold2,
	/*threshold3=*/&dev_ctx.batt_attrs.voltage_threshold3,
	/*percentage_min_threshold=*/&dev_ctx.batt_attrs.percentage_min_threshold,
	/*percentage_threshold1=*/&dev_ctx.batt_attrs.percentage_threshold1,
	/*percentage_threshold2=*/&dev_ctx.batt_attrs.percentage_threshold2,
	/*percentage_threshold3=*/&dev_ctx.batt_attrs.percentage_threshold3,
	/*alarm_state=*/&dev_ctx.batt_attrs.alarm_state);

MYCO_ZB_ZCL_DECLARE_SOIL_MOISTURE_ATTRIB_LIST(
	soil_moisture_attr_list,
	&dev_ctx.soil_moisture_attrs.percentage);

ZB_ZCL_DECLARE_ILLUMINANCE_MEASUREMENT_ATTRIB_LIST(
	illuminance_attr_list,
	/*value=*/&dev_ctx.illuminance_attrs.log_lux,
	/*min_value=*/NULL,
	/*max_value=*/NULL);

MYCO_ZB_DECLARE_CLUSTER_LIST(
	myco_clusters,
	basic_attr_list,
	identify_attr_list,
	temp_measurement_attr_list,
	rel_humi_attr_list,
	batt_attr_list,
	soil_moisture_attr_list,
	illuminance_attr_list);

MYCO_ZB_DECLARE_ENDPOINT(
	myco_ep,
	MYCO_ZIGBEE_ENDPOINT,
	myco_clusters);

ZBOSS_DECLARE_DEVICE_CTX_1_EP(
	myco_ctx,
	myco_ep);

/* ── ADC (identique à la version BLE) ──────────────────────────── */
static int read_adc_mv(const struct adc_dt_spec *spec)
{
	int16_t raw = 0;
	struct adc_sequence seq = {
		.buffer      = &raw,
		.buffer_size = sizeof(raw),
	};
	adc_sequence_init_dt(spec, &seq);
	adc_read_dt(spec, &seq);

	int32_t mv = raw;
	adc_raw_to_millivolts_dt(spec, &mv);
	return mv;
}

static void set_attr(zb_uint16_t cluster_id, zb_uint16_t attr_id, void *data)
{
	zb_zcl_set_attr_val(MYCO_ZIGBEE_ENDPOINT, cluster_id,
			    ZB_ZCL_CLUSTER_SERVER_ROLE, attr_id,
			    (zb_uint8_t *)data, ZB_FALSE);
}

/* ── Callback périodique : lit les capteurs et met à jour les attributs
 * ZCL (déclenche le reporting Zigbee automatiquement). ──────────────── */
static void update_sensors_cb(zb_uint8_t param)
{
	ZVUNUSED(param);

	zb_ret_t ret = ZB_SCHEDULE_APP_ALARM(
		update_sensors_cb, 0,
		ZB_MILLISECONDS_TO_BEACON_INTERVAL(1000 * g_cfg->report_interval_s));
	if (ret != RET_OK) {
		LOG_ERR("Unable to reschedule sensor update callback");
	}

	struct sensor_value temp, hum, lux;

	sensor_sample_fetch(sht4x);
	sensor_channel_get(sht4x, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	sensor_channel_get(sht4x, SENSOR_CHAN_HUMIDITY, &hum);

	sensor_sample_fetch(bh1750);
	sensor_channel_get(bh1750, SENSOR_CHAN_LIGHT, &lux);

	int batt_mv  = read_adc_mv(&adc_batt);
	int batt_pct = CLAMP((batt_mv - 2000) * 100 / (3000 - 2000), 0, 100);

	int soil_mv  = read_adc_mv(&adc_soil);
	int soil_12  = soil_mv * 4095 / 3600;
	int soil_pct = CLAMP((g_cfg->soil_dry - soil_12) * 100 / (g_cfg->soil_dry - g_cfg->soil_wet), 0, 100);

	float t_f   = temp.val1 + temp.val2 / 1000000.0f;
	float h_f   = hum.val1  + hum.val2  / 1000000.0f;
	float lux_f = lux.val1  + lux.val2  / 1000000.0f;

	LOG_INF("T: %.2f C  RH: %.2f %%  Lux: %.2f  Sol: %d%%  Bat: %dmV (%d%%)",
		(double)t_f, (double)h_f, (double)lux_f, soil_pct, batt_mv, batt_pct);

	/* Temperature Measurement : int16, unites 0.01 C */
	zb_int16_t temperature_value = (zb_int16_t)(t_f * 100.0f);
	set_attr(ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
		 ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &temperature_value);

	/* Relative Humidity Measurement : uint16, unites 0.01 % */
	zb_uint16_t rel_humi = (zb_uint16_t)(h_f * 100.0f);
	set_attr(ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
		 ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &rel_humi);

	/* Soil moisture (cluster custom) : uint16, unites 0.01 %, meme
	 * convention que Relative Humidity. */
	zb_uint16_t soil_moisture = (zb_uint16_t)(soil_pct * 100);
	set_attr(MYCO_ZB_ZCL_SOIL_MOISTURE_CLUSTER_ID,
		 MYCO_ZB_ZCL_ATTR_SOIL_MOISTURE_VALUE_ID, &soil_moisture);

	/* Illuminance : formule ZCL standard 10000*log10(lux)+1 (spec 4.2.2.2).
	 * Lux=0 -> log10 indefini, on plancher a 1 lux. */
	float lux_clamped = lux_f < 1.0f ? 1.0f : lux_f;
	zb_uint16_t log_lux = (zb_uint16_t)(10000.0f * log10f(lux_clamped) + 1.0f);
	set_attr(ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT,
		 ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID, &log_lux);

	/* Power Config : voltage en unites 100mV, percentage en unites 0.5% */
	zb_uint8_t voltage_attr = (zb_uint8_t)(batt_mv / 100);
	set_attr(ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
		 ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, &voltage_attr);

	zb_uint8_t percentage_attr = (zb_uint8_t)(2 * batt_pct);
	set_attr(ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
		 ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &percentage_attr);
}

static void identify_cb(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);
	LOG_INF("Identify: commande recue du coordinateur");
}

void zboss_signal_handler(zb_bufid_t bufid)
{
	zb_zdo_app_signal_hdr_t *sig_hndler = NULL;
	zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, &sig_hndler);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

	switch (sig) {
	case ZB_BDB_SIGNAL_STEERING:
	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
		if (status == RET_OK) {
			LOG_INF("Joined Zigbee network successfully");
		} else {
			LOG_WRN("Network steering failed, status: %d", status);
		}
		break;
	case ZB_ZDO_SIGNAL_SKIP_STARTUP:
		LOG_INF("Zigbee stack started, waiting for network");
		ZB_ERROR_CHECK(ZB_SCHEDULE_APP_ALARM(
			update_sensors_cb, 0,
			ZB_MILLISECONDS_TO_BEACON_INTERVAL(1000)));
		break;
	case ZB_ZDO_SIGNAL_LEAVE:
		if (status == RET_OK) {
			LOG_WRN("Left Zigbee network");
		}
		break;
	default:
		break;
	}

	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
	if (bufid) {
		zb_buf_free(bufid);
	}
}

int zigbee_app_run(void)
{
	g_cfg = device_config_get();

	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;
	dev_ctx.batt_attrs.quantity = 1;
	dev_ctx.batt_attrs.size = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_OTHER;
	dev_ctx.batt_attrs.alarm_state = 0;

	ZB_AF_REGISTER_DEVICE_CTX(&myco_ctx);
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(MYCO_ZIGBEE_ENDPOINT, identify_cb);

	/* TX power par defaut = 0dBm (nrf_802154_pib.c). +4dBm = max documente
	 * pour ce module cote BLE (meme PA physique que le 802.15.4) — utile
	 * pour la portee, notamment avec l'antenne integree du module sur ce
	 * petit breakout. */
	zb_trans_set_tx_power(4);

	zigbee_enable();

	LOG_INF("Zigbee stack enabled");

	return 0;
}
