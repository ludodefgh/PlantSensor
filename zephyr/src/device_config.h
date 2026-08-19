/*
 * Parametres capteur ajustables (calibration sol, intervalle de report),
 * persistes en NVS. Ecrits par ble_config_app.c (mode config, via l'app
 * compagnon), lus une seule fois au boot par ble_app.c / zigbee_app.c —
 * pas de rechargement a chaud, il faut repasser par le mode config +
 * "commit" (qui reboote) pour appliquer un changement.
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>

struct device_config {
	uint16_t soil_dry;
	uint16_t soil_wet;
	uint16_t report_interval_s;
};

#define DEVICE_CONFIG_DEFAULT \
	{ .soil_dry = 3500, .soil_wet = 1260, .report_interval_s = 10 }

/* Charge la config persistee (defauts ci-dessus si jamais ecrite). A
 * appeler avant de lire les valeurs — sans effet si deja chargee. */
const struct device_config *device_config_get(void);

/* Ecrit la config en NVS. */
int device_config_set(const struct device_config *cfg);

#endif /* DEVICE_CONFIG_H */
