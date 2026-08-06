# Myco — Module nRF54L15 minimal (form factor XIAO)

Objectif : créer un module nRF54L15 custom au format XIAO castellated,
alimenté par CR2032, permutable avec le XIAO nRF52840 sur les prototypes existants.

Base schéma : fichiers KiCad Seeed XIAO nRF54L15 Sense v1.0 (CC BY-SA 4.0)
Source : `/home/ludovic/Downloads/202004329;XIAO nRF54L15 sense v1.0_SCH&PCB/`
PCB : dessin from scratch (footprint QFN incompatible avec le PCB XIAO)

---

## Contexte

- **Chip : nRF54L15-QFAA-R** (QFN-48, 6×6mm, 0.5mm pitch) — dispo JLCPCB C42458750 ~$4.19
  - ≠ CAAA-R (WLCSP47) utilisé par Seeed — non dispo JLCPCB, nécessite 6 couches
- PCB **4 couches** suffisant pour QFN-48 (vs 6 couches pour WLCSP)
- PCB from scratch au format XIAO castellated (21×17.5mm) — le QFN 6×6mm rentre
- Alimentation : CR2032 (3V) directement sur VDD (1.7–5.5V accepté par nRF54L15)
- Programmation : pads SWD exposés (pas de SAMD11)
- Même pinout XIAO → compatible breadboard et futurs PCBs capteur

### Cristaux — condensateurs de charge

Pas de caps externes nécessaires. Le nRF54L15 a des caps internes programmables.
Valeurs tirées du DTS Seeed (`platform-seeedboards/zephyr/boards/arm/xiao_nrf54l15/xiao_nrf54l15_nrf54l15_cpuapp.dts`) :

```dts
&hfxo {
    load-capacitors = "internal";
    load-capacitance-femtofarad = <16000>;  /* 16pF interne → CL effective ≈ 8pF */
};
&lfxo {
    load-capacitors = "internal";
    load-capacitance-femtofarad = <16000>;  /* 16pF interne → CL effective ≈ 8pF (spec cristal 7pF) */
};
```

Ces valeurs sont à copier telles quelles dans le board DTS custom — mêmes cristaux (1S32000049 et SC16S-7PF20PPM).

---

## Note — Alternative : carrier PCB avec module Raytac AN54LV-15

> À considérer avant de commencer le design QFAA-R.

Le module Raytac AN54LV-15 (8.4×6.4×1.5mm, nRF54L15 WLCSP + RF certifié) rentre
dans un carrier au format XIAO (21×17.5mm). Le carrier serait alors :
- 2 couches (vs 4 pour QFAA-R)
- ~10 composants (vs ~30)
- Zéro RF design — antenne et matching intégrés et certifiés FCC/CE
- Contrainte : zone no-ground 5×2.9mm sous l'antenne sur le carrier
- Contrainte : module à sourcer séparément (pas sur JLCPCB), soudage manuel

**Décision en attente.** Les deux options sont viables pour un usage perso.

---

## Étape 0 — Récupérer le symbole KiCad pour QFAA-R

Le schéma Seeed utilise le CAAA-R (WLCSP47). Il faut le remplacer par le QFAA-R (QFN-48).

- [ ] Trouver/créer le symbole KiCad nRF54L15-QFAA-R (Nordic ou SnapEDA/UltraLibrarian)
- [ ] Vérifier que les noms de pins correspondent au datasheet Nordic nRF54L15 PS
- [ ] Créer le footprint QFN-48 6×6mm 0.5mm pitch (ou importer depuis KiCad lib)
- [ ] Vérifier pad thermique central (exposed pad)

---

## Étape 1 — Schéma KiCad

### 1.1 Feuille "03 Power" — Tout supprimer sauf les rails

Supprimer entièrement :
- [ ] `U5` TPS628438YKAR — buck converter 5V→3.3V
- [ ] `L1` 1µH + `C9` 4.7µF + `C10/C11` 10µF × 2 + `C12/C13` 100nF × 2 — passifs buck
- [ ] `R9` 249K — feedback buck
- [ ] `U1` SGM40567-4.2XG/TR — chargeur batterie LiPo
- [ ] `R4` 120K — iCharge set
- [ ] `C2/C3` 1µF — passifs chargeur
- [ ] `USB1` ST-USB-3316T — connecteur USB-C
- [ ] `R1/R2` 5.1K — résistances CC USB
- [ ] `D1` DF2B7ASL — diode ESD USB
- [ ] `C1` 100nF — filtre USB
- [ ] `Q1` LP0404N3T5G — PMOS sélecteur VBAT/VBUS
- [ ] `D4` PSBD2FD40V1H — diode idéale
- [ ] `U3` SGM2040-3.3YUDH4G/TR — LDO dédié SAMD11
- [ ] `C5/C6` 1µF + `C7/C8` 100nF + `C26` 100nF — passifs LDO SAMD11
- [ ] `U2` TPS22916CYFPR — load switch IMU
- [ ] `U11` TPS22916CYFPR — load switch mic
- [ ] `R5/R6` 10K — diviseur tension batterie (lecture ADC)
- [ ] `R3` 100K + `C4` 100nF — passifs battery read
- [ ] `R10` 10K — pull-up BAT_VOL_READ

