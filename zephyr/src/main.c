/*
 * Myco Mini — firmware unique BLE + Zigbee pour le breakout AN54LQ-15. Le
 * protocole actif est choisi au boot via un flag persiste en NVS (voir
 * protocol_mode.h) — changer de protocole necessite un reboot ("reboot to
 * apply"), pas de bascule a chaud entre les deux radios. Init capteurs
 * partagee ici, puis dispatch vers ble_app_run() ou zigbee_app_run().
 */

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_app.h"
#include "ble_config_app.h"
#include "protocol_mode.h"
#include "zigbee_app.h"

LOG_MODULE_REGISTER(myco, LOG_LEVEL_INF);

/* ── Périphériques partagés (peu importe le protocole) ─────────────── */
static const struct device *sht4x  = DEVICE_DT_GET(DT_NODELABEL(sht4x));
static const struct device *bh1750 = DEVICE_DT_GET(DT_NODELABEL(bh1750));

static const struct gpio_dt_spec soil_pwr =
	GPIO_DT_SPEC_GET(DT_ALIAS(soil_pwr), gpios);

static const struct adc_dt_spec adc_soil =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static const struct adc_dt_spec adc_batt =
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

int main(void)
{
	LOG_INF("Myco booting on nRF54L15...");

	/* soil_pwr allumé en continu (pas de pulse) — le capteur a besoin de
	 * plus que 100ms pour stabiliser son oscillateur interne après power-on. */
	gpio_pin_configure_dt(&soil_pwr, GPIO_OUTPUT_ACTIVE);
	adc_channel_setup_dt(&adc_soil);
	adc_channel_setup_dt(&adc_batt);
	k_msleep(500);  /* laisse le capteur sol se stabiliser avant la 1ere lecture */

	if (!device_is_ready(sht4x) || !device_is_ready(bh1750)) {
		LOG_ERR("Sensor not ready");
		return -1;
	}

	enum protocol_mode mode = protocol_mode_get();
	LOG_INF("Mode protocole selectionne : %s (shell RTT : 'protocol show|set|reboot')",
		protocol_mode_name(mode));

	switch (mode) {
	case PROTOCOL_MODE_ZIGBEE:
		return zigbee_app_run();
	case PROTOCOL_MODE_BLE_CONFIG:
		return ble_config_app_run();
	case PROTOCOL_MODE_BLE:
	default:
		return ble_app_run();
	}
}
