# Converter externe zigbee2mqtt — config Myco

`myco_config.mjs` expose le cluster de config custom (0x0409) du capteur
comme des entités éditables dans Home Assistant : `soil_dry`, `soil_wet`,
`report_interval`, `target_mode` (ble/zigbee/config), et `commit` (écrire
n'importe quoi dedans persiste tout et redémarre le capteur dans le mode
choisi — y compris `target_mode: config` pour rouvrir le mode config BLE
sans toucher physiquement au capteur).

## Installation

**Le plus simple (validé) : via l'UI z2m** — Settings → Dev console →
External Converters → "Create new converter", nom `myco_config.mjs`,
coller le contenu de ce fichier dans "Code", Save. Pas besoin d'accès
filesystem à l'hôte z2m.

Sinon, à la main :

1. Copier `myco_config.mjs` dans le dossier `external_converters/` de
   zigbee2mqtt (créer le dossier s'il n'existe pas, au même niveau que
   `configuration.yaml`).
2. Dans `configuration.yaml`, vérifier `enable_external_js: true`
   (désactivé par défaut depuis z2m 2.11+).
3. Redémarrer zigbee2mqtt.

## ✅ Validé (2026-08-20)

Installé via l'UI z2m + device re-pairé → toutes les entités
(`soil_dry`, `soil_wet`, `report_interval`, `target_mode`, `commit`)
apparaissent dans l'onglet "Exposes" de z2m. Reste non confirmé : le
chemin d'écriture complet (modifier une valeur + écrire `commit` →
persist + reboot reçu côté device).

## ⚠️ Re-pairing nécessaire

Le firmware renseigne maintenant Manufacturer Name = "Myco" et Model
Identifier = "Myco Mini" sur le cluster Basic (avant ça, le device
apparaissait "Unknown" dans z2m — confirmé et corrigé). Le converter
matche via `zigbeeModel: ['Myco Mini']`, ce qui correspond maintenant à
ce que le firmware envoie réellement.

**Mais** : z2m met en cache l'identité Basic à l'interview initial — le
device déjà paire garde son ancienne identité "Unknown" tant qu'il n'est
pas ré-interviewé. Il faut **supprimer puis re-ajouter le device dans
z2m** (pas juste redémarrer z2m) pour que la nouvelle identité soit lue
et que ce converter s'attache.
