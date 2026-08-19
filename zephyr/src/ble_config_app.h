#ifndef BLE_CONFIG_APP_H
#define BLE_CONFIG_APP_H

/* Mode config : GATT peripheral connectable pour une app compagnon
 * (telephone). Ne fait pas de reporting capteur — expose la calibration
 * sol/intervalle en lecture-ecriture, plus un "commit" qui persiste tout
 * et reboote dans le mode radio choisi. Retourne apres avoir demarre
 * l'advertising ; tout le reste se passe dans les callbacks GATT. */
int ble_config_app_run(void);

#endif /* BLE_CONFIG_APP_H */
