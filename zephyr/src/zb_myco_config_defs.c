#include "zb_myco_config_defs.h"

#include <zboss_api.h>

void myco_zcl_config_init_server(void)
{
	zb_zcl_add_cluster_handlers(MYCO_ZB_ZCL_CONFIG_CLUSTER_ID,
				     ZB_ZCL_CLUSTER_SERVER_ROLE,
				     /*cluster_check_value=*/NULL,
				     /*cluster_write_attr_hook=*/NULL,
				     /*cluster_handler=*/NULL);
}

void myco_zcl_config_init_client(void)
{
	/* Rien : pas de role client sur ce cluster. */
}
