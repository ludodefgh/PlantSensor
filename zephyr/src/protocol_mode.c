#include "protocol_mode.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(protocol_mode, LOG_LEVEL_INF);

#define SETTINGS_SUBTREE "myco"
#define SETTINGS_KEY     "myco/mode"

static enum protocol_mode current_mode = PROTOCOL_MODE_DEFAULT;
static bool loaded;

static int mode_set_cb(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	ARG_UNUSED(name);

	if (len != sizeof(current_mode)) {
		return -EINVAL;
	}

	ssize_t ret = read_cb(cb_arg, &current_mode, sizeof(current_mode));

	return ret < 0 ? (int)ret : 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(myco_mode, SETTINGS_SUBTREE, NULL, mode_set_cb, NULL, NULL);

enum protocol_mode protocol_mode_get(void)
{
	if (!loaded) {
		settings_subsys_init();
		settings_load();
		loaded = true;
		LOG_INF("Mode protocole charge depuis NVS : %s", protocol_mode_name(current_mode));
	}

	return current_mode;
}

int protocol_mode_set(enum protocol_mode mode)
{
	current_mode = mode;
	loaded = true;

	return settings_save_one(SETTINGS_KEY, &mode, sizeof(mode));
}

const char *protocol_mode_name(enum protocol_mode mode)
{
	return mode == PROTOCOL_MODE_ZIGBEE ? "zigbee" : "ble";
}

/* ── Commande shell (RTT) : selectionner le protocole sans reflasher ─────
 * "protocol set <ble|zigbee>" puis "protocol reboot" (ou tout reboot) pour
 * appliquer — un futur jumper/bouton ou une UI ecriraient le meme flag via
 * protocol_mode_set(). ──────────────────────────────────────────────── */

static int cmd_protocol_show(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Protocole actuel : %s", protocol_mode_name(protocol_mode_get()));
	return 0;
}

static int cmd_protocol_set(const struct shell *sh, size_t argc, char **argv)
{
	enum protocol_mode mode;

	if (argc != 2) {
		shell_error(sh, "Usage: protocol set <ble|zigbee>");
		return -EINVAL;
	}

	if (strcmp(argv[1], "ble") == 0) {
		mode = PROTOCOL_MODE_BLE;
	} else if (strcmp(argv[1], "zigbee") == 0) {
		mode = PROTOCOL_MODE_ZIGBEE;
	} else {
		shell_error(sh, "Mode inconnu : %s (ble ou zigbee)", argv[1]);
		return -EINVAL;
	}

	int err = protocol_mode_set(mode);

	if (err) {
		shell_error(sh, "Echec sauvegarde NVS : %d", err);
		return err;
	}

	shell_print(sh, "Mode enregistre : %s. Lance 'protocol reboot' pour appliquer.",
		    protocol_mode_name(mode));
	return 0;
}

static int cmd_protocol_reboot(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Reboot...");
	k_msleep(200); /* laisse le temps au shell d'afficher le message avant reset */
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(protocol_cmds,
	SHELL_CMD(show, NULL, "Affiche le protocole actuellement selectionne", cmd_protocol_show),
	SHELL_CMD(set, NULL, "Choisit le protocole (ble|zigbee) — effectif apres reboot",
		  cmd_protocol_set),
	SHELL_CMD(reboot, NULL, "Redemarre pour appliquer le protocole selectionne",
		  cmd_protocol_reboot),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(protocol, &protocol_cmds, "Gestion du protocole radio (BLE/Zigbee)", NULL);
