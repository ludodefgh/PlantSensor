/*
 * Selection du protocole radio (BLE ou Zigbee) au boot. Le choix est
 * persiste en NVS (partition "storage") et lu une seule fois au demarrage
 * — changer de protocole necessite un reboot ("reboot to apply"), pas de
 * bascule a chaud entre les deux radios.
 */

#ifndef PROTOCOL_MODE_H
#define PROTOCOL_MODE_H

enum protocol_mode {
	PROTOCOL_MODE_BLE = 0,
	PROTOCOL_MODE_ZIGBEE = 1,
	/* GATT de configuration (app compagnon) au lieu du broadcast BTHome —
	 * pas de reporting capteur dans ce mode. */
	PROTOCOL_MODE_BLE_CONFIG = 2,
};

#define PROTOCOL_MODE_DEFAULT PROTOCOL_MODE_BLE

/* Charge le mode persiste (defaut si jamais ecrit). A appeler une fois au
 * boot, avant de decider quelle stack initialiser. */
enum protocol_mode protocol_mode_get(void);

/* Ecrit le mode en NVS. Necessite un reboot pour prendre effet. */
int protocol_mode_set(enum protocol_mode mode);

const char *protocol_mode_name(enum protocol_mode mode);

#endif /* PROTOCOL_MODE_H */
