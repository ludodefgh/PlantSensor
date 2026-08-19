# Myco Config — app compagnon Android

App Android native (Kotlin + Jetpack Compose, BLE natif — pas de
dépendance BLE tierce) pour configurer le capteur Myco : calibration sol
et intervalle de report, plus choix du protocole radio (BLE ou Zigbee) au
redémarrage. Elle parle au service GATT custom exposé par le firmware
quand il est en **mode config** (`ble_config_app.c` sur la branche
`feature/an54lq15-dual-protocol`).

## Prérequis

- Android Studio (Koala ou plus récent) — il télécharge lui-même le SDK,
  Gradle et le plugin Android au premier lancement du projet.
- Un téléphone Android 12+ (minSdk 31 — voir plus bas pourquoi).
- Le capteur flashé avec le firmware dual-protocol, en mode config.

Ce projet n'a pas été compilé dans l'environnement où il a été écrit (pas
de SDK Android disponible côté machine de dev) — ouvrir dans Android
Studio pour le premier build, il gère l'installation des composants
manquants automatiquement.

## Passer le capteur en mode config

Pas encore de bouton physique — pour l'instant, via le shell RTT
(`protocol_mode.c`) :

```
protocol set config
protocol reboot
```

Le capteur redémarre, arrête son advertising BTHome/Zigbee habituel, et
annonce à la place un périphérique BLE connectable nommé **"Myco-Config"**
exposant le service ci-dessous.

## Utilisation de l'app

1. Lancer l'app, autoriser les permissions Bluetooth demandées.
2. "Rechercher le capteur" — scan filtré sur l'UUID du service, connexion
   automatique au premier trouvé.
3. Les valeurs actuelles (calibration sol, intervalle) se chargent dans
   les champs.
4. Modifier ce qu'on veut, choisir le mode radio cible (BLE ou Zigbee).
5. "Appliquer et redémarrer" — écrit les 4 caractéristiques puis le
   "commit". Le firmware persiste tout en NVS, choisit le nouveau
   `protocol_mode`, et redémarre : **la connexion BLE tombe à ce
   moment-là, c'est normal**, pas une erreur. L'app affiche un message de
   succès plutôt que de traiter la déconnexion comme un échec.

Rien n'est appliqué avant le "commit" — fermer l'app ou perdre la
connexion avant d'appuyer dessus n'a aucun effet sur le capteur.

## Service GATT (doit rester synchronisé avec `ble_config_app.c`)

UUID de base custom `59ad0000-1212-4a2d-8b1e-2c9f4a5d00XX` (projet perso,
pas de UUID SIG officiel) :

| Caractéristique | UUID (...XX) | Type | Lecture/Écriture |
|---|---|---|---|
| Service | 0001 | — | — |
| Sol sec (ADC brut) | 0002 | uint16 LE | R/W |
| Sol humide (ADC brut) | 0003 | uint16 LE | R/W |
| Intervalle de report (s) | 0004 | uint16 LE | R/W |
| Mode cible (0=BLE, 1=Zigbee) | 0005 | uint8 | R/W |
| Commit (persiste + reboot) | 0006 | — (n'importe quel octet) | Write without response |

`MycoGatt.kt` centralise ces constantes côté app.

## Pourquoi minSdk 31

Android 12 (API 31) a introduit `BLUETOOTH_SCAN`/`BLUETOOTH_CONNECT` à la
place de `BLUETOOTH`/`BLUETOOTH_ADMIN` + `ACCESS_FINE_LOCATION`. En
déclarant `usesPermissionFlags="neverForLocation"` sur `BLUETOOTH_SCAN`
(l'app ne dérive jamais de position depuis le scan), on évite complètement
la permission de localisation. Ça simplifie le code de permissions au prix
de ne pas supporter Android 11 et antérieur — à revoir si besoin un jour
de supporter de vieux téléphones.

## Alternative sans app

`nRF Connect for Mobile` (Nordic, gratuite, Android et iOS) peut parler à
ce même service GATT sans rien installer de custom — utile pour débugger
le firmware indépendamment de l'app, ou en attendant qu'elle soit prête.
