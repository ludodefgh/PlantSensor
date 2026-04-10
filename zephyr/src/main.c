#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

LOG_MODULE_REGISTER(myco, LOG_LEVEL_DBG);

/* ── Périphériques ──────────────────────────────────────────── */
static const struct device *sht4x  = DEVICE_DT_GET(DT_NODELABEL(sht4x));
static const struct device *bh1750 = DEVICE_DT_GET(DT_NODELABEL(bh1750));

static const struct gpio_dt_spec boost_en =
	GPIO_DT_SPEC_GET(DT_ALIAS(boost_en), gpios);
static const struct gpio_dt_spec soil_pwr =
	GPIO_DT_SPEC_GET(DT_ALIAS(soil_pwr), gpios);
static const struct gpio_dt_spec user_led =
	GPIO_DT_SPEC_GET(DT_ALIAS(user_led), gpios);

static const struct adc_dt_spec adc_soil =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static const struct adc_dt_spec adc_batt =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

/* ── Calibration ────────────────────────────────────────────── */
#define SOIL_DRY 3500
#define SOIL_WET 1260

/* ── BTHome v2 ──────────────────────────────────────────────── */
/* Service data : UUID(2) + device_info(1) + objects = 17 bytes */
static uint8_t bthome_sd[17] = {
	0xD2, 0xFC,  /* UUID 0xFCD2 little endian */
	0x40,        /* v2, unencrypted, not trigger-based */
	0x01, 0x00,              /* Battery  uint8  % */
	0x02, 0x00, 0x00,        /* Temp     sint16 ×0.01 °C */
	0x03, 0x00, 0x00,        /* Humidity uint16 ×0.01 % */
	0x05, 0x00, 0x00, 0x00,  /* Lux      uint24 ×0.01 lx */
	0x2F, 0x00,              /* Moisture uint8  % */
};

static const uint8_t bthome_uuid[] = {0xD2, 0xFC};  /* 0xFCD2 LE */

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_UUID16_ALL, bthome_uuid, sizeof(bthome_uuid)),
	BT_DATA(BT_DATA_SVC_DATA16, bthome_sd, sizeof(bthome_sd)),
};

static const struct bt_le_adv_param adv_param =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_USE_IDENTITY |
			     BT_LE_ADV_OPT_CONN,
			     160, 160,
			     NULL);

static void bthome_update(uint8_t bat, float temp, float hum, float lux, uint8_t soil)
{
	int16_t  t  = (int16_t)(temp * 100.0f);
	uint16_t h  = (uint16_t)(hum  * 100.0f);
	uint32_t lx = (uint32_t)(lux  * 100.0f);

	bthome_sd[4]  = bat;
	bthome_sd[6]  = (t  >>  0) & 0xFF;
	bthome_sd[7]  = (t  >>  8) & 0xFF;
	bthome_sd[9]  = (h  >>  0) & 0xFF;
	bthome_sd[10] = (h  >>  8) & 0xFF;
	bthome_sd[12] = (lx >>  0) & 0xFF;
	bthome_sd[13] = (lx >>  8) & 0xFF;
	bthome_sd[14] = (lx >> 16) & 0xFF;
	bthome_sd[16] = soil;
}

/* ── ADC ────────────────────────────────────────────────────── */
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

/* ── Main ───────────────────────────────────────────────────── */
int main(void)
{
	LOG_INF("Myco booting on nRF54L15...");

	gpio_pin_configure_dt(&boost_en, GPIO_OUTPUT_ACTIVE);
	gpio_pin_configure_dt(&soil_pwr, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&user_led, GPIO_OUTPUT_ACTIVE);  /* LED ON = système actif */
	adc_channel_setup_dt(&adc_soil);
	adc_channel_setup_dt(&adc_batt);
	k_msleep(20);

	if (!device_is_ready(sht4x) || !device_is_ready(bh1750)) {
		LOG_ERR("Sensor not ready");
		return -1;
	}

	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed: %d", err);
		return -1;
	}

	/* Log adresse MAC */
	bt_addr_le_t addr;
	size_t count = 1;
	bt_id_get(&addr, &count);
	LOG_INF("BLE ready  MAC: %02X:%02X:%02X:%02X:%02X:%02X",
		addr.a.val[5], addr.a.val[4], addr.a.val[3],
		addr.a.val[2], addr.a.val[1], addr.a.val[0]);

	/* Démarrage advertising au boot */
	err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("bt_le_adv_start failed: %d", err);
		return -1;
	}

	struct sensor_value temp, hum, lux;

	while (1) {
		/* SHT40 */
		sensor_sample_fetch(sht4x);
		sensor_channel_get(sht4x, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_channel_get(sht4x, SENSOR_CHAN_HUMIDITY, &hum);

		/* BH1750 */
		sensor_sample_fetch(bh1750);
		sensor_channel_get(bh1750, SENSOR_CHAN_LIGHT, &lux);

		/* Batterie */
		int batt_mv  = read_adc_mv(&adc_batt);
		int batt_pct = CLAMP((batt_mv - 2000) * 100 / (3000 - 2000), 0, 100);

		/* Sol */
		gpio_pin_set_dt(&soil_pwr, 1);
		k_msleep(100);
		int soil_mv = read_adc_mv(&adc_soil);
		gpio_pin_set_dt(&soil_pwr, 0);
		int soil_12 = soil_mv * 4095 / 3600;
		int soil_pct = CLAMP((SOIL_DRY - soil_12) * 100 / (SOIL_DRY - SOIL_WET), 0, 100);

		float t_f   = temp.val1 + temp.val2 / 1000000.0f;
		float h_f   = hum.val1  + hum.val2  / 1000000.0f;
		float lux_f = lux.val1  + lux.val2  / 1000000.0f;

		LOG_INF("T: %.2f C  RH: %.2f %%  Lux: %.2f  Sol: %d%%  Bat: %dmV (%d%%)",
			(double)t_f, (double)h_f, (double)lux_f,
			soil_pct, batt_mv, batt_pct);

		/* BTHome : met à jour le payload en live (advertising continu) */
		bthome_update(batt_pct, t_f, h_f, lux_f, soil_pct);
		err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
		if (err && err != -EAGAIN) {
			LOG_ERR("bt_le_adv_update_data failed: %d", err);
		} else {
			/* Clignotement LED : 2 flashs rapides = advertising OK */
			gpio_pin_set_dt(&user_led, 0); k_msleep(80);
			gpio_pin_set_dt(&user_led, 1); k_msleep(80);
			gpio_pin_set_dt(&user_led, 0); k_msleep(80);
			gpio_pin_set_dt(&user_led, 1);
			LOG_DBG("BTHome advertised");
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
