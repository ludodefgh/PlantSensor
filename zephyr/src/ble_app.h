#ifndef BLE_APP_H
#define BLE_APP_H

/* Boucle BLE/BTHome v2 — capteurs deja initialises par main(), ne revient
 * jamais en usage normal (boucle infinie d'advertising). */
int ble_app_run(void);

#endif /* BLE_APP_H */
