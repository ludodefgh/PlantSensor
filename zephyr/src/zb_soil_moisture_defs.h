/*
 * Cluster Zigbee "custom" pour l'humidité du sol — pas de cluster ZCL
 * standard pour ça. ID et layout repris du projet b-parasite
 * (github.com/rbaron/b-parasite), qui matche la convention msSoilMoisture
 * de zigbee-herdsman (zigbee2mqtt) — donc reconnu sans quirk/converter
 * custom côté zigbee2mqtt. Valeur en % avec 2 décimales (unités de 0.01%),
 * même convention que le cluster Relative Humidity standard.
 */

#ifndef ZB_SOIL_MOISTURE_DEFS_H
#define ZB_SOIL_MOISTURE_DEFS_H

#include <zboss_api.h>
#include <zcl/zb_zcl_common.h>

#define MYCO_ZB_ZCL_SOIL_MOISTURE_CLUSTER_ID 1032  /* 0x0408, msSoilMoisture */

#define MYCO_ZB_ZCL_ATTR_SOIL_MOISTURE_VALUE_ID 0x00  /* uint16 */

void myco_zcl_soil_moisture_init_server(void);
void myco_zcl_soil_moisture_init_client(void);

#define MYCO_ZB_ZCL_SOIL_MOISTURE_CLUSTER_ID_SERVER_ROLE_INIT myco_zcl_soil_moisture_init_server
#define MYCO_ZB_ZCL_SOIL_MOISTURE_CLUSTER_ID_CLIENT_ROLE_INIT myco_zcl_soil_moisture_init_client

#define MYCO_ZB_ZCL_SOIL_MOISTURE_CLUSTER_REVISION_DEFAULT ((zb_uint16_t)1)

#define ZB_SET_ATTR_DESCR_WITH_MYCO_ZB_ZCL_ATTR_SOIL_MOISTURE_VALUE_ID(data_ptr) \
	{                                                                     \
		MYCO_ZB_ZCL_ATTR_SOIL_MOISTURE_VALUE_ID,                     \
		ZB_ZCL_ATTR_TYPE_U16,                                        \
		ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_REPORTING, \
		(ZB_ZCL_NON_MANUFACTURER_SPECIFIC),                          \
		(void *)data_ptr                                             \
	}

#define MYCO_ZB_ZCL_DECLARE_SOIL_MOISTURE_ATTRIB_LIST(attr_list, value)                  \
	ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, MYCO_ZB_ZCL_SOIL_MOISTURE) \
	ZB_ZCL_SET_ATTR_DESC(MYCO_ZB_ZCL_ATTR_SOIL_MOISTURE_VALUE_ID, (value))            \
	ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

#endif /* ZB_SOIL_MOISTURE_DEFS_H */
