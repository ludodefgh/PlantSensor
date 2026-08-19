#ifndef ZIGBEE_APP_H
#define ZIGBEE_APP_H

/* Demarre la stack Zigbee (ZBOSS) — capteurs deja initialises par main().
 * Le reporting periodique est pilote par un alarm ZBOSS (update_sensors_cb),
 * donc cette fonction retourne immediatement — le vrai travail se passe
 * dans zboss_signal_handler() (callback global attendu par ZBOSS). */
int zigbee_app_run(void);

#endif /* ZIGBEE_APP_H */