Ajouter :
- [ ] Symbole connecteur 2 pins BAT+ / BAT− (CR2032 ou JST-PH 2mm)
- [ ] Net `VBAT` directement relié à `VSYS_3V3` (renommer ou fusionner le rail)
- [ ] Caps découplage CR2032 : `10µF` bulk + `100nF` filtre sur le nouveau rail

### 1.2 Feuille "04 Debug & XIAO Header" — Supprimer le bloc SAMD11

Supprimer entièrement :
- [ ] `U4` ATSAMD11D14A-UUT — debug IC
- [ ] `U12` UM3301DA — level shifter SWD
- [ ] `Q2` LN237N3T5G — MOSFET reset nRF
- [ ] `C7/C8` 100nF — découplage SAMD11
- [ ] `RB` 100K — pull-up SWDIO SAMD11
- [ ] `R7` 100K — pull-up nRF reset
- [ ] `R8` 100K — pull-up SAMD11 reset
- [ ] Tous les labels SAMD11_* (SAMD11_USB_DP/DN, SAMD11_SWDIO, SAMD11_SWCLK, SAMD11_RESET, SAMD11_3V3)
- [ ] Labels SAMD11_TX / SAMD11_RX (P1.08/P1.09 redeviennent libres)

Conserver :
- [ ] `U9` XIAO Pin header — connecteur castellated (NE PAS TOUCHER)
- [ ] Test points SWD dos : TP1 (SWCLK), TP2 (SWDIO), TP3 (GND), TP4 (RST), TP5 (3V3)

Ajouter :
- [ ] Connecteur SWD 4 pins ou labels nets exposés : `nRF54_SWDIO`, `nRF54_SWCLK`, `VSYS`, `GND`

### 1.3 Feuille "06 Peripherals" — Supprimer IMU et mic

Supprimer entièrement :
- [ ] `U7` LSM6DS3TR-C — IMU
- [ ] `C22/C23` 100nF + `C24/C25` 100nF — découplage IMU
- [ ] `R12/R13` 10K — pull-ups I2C IMU (si non utilisés ailleurs)
- [ ] `U8` MSM261DGT006 — PDM microphone
- [ ] `C28` 100nF — découplage mic
- [ ] `R14` 10K + `R15` 1.5K — polarisation mic
- [ ] `K1/K2` TD-1183SN — boutons (optionnel, garder K1 si reset voulu)
- [ ] `R16–R21` 0R — résistances de configuration (config ponts IMU/mic)
- [ ] `R18/R19` 4.7K — pull-ups
- [ ] `L2` 4.7µH — inductance NFC (si NFC non utilisé)
- [ ] `D2` LED rouge (optionnel — garder `D3` LED verte si voulu)

### 1.4 Feuille "05 nRF54L15-CAAA-R" — NE PAS TOUCHER

Tout conserver tel quel :
- `U6` nRF54L15-CAAA-R
- `U10` FM8625H (RF switch : VDD→P2.03, VCTL→P2.05)
- `X1` 32MHz HFXO (1S32000049, CL=8pF, ESR=70Ω) — pas de caps externes
- `X2` 32.768kHz LFXO (SC16S-7PF20PPM, CL=7pF) — pas de caps externes
- `ANT1` KH5220-A36 (antenne céramique 2.4GHz)
- `FB1` 120Ω ferrite + `C15` 2.2µF + `C16` 10nF + `C17` 3.9pF — filtre DEC
- `C14` 2.2µF — découplage VSS_PA
- Réseau RF matching : `L3`(3.6nH) `L4`(4.7nH) `L5`(2nH) `L6`(6.8nH)
- Caps RF : `C18`(1.2pF) `C19`(1.2pF) `C29`(100pF) `C27`(100pF) `C31`(1pF) `C20`

---

## Étape 2 — PCB KiCad (from scratch)

> Le PCB Seeed est basé sur le CAAA-R WLCSP47 — non réutilisable.
> Nouveau PCB au format XIAO castellated (21×17.5mm), 4 couches.

### 2.0 Setup projet KiCad

- [ ] Nouveau projet KiCad, board outline XIAO (21×17.5mm, coins arrondis R=0.5mm)
- [ ] Stackup 4 couches : F.Cu / In1.Cu (GND) / In2.Cu (PWR) / B.Cu
- [ ] Design rules JLCPCB 4 couches : min trace 0.1mm, min via 0.2mm drill / 0.45mm annular

### 2.1 Footprints à NE PAS reprendre du PCB Seeed

