// Converter externe zigbee2mqtt pour le cluster de config custom du
// capteur Myco (cluster 0x0409 / 1033, cf. zephyr/src/zb_myco_config_defs.h
// sur la branche feature/an54lq15-dual-protocol). Expose la calibration
// sol, l'intervalle de report, et le mode radio cible comme des entites
// modifiables dans Home Assistant ; ecrire dans "commit" declenche la
// persistance (device_config.c / protocol_mode.c) et un redemarrage dans
// le mode choisi cote firmware — y compris vers le mode config BLE
// (target_mode = "config"), pour ouvrir l'app compagnon telephone sans
// avoir a toucher physiquement au capteur.
//
// Installation :
//   1. Copier ce fichier dans le dossier `external_converters/` de
//      zigbee2mqtt (au meme niveau que configuration.yaml).
//   2. Dans configuration.yaml, s'assurer que `enable_external_js: true`
//      (desactive par defaut depuis z2m 2.11).
//   3. Redemarrer zigbee2mqtt.
//   4. Verifier que `zigbeeModel`/`vendor`/`model` ci-dessous matchent
//      bien ce que le device paire montre deja dans z2m (Basic cluster,
//      Model Identifier / Manufacturer Name) — le firmware ne les
//      renseigne pas explicitement pour l'instant, donc ca peut necessiter
//      un ajustement ou un re-pairing (remove + re-add dans z2m) pour que
//      le converter s'attache correctement au device deja connu.

import {numeric, enumLookup} from 'zigbee-herdsman-converters/lib/modernExtend';

const CONFIG_CLUSTER = 0x0409;

const UINT8 = 0x20;
const UINT16 = 0x21;

export default {
    zigbeeModel: ['Myco Mini'],
    model: 'myco-mini-sensor',
    vendor: 'Myco',
    description: 'Capteur de plante Myco — calibration et mode radio via Zigbee',
    extend: [
        numeric({
            name: 'soil_dry',
            cluster: CONFIG_CLUSTER,
            attribute: {ID: 0x0000, type: UINT16},
            description: 'Valeur ADC brute correspondant au sol sec (calibration)',
            valueMin: 0,
            valueMax: 4095,
            access: 'ALL',
            reporting: false,
            entityCategory: 'config',
        }),
        numeric({
            name: 'soil_wet',
            cluster: CONFIG_CLUSTER,
            attribute: {ID: 0x0001, type: UINT16},
            description: 'Valeur ADC brute correspondant au sol humide (calibration)',
            valueMin: 0,
            valueMax: 4095,
            access: 'ALL',
            reporting: false,
            entityCategory: 'config',
        }),
        numeric({
            name: 'report_interval',
            cluster: CONFIG_CLUSTER,
            attribute: {ID: 0x0002, type: UINT16},
            description: "Intervalle de report des capteurs, en secondes",
            unit: 's',
            valueMin: 1,
            valueMax: 3600,
            access: 'ALL',
            reporting: false,
            entityCategory: 'config',
        }),
        enumLookup({
            name: 'target_mode',
            cluster: CONFIG_CLUSTER,
            attribute: {ID: 0x0003, type: UINT8},
            description: 'Mode radio applique au prochain "commit" (redemarre le capteur)',
            lookup: {ble: 0, zigbee: 1, config: 2},
            access: 'ALL',
            reporting: false,
            entityCategory: 'config',
        }),
        numeric({
            name: 'commit',
            cluster: CONFIG_CLUSTER,
            attribute: {ID: 0x0004, type: UINT8},
            description:
                'Ecrire n\'importe quelle valeur ici pour persister soil_dry/soil_wet/' +
                'report_interval/target_mode et redemarrer le capteur dans le mode choisi. ' +
                'La connexion Zigbee tombe brievement pendant le reboot, c\'est normal.',
            valueMin: 0,
            valueMax: 1,
            access: 'SET',
            reporting: false,
            entityCategory: 'config',
        }),
    ],
};
