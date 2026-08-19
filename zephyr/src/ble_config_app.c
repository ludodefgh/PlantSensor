/*
 * Mode config : service GATT custom pour une app compagnon (telephone).
 * Expose la calibration sol + l'intervalle de report en lecture-ecriture
 * dans un "brouillon" (staging_*), et un caractere "commit" qui valide,
 * persiste en NVS (device_config.c) et reboote dans le mode radio choisi
 * (protocol_mode.c). Rien n'est applique tant que "commit" n'est pas
 * ecrit — la connexion peut se couper sans effet, comme annuler.
 */

#include "ble_config_app.h"

#include <string.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "device_config.h"
#include "protocol_mode.h"

LOG_MODULE_REGISTER(myco_config, LOG_LEVEL_INF);

/* UUID custom (pas de collision connue — projet personnel, pas de
 * registration formelle necessaire). Base 59ad0000-1212-4a2d-8b1e-2c9f4a5d0000,
 * dernier bloc incremente par caracteristique. */
#define BT_UUID_MYCO_CFG_SVC_VAL \
	BT_UUID_128_ENCODE(0x59ad0000, 0x1212, 0x4a2d, 0x8b1e, 0x2c9f4a5d0001)
#define BT_UUID_MYCO_CFG_SOIL_DRY_VAL \
	BT_UUID_128_ENCODE(0x59ad0000, 0x1212, 0x4a2d, 0x8b1e, 0x2c9f4a5d0002)
#define BT_UUID_MYCO_CFG_SOIL_WET_VAL \
	BT_UUID_128_ENCODE(0x59ad0000, 0x1212, 0x4a2d, 0x8b1e, 0x2c9f4a5d0003)
#define BT_UUID_MYCO_CFG_INTERVAL_VAL \
	BT_UUID_128_ENCODE(0x59ad0000, 0x1212, 0x4a2d, 0x8b1e, 0x2c9f4a5d0004)
#define BT_UUID_MYCO_CFG_TARGET_MODE_VAL \
	BT_UUID_128_ENCODE(0x59ad0000, 0x1212, 0x4a2d, 0x8b1e, 0x2c9f4a5d0005)
#define BT_UUID_MYCO_CFG_COMMIT_VAL \
	BT_UUID_128_ENCODE(0x59ad0000, 0x1212, 0x4a2d, 0x8b1e, 0x2c9f4a5d0006)

static const struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(BT_UUID_MYCO_CFG_SVC_VAL);
static const struct bt_uuid_128 soil_dry_uuid = BT_UUID_INIT_128(BT_UUID_MYCO_CFG_SOIL_DRY_VAL);
static const struct bt_uuid_128 soil_wet_uuid = BT_UUID_INIT_128(BT_UUID_MYCO_CFG_SOIL_WET_VAL);
static const struct bt_uuid_128 interval_uuid = BT_UUID_INIT_128(BT_UUID_MYCO_CFG_INTERVAL_VAL);
static const struct bt_uuid_128 target_mode_uuid =
	BT_UUID_INIT_128(BT_UUID_MYCO_CFG_TARGET_MODE_VAL);
static const struct bt_uuid_128 commit_uuid = BT_UUID_INIT_128(BT_UUID_MYCO_CFG_COMMIT_VAL);

/* Brouillon modifie par l'app compagnon ; applique seulement au commit. */
static struct device_config staging_cfg;
static uint8_t staging_target_mode = PROTOCOL_MODE_BLE;

static ssize_t read_u16(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, uint16_t len, uint16_t offset)
{
	const uint16_t *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
}

static ssize_t write_u16(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(flags);
	uint16_t *value = attr->user_data;

	if (offset != 0 || len != sizeof(*value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	memcpy(value, buf, len);
	return len;
}

static ssize_t read_u8(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	const uint8_t *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
}

static ssize_t write_target_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(flags);
	uint8_t *value = attr->user_data;

	if (offset != 0 || len != sizeof(*value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t v = *(const uint8_t *)buf;

	if (v != PROTOCOL_MODE_BLE && v != PROTOCOL_MODE_ZIGBEE) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	*value = v;
	return len;
}

static ssize_t write_commit(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(buf);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	if (staging_cfg.soil_dry <= staging_cfg.soil_wet) {
		LOG_ERR("Commit refuse : soil_dry (%u) doit etre > soil_wet (%u)",
			staging_cfg.soil_dry, staging_cfg.soil_wet);
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	if (staging_cfg.report_interval_s == 0) {
		LOG_ERR("Commit refuse : intervalle nul");
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	device_config_set(&staging_cfg);
	protocol_mode_set((enum protocol_mode)staging_target_mode);

	LOG_INF("Config committee (sec=%u humide=%u intervalle=%us), reboot en mode %s",
		staging_cfg.soil_dry, staging_cfg.soil_wet, staging_cfg.report_interval_s,
		protocol_mode_name((enum protocol_mode)staging_target_mode));

	/* Ecriture "without response" — la connexion tombe au reboot, c'est
	 * le signal attendu par l'app plutot qu'un ACK GATT. */
	k_msleep(200);
	sys_reboot(SYS_REBOOT_COLD);

	return len;
}

BT_GATT_SERVICE_DEFINE(myco_cfg_svc,
	BT_GATT_PRIMARY_SERVICE(&svc_uuid),
	BT_GATT_CHARACTERISTIC(&soil_dry_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_u16, write_u16, &staging_cfg.soil_dry),
	BT_GATT_CHARACTERISTIC(&soil_wet_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_u16, write_u16, &staging_cfg.soil_wet),
	BT_GATT_CHARACTERISTIC(&interval_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_u16, write_u16, &staging_cfg.report_interval_s),
	BT_GATT_CHARACTERISTIC(&target_mode_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
		read_u8, write_target_mode, &staging_target_mode),
	BT_GATT_CHARACTERISTIC(&commit_uuid.uuid,
		BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL, write_commit, NULL),
);

/* Flags + UUID128 dans l'ADV (21 octets), nom dans la scan response (13
 * octets) — les deux tiennent chacun sous la limite de 31 octets. */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_UUID128_ALL, ((uint8_t[]){BT_UUID_MYCO_CFG_SVC_VAL}), 16),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, "Myco-Config", 11),
};

static const struct bt_le_adv_param adv_param =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2,
			      BT_GAP_ADV_FAST_INT_MAX_2, NULL);

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_WRN("Connexion echouee (err %u)", err);
		return;
	}
	LOG_INF("App compagnon connectee");
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	LOG_INF("App compagnon deconnectee (raison %u)", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = on_connected,
	.disconnected = on_disconnected,
};

int ble_config_app_run(void)
{
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable failed: %d", err);
		return -1;
	}

	staging_cfg = *device_config_get();
	staging_target_mode = PROTOCOL_MODE_BLE;

	err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("bt_le_adv_start failed: %d", err);
		return -1;
	}

	LOG_INF("Mode config : GATT 'Myco-Config' actif, en attente d'une connexion");
	LOG_INF("Config actuelle : sec=%u humide=%u intervalle=%us",
		staging_cfg.soil_dry, staging_cfg.soil_wet, staging_cfg.report_interval_s);

	/* Heartbeat periodique — comme ble_app.c/zigbee_app.c, utile pour
	 * confirmer via RTT que le mode config tourne toujours meme sans
	 * connexion (rien d'autre ne boucle dans ce mode). */
	while (1) {
		k_sleep(K_SECONDS(10));
		LOG_INF("Mode config actif, en attente...");
	}

	return 0;
}