| Ref | Composant |
|-----|-----------|
| U1 | SGM40567 chargeur |
| U2 | TPS22916 load switch IMU |
| U3 | SGM2040 LDO SAMD11 |
| U4 | ATSAMD11D14A |
| U5 | TPS628438 buck |
| U7 | LSM6DS3TR-C IMU |
| U8 | MSM261DGT006 mic |
| U11 | TPS22916 load switch mic |
| U12 | UM3301DA level shifter |
| Q1 | LP0404N3T5G PMOS |
| Q2 | LN237N3T5G PMOS |
| USB1 | USB-C |
| D1 | ESD USB |
| D4 | PSBD2FD40V1H |
| L1 | 1µH buck |
| L2 | 4.7µH NFC |
| R1, R2 | 5.1K CC USB |
| R3, R4 | 100K, 120K |
| R5, R6 | 10K diviseur batt |
| R7, R8 | 100K reset |
| R9 | 249K feedback buck |
| R10 | 10K BAT_VOL |
| R12–R19 | pull-ups + config |
| R16–R21 | 0R config |
| C1–C9 | caps buck/SAMD11/USB/charger |
| C22–C26 | caps IMU/mic |
| K1, K2 | boutons |
| ANT2 | IPEX (optionnel — garder si antenne externe voulue) |
| D2 | LED rouge |
| Tous TP* dos sauf SWD | test points |

### 2.2 Footprints à placer sur le nouveau PCB

| Ref | Composant |
|-----|-----------|
| U6 | nRF54L15-QFAA-R (QFN-48, C42458750) |
| U9 | XIAO Pin header (castellated) |
| U10 | FM8625H RF switch |
| X1 | 32MHz HFXO |
| X2 | 32.768kHz LFXO |
| ANT1 | KH5220-A36 antenne |
| FB1 | 120Ω ferrite |
| L3, L4, L5, L6 | RF matching |
| C10, C11 | 10µF × 2 découplage VDD |
| C12, C13 | 100nF découplage VDD |
| C14, C15 | 2.2µF découplage |
| C16 | 10nF DEC |
| C17 | 3.9pF DEC |
| C18, C19 | 1.2pF RF matching |
| C20, C27, C29, C31 | RF switch + matching |
| D3 | LED verte (optionnel) |
| TP1–TP5 dos | SWD test points |

### 2.3 Footprints à ajouter

- [ ] Connecteur batterie : 2 pads THT (CR2032 holder) ou empreinte JST-PH 2mm sur bord
- [ ] Bulk CR2032 : `10µF` 0402 proche du bord BAT+
- [ ] Connecteur SWD dos : 4 pads 1.27mm (SWDIO, SWDCLK, GND, VDD) ou Tag-Connect TC2030

### 2.4 Reroutage à faire

- [ ] Supprimer tous les nets liés aux composants supprimés
- [ ] Rerouter le rail VDD principal : `BAT+` → `VSYS_3V3` (ex-sortie buck)
- [ ] Vérifier continuité : `BAT+` → caps bulk → VDD U6 (2 balls) + VDD U10

---

## Étape 3 — Vérification

- [ ] ERC schéma → 0 erreurs bloquantes
- [ ] DRC PCB → 0 erreurs (attention aux clearances 6 couches)
- [ ] Vérifier que les nets SWD (nRF54_SWDIO, nRF54_SWCLK) sont bien exposés sur les pads dos
- [ ] Vérifier continuité LFXO : X2 pin1 → XL2 (P1.01) et X2 pin2 → XL1 (P1.00)
- [ ] Vérifier RF switch : P2.03 → U10 VDD, P2.05 → U10 VCTL

---

## Étape 4 — Export et commande JLCPCB

- [ ] Générer Gerbers (4 couches : F.Cu, In1.Cu, In2.Cu, B.Cu + Edge.Cuts)
- [ ] Générer fichier drill
- [ ] Générer BOM + CPL (assembly JLCPCB)
- [ ] Commande JLCPCB : 4 couches, ENIG recommandé (QFN pad thermique), épaisseur 1mm
- [ ] Vérifier que nRF54L15-QFAA-R (C42458750) est bien en stock au moment de la commande

---

## Notes importantes

**Alimentation CR2032**
- VDD nRF54L15 : 1.7V–5.5V → CR2032 (3V nominal, 2.0V cutoff) compatible direct
- Peak TX BLE : ~5mA — CR2032 impédance ~10Ω → chute ~50mV, OK
- Ajouter 10µF bulk obligatoire pour stabiliser les bursts TX

**RF switch FM8625H**
- Sans activation : signal BLE à -96 dBm (inutilisable)
- P2.03 (rfsw_pwr) = HIGH → alimente VDD du switch
- P2.05 (rfsw_ctl) = HIGH → sélectionne antenne céramique ANT1
- Conserver `regulator-boot-on` dans l'overlay Zephyr

**Programmation**
- 1er flash : J-Link EDU Mini (ou nRF9160 DK) via pads SWD
- Pas de SAMD11, pas d'USB sur le module

**Compatibilité XIAO nRF52840**
- Même pinout castellated 14 pins → permutable directement
- Firmware Zephyr : branch `feature/nrf54l15-zephyr`, board `xiao_nrf54l15/nrf54l15/cpuapp`
