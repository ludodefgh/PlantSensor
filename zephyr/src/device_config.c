#include "device_config.h"

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(device_config, LOG_LEVEL_INF);

/* Sous-arbre distinct de celui de protocol_mode.c ("myco") — Settings
 * n'autorise qu'un seul handler par nom de sous-arbre. */
#define SETTINGS_SUBTREE "mycocfg"
#define SETTINGS_KEY     "mycocfg/data"

static struct device_config current_cfg = DEVICE_CONFIG_DEFAULT;
static bool loaded;

static int cfg_set_cb(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	ARG_UNUSED(name);

	if (len != sizeof(current_cfg)) {
		return -EINVAL;
	}

	ssize_t ret = read_cb(cb_arg, &current_cfg, sizeof(current_cfg));

	return ret < 0 ? (int)ret : 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(myco_cfg, SETTINGS_SUBTREE, NULL, cfg_set_cb, NULL, NULL);

const struct device_config *device_config_get(void)
{
	if (!loaded) {
		settings_subsys_init();
		settings_load();
		loaded = true;
		LOG_INF("Config chargee depuis NVS : sec=%u humide=%u intervalle=%us",
			current_cfg.soil_dry, current_cfg.soil_wet, current_cfg.report_interval_s);
	}

	return &current_cfg;
}

int device_config_set(const struct device_config *cfg)
{
	current_cfg = *cfg;
	loaded = true;

	return settings_save_one(SETTINGS_KEY, &current_cfg, sizeof(current_cfg));
}
