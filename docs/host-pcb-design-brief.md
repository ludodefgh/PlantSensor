# Myco Mini — Brief de design KiCad : PCB Host (Proto de validation)

> Ce document est destiné à Claude Code pour réaliser le schématique et le PCB dans KiCad.
> Contexte projet complet disponible dans le repo `ludodefgh/PlantSensor` (docs canoniques :
> `myco-hardware-design.md`, `myco-decision-log.md`, `myco-roadmap.md`).
> Repo breakout référencé : `ludodefgh/an54lq-15-breakout` (fichier `INTEGRATION.md` = interface faisant foi).

---

## 1. Objectif de ce PCB

Ce n'est **pas** le PCB de production Myco Mini v1. C'est un PCB de validation intermédiaire qui doit :

1. Recevoir le breakout board `an54lq-15-breakout` en connecteurs socketés (swappable).
2. Servir de banc de test pour la géométrie des traces capacitives du capteur d'humidité sol (objectif principal).
3. Valider les footprints bare-die de SHT40-AD1B et BH1750FVI-TR avant de les envoyer en commande JLCPCB PCBA pour le vrai PCB v1.
4. Intégrer une protection contre l'inversion de polarité CR2032 (issue GitHub ouverte, jamais implémentée).
5. Offrir un moyen de tester différents comportements firmware (pairing, protocole) via DIP switches.

Ne pas optimiser pour la taille finale du produit — ce n'est pas contraint par un boîtier. Prioriser la facilité d'assemblage manuel (hotplate reflow) et de debug (test points accessibles).

---

## 2. Interface avec le breakout board (référence : `INTEGRATION.md`)

Le breakout **n'est pas à re-designer** — c'est une carte fille externe qui se connecte via headers. Le host n'a donc **pas besoin** du footprint du module AN54LQ-15 lui-même ; seulement des connecteurs pour le recevoir.

### Connecteurs requis côté host

| Connecteur | Type | Pitch | Notes |
|---|---|---|---|
| Récepteur pour `J1` breakout ("GPIO_L") | Header **femelle** 1×17, THT | 2.54mm | |
| Récepteur pour `J4` breakout ("GPIO_R") | Header **femelle** 1×17, THT | 2.54mm | Espacement centre-à-centre entre les deux rangs : **25.4mm exact** |

**Pas de connecteur pour `J5`** (SWD/alim dédié) — flash prévu directement sur le breakout, pas besoin de le dupliquer côté host.

### Signaux disponibles sur J1 (breakout, à mapper sur le header femelle host)

| Pin J1 | Net | Pin J1 | Net |
|---|---|---|---|
| 1 | GND | 10 | P1.02 (NFC1, GPIO libre) |
| 2 | P1.09 | 11 | P1.03 (NFC2, GPIO libre) |
| 3 | P1.10 | 12 | P1.04 |
| 4 | P1.11 | 13 | P1.05 |
| 5 | P1.12 | 14 | P1.06 |
| 6 | P1.13 | 15 | P1.07 |
| 7 | P1.14 | 16 | P1.08 |
| 8 | GND | 17 | *NC* |
| 9 | VDD_NRF | | |

### Signaux disponibles sur J4 (breakout, à mapper sur le header femelle host)

| Pin J4 | Net | Pin J4 | Net |
|---|---|---|---|
| 1 | NRESET (reset bufferisé, safe à driver push-pull) | 10 | P2.07 |
| 2 | P0.04 | 11 | P2.06 |
| 3 | P0.03 | 12 | P2.05 |
| 4 | P0.02 | 13 | P2.04 |
| 5 | P0.01 | 14 | P2.03 |
| 6 | P0.00 | 15 | P2.02 |
| 7 | P2.10 | 16 | P2.01 |
| 8 | P2.09 | 17 | P2.00 |
| 9 | P2.08 | | |

**Contraintes du breakout à respecter (ne pas dupliquer / ne pas violer) :**
- Ne pas ajouter de deuxième cristal 32.768kHz — LFXO déjà géré en interne sur le breakout.
- Ne pas ajouter de découplage bulk supplémentaire sur VDD_NRF au connecteur, sauf si la trace entre le connecteur et la source d'alimentation host dépasse quelques cm — dans ce cas, petit cap de bypass local près du connecteur (valeur à déterminer selon layout final, pas critique).
- Ne rien câbler sur DCC/DECD — non exposés sur les headers.
- SWDIO/SWDCLK ne sont **pas** répétés sur J1/J4 (uniquement sur J5, non utilisé ici) — aucune action requise côté host.

