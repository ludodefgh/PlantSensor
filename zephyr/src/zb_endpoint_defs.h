/*
 * Déclaration bas niveau de l'endpoint Myco (clusters custom au-delà de
 * ce que fournissent les macros ZB_HA_DECLARE_*_DEVICE — celles-ci ne
 * couvrent qu'un seul "device type" HA à la fois, alors qu'on combine
 * Temperature + Humidity + Illuminance + Power Config + Soil Moisture
 * (custom) sur un seul endpoint). Pattern repris du projet b-parasite
 * (github.com/rbaron/b-parasite), qui fait exactement ça pour un capteur
 * de plante Zigbee.
 */

#ifndef ZB_ENDPOINT_DEFS_H
#define ZB_ENDPOINT_DEFS_H

#include "zb_myco_config_defs.h"
#include "zb_soil_moisture_defs.h"

#define MYCO_ZIGBEE_ENDPOINT 10
#define MYCO_BASIC_MANUF_NAME "Myco"

#define MYCO_ZB_DEVICE_ID 0x0008
#define MYCO_ZB_DEVICE_VERSION 0
#define MYCO_ZB_IN_CLUSTER_NUM 8
#define MYCO_ZB_OUT_CLUSTER_NUM 0
#define MYCO_ZB_CLUSTER_NUM (MYCO_ZB_IN_CLUSTER_NUM + MYCO_ZB_OUT_CLUSTER_NUM)
#define MYCO_ZB_ATTR_REPORTING_COUNT 5

#define MYCO_ZB_DECLARE_CLUSTER_LIST(                                       \
	cluster_list_name,                                                   \
	basic_attr_list,                                                     \
	identify_attr_list,                                                  \
	temp_measurement_attr_list,                                          \
	rel_humidity_attr_list,                                              \
	batt_attr_list,                                                      \
	soil_moisture_attr_list,                                             \
	illuminance_attr_list,                                               \
	config_attr_list)                                                    \
	zb_zcl_cluster_desc_t cluster_list_name[] =                          \
	{                                                                     \
		ZB_ZCL_CLUSTER_DESC(                                         \
			ZB_ZCL_CLUSTER_ID_IDENTIFY,                          \
			ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t), \
			(identify_attr_list),                                \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                          \
			ZB_ZCL_MANUF_CODE_INVALID),                          \
		ZB_ZCL_CLUSTER_DESC(                                         \
			ZB_ZCL_CLUSTER_ID_BASIC,                             \
			ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t),    \
			(basic_attr_list),                                   \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                          \
			ZB_ZCL_MANUF_CODE_INVALID),                          \
		ZB_ZCL_CLUSTER_DESC(                                         \
			ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,                  \
			ZB_ZCL_ARRAY_SIZE(temp_measurement_attr_list, zb_zcl_attr_t), \
			(temp_measurement_attr_list),                        \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                          \
			ZB_ZCL_MANUF_CODE_INVALID),                          \
		ZB_ZCL_CLUSTER_DESC(                                         \
			ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,          \
			ZB_ZCL_ARRAY_SIZE(rel_humidity_attr_list, zb_zcl_attr_t), \
			(rel_humidity_attr_list),                            \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                          \
			ZB_ZCL_MANUF_CODE_INVALID),                          \
		ZB_ZCL_CLUSTER_DESC(                                         \
			MYCO_ZB_ZCL_SOIL_MOISTURE_CLUSTER_ID,                \
			ZB_ZCL_ARRAY_SIZE(soil_moisture_attr_list, zb_zcl_attr_t), \
			(soil_moisture_attr_list),                           \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                          \
			ZB_ZCL_MANUF_CODE_INVALID),                          \
		ZB_ZCL_CLUSTER_DESC(                                         \
			ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT,           \
			ZB_ZCL_ARRAY_SIZE(illuminance_attr_list, zb_zcl_attr_t), \
			(illuminance_attr_list),                             \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                          \
			ZB_ZCL_MANUF_CODE_INVALID),                          \
		ZB_ZCL_CLUSTER_DESC(                                         \
			ZB_ZCL_CLUSTER_ID_POWER_CONFIG,                      \
			ZB_ZCL_ARRAY_SIZE(batt_attr_list, zb_zcl_attr_t),    \
			(batt_attr_list),                                    \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                          \
			ZB_ZCL_MANUF_CODE_INVALID),                          \
		ZB_ZCL_CLUSTER_DESC(                                         \
			MYCO_ZB_ZCL_CONFIG_CLUSTER_ID,                       \
			ZB_ZCL_ARRAY_SIZE(config_attr_list, zb_zcl_attr_t),  \
			(config_attr_list),                                  \
			ZB_ZCL_CLUSTER_SERVER_ROLE,                          \
			ZB_ZCL_MANUF_CODE_INVALID)                           \
	}

#define MYCO_ZB_DECLARE_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num) \
	ZB_DECLARE_SIMPLE_DESC(in_clust_num, out_clust_num);                 \
	ZB_AF_SIMPLE_DESC_TYPE(in_clust_num, out_clust_num)                  \
	simple_desc_##ep_name =                                              \
	{                                                                     \
		ep_id,                                                       \
		ZB_AF_HA_PROFILE_ID,                                         \
		MYCO_ZB_DEVICE_ID,                                           \
		MYCO_ZB_DEVICE_VERSION,                                      \
		0,                                                            \
		in_clust_num,                                                \
		out_clust_num,                                               \
		{                                                             \
			ZB_ZCL_CLUSTER_ID_BASIC,                             \
			ZB_ZCL_CLUSTER_ID_IDENTIFY,                          \
			ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,                  \
			ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,          \
			MYCO_ZB_ZCL_SOIL_MOISTURE_CLUSTER_ID,                \
			ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT,           \
			ZB_ZCL_CLUSTER_ID_POWER_CONFIG,                      \
			MYCO_ZB_ZCL_CONFIG_CLUSTER_ID,                       \
		}                                                             \
	}

#define MYCO_ZB_DECLARE_ENDPOINT(ep_name, ep_id, cluster_list)                                      \
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_ctx_##ep_name, MYCO_ZB_ATTR_REPORTING_COUNT);  \
	MYCO_ZB_DECLARE_SIMPLE_DESC(ep_name, ep_id,                                                  \
				    MYCO_ZB_IN_CLUSTER_NUM, MYCO_ZB_OUT_CLUSTER_NUM);               \
	ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID,                             \
				    /*reserved_length=*/0, /*reserved_ptr=*/NULL,                   \
				    ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list, \
				    (zb_af_simple_desc_1_1_t *)&simple_desc_##ep_name,              \
				    MYCO_ZB_ATTR_REPORTING_COUNT, reporting_ctx_##ep_name,          \
				    /*lev_ctrl_count=*/0, /*lev_ctrl_ctx=*/NULL)

#endif /* ZB_ENDPOINT_DEFS_H */
