/*
 * Cluster Zigbee "custom" pour la configuration du capteur (calibration
 * sol, intervalle de report, mode radio cible) — pas de cluster ZCL
 * standard pour ça, et pas de convention zigbee2mqtt existante non plus
 * (contrairement au cluster Soil Moisture, qui matche msSoilMoisture) :
 * necessite un converter externe cote zigbee2mqtt pour apparaitre comme
 * des controles dans Home Assistant (cf. companion-app/README.md).
 *
 * Meme mecanisme "brouillon + commit" que le mode config BLE
 * (ble_config_app.c) : ecrire les attributs souhaites, puis ecrire
 * n'importe quelle valeur dans "commit" pour persister (device_config.c /
 * protocol_mode.c) et redemarrer dans le mode choisi — y compris vers le
 * mode config BLE (PROTOCOL_MODE_BLE_CONFIG), pour ouvrir l'app compagnon
 * telephone sans acces physique/RTT a l'appareil.
 */

#ifndef ZB_MYCO_CONFIG_DEFS_H
#define ZB_MYCO_CONFIG_DEFS_H

#include <zboss_api.h>
#include <zcl/zb_zcl_common.h>

#define MYCO_ZB_ZCL_CONFIG_CLUSTER_ID 1033  /* 0x0409, custom */

#define MYCO_ZB_ZCL_ATTR_CONFIG_SOIL_DRY_ID    0x0000  /* uint16, ADC brut */
#define MYCO_ZB_ZCL_ATTR_CONFIG_SOIL_WET_ID    0x0001  /* uint16, ADC brut */
#define MYCO_ZB_ZCL_ATTR_CONFIG_INTERVAL_ID    0x0002  /* uint16, secondes */
#define MYCO_ZB_ZCL_ATTR_CONFIG_TARGET_MODE_ID 0x0003  /* uint8 : 0=BLE 1=Zigbee 2=config BLE */
#define MYCO_ZB_ZCL_ATTR_CONFIG_COMMIT_ID      0x0004  /* uint8, ecrire pour declencher */

void myco_zcl_config_init_server(void);
void myco_zcl_config_init_client(void);

#define MYCO_ZB_ZCL_CONFIG_CLUSTER_ID_SERVER_ROLE_INIT myco_zcl_config_init_server
#define MYCO_ZB_ZCL_CONFIG_CLUSTER_ID_CLIENT_ROLE_INIT myco_zcl_config_init_client

#define MYCO_ZB_ZCL_CONFIG_CLUSTER_REVISION_DEFAULT ((zb_uint16_t)1)

#define ZB_SET_ATTR_DESCR_WITH_MYCO_ZB_ZCL_ATTR_CONFIG_SOIL_DRY_ID(data_ptr)    \
	{                                                                     \
		MYCO_ZB_ZCL_ATTR_CONFIG_SOIL_DRY_ID,                         \
		ZB_ZCL_ATTR_TYPE_U16,                                        \
		ZB_ZCL_ATTR_ACCESS_READ_WRITE,                               \
		(ZB_ZCL_NON_MANUFACTURER_SPECIFIC),                          \
		(void *)data_ptr                                             \
	}

#define ZB_SET_ATTR_DESCR_WITH_MYCO_ZB_ZCL_ATTR_CONFIG_SOIL_WET_ID(data_ptr)    \
	{                                                                     \
		MYCO_ZB_ZCL_ATTR_CONFIG_SOIL_WET_ID,                         \
		ZB_ZCL_ATTR_TYPE_U16,                                        \
		ZB_ZCL_ATTR_ACCESS_READ_WRITE,                               \
		(ZB_ZCL_NON_MANUFACTURER_SPECIFIC),                          \
		(void *)data_ptr                                             \
	}

#define ZB_SET_ATTR_DESCR_WITH_MYCO_ZB_ZCL_ATTR_CONFIG_INTERVAL_ID(data_ptr)    \
	{                                                                     \
		MYCO_ZB_ZCL_ATTR_CONFIG_INTERVAL_ID,                         \
		ZB_ZCL_ATTR_TYPE_U16,                                        \
		ZB_ZCL_ATTR_ACCESS_READ_WRITE,                               \
		(ZB_ZCL_NON_MANUFACTURER_SPECIFIC),                          \
		(void *)data_ptr                                             \
	}

#define ZB_SET_ATTR_DESCR_WITH_MYCO_ZB_ZCL_ATTR_CONFIG_TARGET_MODE_ID(data_ptr) \
	{                                                                     \
		MYCO_ZB_ZCL_ATTR_CONFIG_TARGET_MODE_ID,                      \
		ZB_ZCL_ATTR_TYPE_U8,                                         \
		ZB_ZCL_ATTR_ACCESS_READ_WRITE,                               \
		(ZB_ZCL_NON_MANUFACTURER_SPECIFIC),                          \
		(void *)data_ptr                                             \
	}

#define ZB_SET_ATTR_DESCR_WITH_MYCO_ZB_ZCL_ATTR_CONFIG_COMMIT_ID(data_ptr)      \
	{                                                                     \
		MYCO_ZB_ZCL_ATTR_CONFIG_COMMIT_ID,                           \
		ZB_ZCL_ATTR_TYPE_U8,                                         \
		ZB_ZCL_ATTR_ACCESS_READ_WRITE,                               \
		(ZB_ZCL_NON_MANUFACTURER_SPECIFIC),                          \
		(void *)data_ptr                                             \
	}

#define MYCO_ZB_ZCL_DECLARE_CONFIG_ATTRIB_LIST(                                            \
	attr_list, soil_dry, soil_wet, interval, target_mode, commit)               \
	ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, MYCO_ZB_ZCL_CONFIG) \
	ZB_ZCL_SET_ATTR_DESC(MYCO_ZB_ZCL_ATTR_CONFIG_SOIL_DRY_ID, (soil_dry))        \
	ZB_ZCL_SET_ATTR_DESC(MYCO_ZB_ZCL_ATTR_CONFIG_SOIL_WET_ID, (soil_wet))        \
	ZB_ZCL_SET_ATTR_DESC(MYCO_ZB_ZCL_ATTR_CONFIG_INTERVAL_ID, (interval))        \
	ZB_ZCL_SET_ATTR_DESC(MYCO_ZB_ZCL_ATTR_CONFIG_TARGET_MODE_ID, (target_mode))  \
	ZB_ZCL_SET_ATTR_DESC(MYCO_ZB_ZCL_ATTR_CONFIG_COMMIT_ID, (commit))            \
	ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

#endif /* ZB_MYCO_CONFIG_DEFS_H */