---

## 3. Assignation GPIO (host)

| Fonction | Pin nRF54L15 | Pin breakout | Notes |
|---|---|---|---|
| I2C SDA | P0.02 | J4 pin 4 | Bus partagé SHT40 + BH1750 |
| I2C SCL | P0.03 | J4 pin 3 | Bus partagé SHT40 + BH1750 |
| SAADC — soil moisture (segment 1 / surface) | P1.07 | J1 pin 15 | |
| SAADC — soil moisture (segment 2 / profondeur) | *à confirmer* | *à confirmer* | Voir §5 — choisir un 2e pin SAADC-capable parmi les GPIO libres (P1.04/P1.05/P1.06 candidats). **Confirmer dans le datasheet nRF54L15 quels pins sont réellement SAADC-capables avant de router.** |
| GPIO — boost EN (XC9145) | P1.13 | J1 pin 6 | Actif direct depuis GPIO, pas de MOSFET nécessaire (true load disconnect du XC9145) |
| GPIO — soil sensor VCC (power switch) | P1.14 | J1 pin 7 | Alimente le(s) segment(s) sol uniquement pendant la lecture |
| Reset (host-driven, optionnel) | NRESET | J4 pin 1 | Optionnel — le flash se fait via le breakout, ce signal n'est nécessaire que si le host doit reset le MCU en logiciel |
| DIP switch bit 0–3 | P2.00–P2.03 | J4 pins 17/16/15/14 | Pull-up interne firmware, pas de résistances externes |
| Batterie — mesure tension | Canal SAADC interne VDD si disponible, sinon pin dédiée | — | **Vérifier dans le datasheet nRF54L15 (chapitre SAADC) si un canal de mesure VDD interne existe** (architecture similaire aux nRF52/53 qui ont ce canal). Si oui : pas de GPIO ni de composant supplémentaire nécessaire, VDD_NRF = tension pile directement (MCU en connexion directe CR2032, pas de régulateur intermédiaire). Si non : fallback sur un pin SAADC libre + éviter tout diviseur résistif non commuté (fuite de courant continue incompatible avec le budget idle en µA). |
| VDD_NRF (alim) | — | J1 pin 9 | Voir §4 pour le chemin d'alimentation complet |
| GND | — | J1 pins 1/8 | |

---

## 4. Architecture d'alimentation

```
CR2032+ ─── [Protection inverse polarité, voir ci-dessous] ───┬─── VDD_NRF (breakout J1 pin 9)
                                                                └─── SHT40 VDD (direct, toujours alimenté)

nRF54L15 GPIO P1.13 ─── XC9145 EN pin
XC9145 VOUT (3.3V) ───┬─── BH1750 VCC
                       └─── Soil sensor VCC (via P1.14, switché)

CR2032− ─── GND commun
```

### Protection inverse polarité (nouveau, à implémenter)

Circuit P-MOSFET côté haut, en série sur le chemin VBAT+ avant qu'il n'atteigne VDD_NRF :

