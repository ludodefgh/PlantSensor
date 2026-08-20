# Converter externe zigbee2mqtt — config Myco

`myco_config.mjs` expose le cluster de config custom (0x0409) du capteur
comme des entités éditables dans Home Assistant : `soil_dry`, `soil_wet`,
`report_interval`, `target_mode` (ble/zigbee/config), et `commit` (écrire
n'importe quoi dedans persiste tout et redémarre le capteur dans le mode
choisi — y compris `target_mode: config` pour rouvrir le mode config BLE
sans toucher physiquement au capteur).

## Installation

1. Copier `myco_config.mjs` dans le dossier `external_converters/` de
   zigbee2mqtt (créer le dossier s'il n'existe pas, au même niveau que
   `configuration.yaml`).
2. Dans `configuration.yaml`, vérifier `enable_external_js: true`
   (désactivé par défaut depuis z2m 2.11+).
3. Redémarrer zigbee2mqtt.

## ⚠️ Point à vérifier : l'identité du device

Le converter matche le device via `zigbeeModel: ['Myco Mini']` — **c'est
une supposition, pas une valeur confirmée**. Le firmware actuel ne
renseigne pas explicitement le Manufacturer Name / Model Identifier du
cluster Basic (`zephyr/src/zigbee_app.c`, `ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST`
sans les champs manuf/model) — donc ce que le device paire montre
réellement à z2m est probablement une valeur par défaut de la stack ZBOSS,
pas "Myco Mini".

**Pour vérifier** : dans l'UI z2m, ouvrir la page du device → onglet
"Dev console" (ou dans `data/database.db` / le topic MQTT
`zigbee2mqtt/bridge/devices`) et regarder les champs `modelID` /
`manufacturerName` actuels. Si le converter ne s'attache pas (le device
reste "unsupported" ou les nouvelles entités n'apparaissent pas après
redémarrage), soit :
- ajuster `zigbeeModel`/`vendor`/`model` dans `myco_config.mjs` pour
  matcher exactement ce que le device montre déjà, ou
- (option plus propre à terme) ajouter les attributs Manufacturer
  Name/Model Identifier au cluster Basic côté firmware — mais ça
  nécessite de re-pairer le device dans z2m (remove + re-add) pour que la
  nouvelle identité soit relue, l'ancienne étant mise en cache à
  l'interview initial.

Dites-moi ce que `modelID`/`manufacturerName` affiche réellement et
j'ajuste le fichier sans avoir besoin d'un nouveau flash.