- **Composant :** AO3401 (P-channel, déjà utilisé ailleurs dans le projet — cohérence BOM)
- Source → CR2032+
- Drain → chemin VDD_NRF du système (vers J1 pin 9 et SHT40 VDD)
- Gate → GND système
- Orientation body diode : doit bloquer le courant en cas d'inversion de polarité (vérifier au schématique, cf. l'erreur d'orientation déjà notée dans le decision log pour l'AO3401 du boost control — ne pas répéter cette erreur ici)
- Chute de tension attendue en fonctionnement normal : négligeable (Iq × RDS(on), de l'ordre du mV à quelques µA de courant idle)

### Composants d'alimentation

| Composant | Référence | Rôle |
|---|---|---|
| Boost converter | TOREX XC9145B33CMR-G | 3.3V fixe, true load disconnect, EN pin direct GPIO |
| MOSFET protection inverse | AO3401 SOT-23 | Voir ci-dessus |
| Support CR2032 | — | À définir selon dispo JLCPCB/LCSC |

---

## 5. Capteur capacitif sol — segmentation expérimentale par profondeur

**Objectif de cette itération :** tester si une segmentation physique de l'électrode capacitive permet une lecture différenciée par profondeur (surface vs profondeur), en plus de valider la géométrie de base.

- **2 segments indépendants** empilés verticalement sur la sonde (pas de mux pour cette itération — 2 canaux SAADC séparés).
- Chaque segment : géométrie interdigitée en U, spacing 0.15mm, finition ENIG (cohérent avec le design PCB v1 documenté).
- Longueur totale de sonde disponible ~35–40mm (référence PCB v1) — à diviser entre les 2 segments ; accepter une sensibilité réduite par segment pour cette phase de test.
- Chaque segment a sa propre paire de traces de retour, routées séparément vers 2 pins SAADC-capables distincts (voir §3 — pin exact du 2e segment à confirmer selon les pins SAADC réels du nRF54L15).
- Alimentation des segments : partagée via le même GPIO de power-switch (P1.14) ou séparée si nécessaire pour isoler les lectures — à évaluer selon contraintes de routage.

**Épaisseur PCB sous la zone sonde :** 0.8mm (cohérent avec cible PCB v1) si faisable sur ce proto ; sinon épaisseur standard du fab acceptable pour cette phase de validation (préciser en note si le PCB entier est fabriqué à épaisseur uniforme plutôt que zonée).

---

## 6. Capteurs — SHT40 et BH1750 (bare-die)

| Capteur | Référence | Interface | Alimentation |
|---|---|---|---|
| Température/humidité air | SHT40-AD1B | I2C (P0.02/P0.03) | Direct CR2032 (toujours alimenté) |
| Luminosité | BH1750FVI-TR | I2C (P0.02/P0.03, bus partagé) | Via boost XC9145 (switché, P1.13) |

Footprints bare-die à utiliser (pas de dev board) — objectif explicite de valider ces footprints exacts avant la commande PCBA JLCPCB de PCB v1. Prévoir un espacement/orientation compatible avec assemblage manuel à la hotplate (pas de stencil prévu pour cette itération — pâte appliquée manuellement).

---

## 7. DIP switches (test de comportements firmware)

- Bloc DIP switch **4 positions**, SMD ou THT selon dispo JLCPCB/LCSC.
- Connecté à `P2.00`–`P2.03` (breakout J4 pins 17/16/15/14).
- Pull-up interne géré en firmware — **pas de résistances de tirage externes** sur le PCB.
- Usage prévu : sélection de mode (pairing on/off, protocole BLE vs 802.15.4 — voir note ci-dessous), lu au boot par le firmware.

**Note contextuelle (pas une contrainte de design, juste pour info) :** le nRF54L15 supporte nativement BLE + 802.15.4 (Thread/Zigbee/Matter) sur le même radio — ces DIP switches pourraient éventuellement servir à des tests de bascule protocole dans une itération firmware future. Aucun impact sur le layout PCB.

---

## 8. Contraintes de fabrication

- **EDA :** KiCad (version alignée avec le projet breakout, KiCad 10)
- **Fabrication :** JLCPCB, finition ENIG (cohérence corrosion pour la zone sonde sol)
- **Stratégie composants :** Basic Parts JLCPCB préférés quand possible ; ce PCB étant assemblé manuellement (hotplate), la contrainte Extended Parts est moins critique ici que pour PCB v1, mais garder les mêmes références que PCB v1 quand c'est possible pour cohérence BOM.
- **BOM :** générer au format compatible JLCPCB/LCSC en sortie (voir skill `jlcpcb-bom` du projet si disponible côté Claude Code).

---

## 9. Livrables attendus

1. Schématique KiCad (`.kicad_sch`) — breakout header interface, alimentation + protection, capteurs bare-die, capteur sol segmenté, DIP switches.
2. PCB (`.kicad_pcb`) — placement, routage, zone sonde sol avec les 2 segments capacitifs, footprints bare-die SHT40/BH1750.
3. BOM exportée (JLCPCB/LCSC format).
4. Toute question de faisabilité (footprint SAADC pin exact pour le 2e segment sol, canal VDD interne SAADC, épaisseur PCB zonée) doit être signalée explicitement plutôt que supposée — plusieurs points de ce brief sont marqués "à confirmer" et nécessitent une vérification datasheet avant routage définitif.

---

## 10. Points explicitement non résolus (à ne pas trancher silencieusement)

- Pin SAADC exact pour le 2e segment du capteur sol capacitif.
- Existence d'un canal SAADC interne de mesure VDD sur le nRF54L15 (sinon, prévoir un pin de fallback).
- Épaisseur PCB zonée (0.8mm sous la sonde) — faisable sur ce proto ou épaisseur uniforme acceptée pour cette itération.
- Choix final AN54LQ-15 vs AN54LV-15 pour PCB v1 (n'affecte pas ce host PCB, qui reçoit le breakout quel que soit le module dessus).
