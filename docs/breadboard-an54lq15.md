# Breadboard : firmware Myco sur le breakout AN54LQ-15

Détour de validation hardware — faire tourner le firmware Zephyr existant (déjà validé sur XIAO nRF54L15 Sense en Phase 2) sur le breakout custom `NRF54L_breakout` (module Raytac AN54LQ-15 nu, sans la carte XIAO autour), avec les capteurs des protos précédents. Objectif principal : confirmer que le module nu tient sur une pile CR2032 — le XIAO Sense n'y arrivait pas.

Board Zephyr : `raytac_an54l15q_db/nrf54l15/cpuapp` (target officiel le plus proche du module — c'est la eval board Raytac, pas notre breakout, mais c'est le bon SoC/module ; on écrase son overlay de toute façon). Branche firmware : `feature/an54lq15-breakout-test`.

Montage simplifié au minimum pour ce test : pas de MOSFET de gating, pas de LED. Juste le breakout + les capteurs + BLE.

## 1. Vue d'ensemble

```
                    ┌─────────────────────────┐
   CR2032 (+) ──────┤ VDD_NRF        NRF54L_   │
   CR2032 (−) ──┬───┤ GND            breakout  │
                │    │                (J1/J4/J5)│
                │    └─────────────────────────┘
                │
                └──────────────────── GND commun (breadboard rail −)
```

Alimentation par pile bouton CR2032 directe sur `VDD_NRF` — c'est tout l'intérêt du test. Pas de régulateur externe côté breakout : le module gère déjà son propre DC/DC interne (voir `INTEGRATION.md` du repo breakout).

## 2. Table de câblage

Tout regroupé côté **J1** (GPIO_L) pour que la sonde sol + le bus I2C soient du même côté sur la breadboard — plus de câblage qui traverse vers J4.

| Fonction | Pin nRF54L15 | Pin breakout | Vers |
|---|---|---|---|
| Alim | VDD_NRF | **J1 pin 9** (ou J5 pin 1) | CR2032 (+) |
| Masse | GND | **J1 pin 1**, **J1 pin 8** (ou J5 pin 4) | CR2032 (−) / rail GND commun |
| I2C SDA | P1.10 | **J1 pin 3** | SHT40 SDA + BH1750 SDA (bus partagé) |
| I2C SCL | P1.11 | **J1 pin 4** | SHT40 SCL + BH1750 SCL (bus partagé) |
| ADC sol | P1.05 (AIN1) | **J1 pin 13** | Sortie analogique sonde sol capacitive |
| ADC batterie | P1.06 (AIN2) | **J1 pin 14** | VDD_NRF (lecture directe de la pile, même rail) |
| GPIO soil_pwr | P1.04 | **J1 pin 12** | VCC sonde sol (alim commutée, "high drive") |

Tous les pins ci-dessus sont confirmés exposés sur J1 dans `PIN_MAPPING.md` du repo breakout (aucune collision avec les pins non-exposés XL1/XL2/DCC/DECD).

### SHT40 + BH1750 (bus I2C partagé)

Les deux sur le même rail — sortie du boost converter (§3) — plutôt que de scinder l'alim des deux capteurs entre VDD_NRF et le boost. Le SHT40 fonctionne sur 1.08–3.6V donc une sortie boost dans la plage du BH1750 (2.4–3.6V) lui convient aussi.

| Sonde | VCC | GND | SDA | SCL |
|---|---|---|---|---|
| SHT40 (0x44) | sortie du boost converter — voir §3 | GND | J1 pin 3 | J1 pin 4 |
| BH1750 (0x23) | sortie du boost converter — voir §3 | GND | J1 pin 3 | J1 pin 4 |

Les modules SHT40/BH1750 du commerce ont quasiment toujours des pull-ups SDA/SCL embarquées (4.7 kΩ typique), référencées à leur propre VCC — donc maintenant à la sortie du boost, pas à VDD_NRF. **À vérifier** : si la sortie du boost dépasse notablement VDD_NRF (la pile CR2032, elle, alimente le SoC directement sans passer par le boost), les pull-ups tireraient SDA/SCL au-dessus de l'alim du SoC — comparer la tension de sortie du boost à VDD_NRF et à l'abs-max GPIO du nRF54L15 avant de brancher. Si pas de pull-ups sur les modules, ajouter 2× 4.7 kΩ vers la sortie du boost (pas vers VDD_NRF).

### Sonde sol capacitive

| Sonde | VCC | GND | AOUT |
|---|---|---|---|
| Sonde sol | J1 pin 12 (`soil_pwr`) | GND | J1 pin 13 (`AIN1`) |

Alimentée directement par le GPIO `soil_pwr` (pas de transistor — le firmware configure ce pin en "high drive"), coupée entre les mesures pour économiser l'énergie.

## 3. SHT40 + BH1750 : boost converter toujours allumé, pas de MOSFET

Sur les protos précédents, un MOSFET P AO3401 gatait un boost converter dédié au BH1750 (celui-ci a besoin de 2.4–3.6V, insuffisant en fin de vie de pile) — piloté par le GPIO `boost_en`. Pour ce test de mise en service, ce circuit est simplifié :

- **Pas de MOSFET, pas de pin `boost_en`** — retiré du firmware et de l'overlay (plus de GPIO à piloter).
- **Boost converter toujours actif** : ton module boost (entrée sur VDD_NRF/CR2032, sortie ~3.0–3.6V) alimente **les deux capteurs I2C** en continu, sans pin EN à câbler.

| Boost converter | IN | GND | OUT |
|---|---|---|---|
| (toujours-on) | VDD_NRF / CR2032 (+) | GND commun | → SHT40 VCC + BH1750 VCC |

## 4. Pas de LED

Pas de LED de statut sur ce montage. La vérification que le firmware tourne se fait uniquement en scannant l'annonce BLE (§6) — pas de clignotement à observer.

## 5. Flash — via ta sonde Pico (cf. `PICO-SWD-FLASHING-GUIDE.md`)

### 5.1 Câblage SWD (header J5)

| Pico | Signal | J5 (breakout) |
|---|---|---|
| GPIO2 (pin 4) | SWCLK | **J5 pin 3** |
| GPIO3 (pin 5) | SWDIO | **J5 pin 2** |
| GPIO1 (pin 2) | RESET | **J5 pin 5** |
| GND | GND | **J5 pin 4** |
| 3V3(OUT) (pin 36) | alim cible | **J5 pin 1** (VDD_NRF) |

Pour la **première mise en service** (flash + vérif que ça boot), alimente depuis le Pico (3V3 OUT), **pas** de CR2032 branchée en même temps — cf. l'avertissement du guide (deux alims en //, pas de level-shifter sur un Pico nu).

### 5.2 Build

```bash
TC=~/ncs/toolchains/2ac5840438
export PATH="$TC/bin:$TC/usr/bin:$TC/usr/local/bin:$TC/opt/bin:$TC/opt/nanopb/generator-bin:$TC/nrfutil/bin:$TC/opt/zephyr-sdk/arm-zephyr-eabi/bin:$TC/opt/zephyr-sdk/riscv64-zephyr-elf/bin:$PATH"
export LD_LIBRARY_PATH="$TC/lib:$TC/lib/x86_64-linux-gnu:$TC/usr/local/lib:$LD_LIBRARY_PATH"
export GIT_EXEC_PATH="$TC/usr/local/libexec/git-core"
export GIT_TEMPLATE_DIR="$TC/usr/local/share/git-core/templates"
export PYTHONHOME="$TC/usr/local"
export PYTHONPATH="$TC/usr/local/lib/python3.12:$TC/usr/local/lib/python3.12/site-packages"
export NRFUTIL_HOME="$TC/nrfutil/home"
export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
export ZEPHYR_SDK_INSTALL_DIR="$TC/opt/zephyr-sdk"

cd zephyr
west build -b raytac_an54l15q_db/nrf54l15/cpuapp -d build_breakout .
```

Déjà buildé et validé (0 warning, 0 erreur) — flash 11.24 %, RAM 16.35 %. Binaire : `zephyr/build_breakout/merged.hex`.

### 5.3 Flash (pyOCD — pas nrfutil/nrfjprog/OpenOCD, cf. guide §5)

```bash
~/pyocd-venv/bin/pyocd flash -t nrf54l zephyr/build_breakout/merged.hex
~/pyocd-venv/bin/pyocd reset -t nrf54l
```

## 6. Vérifier que ça tourne

- Scanner BLE (nRF Connect for Mobile, ou Home Assistant si le dongle BLE est à portée) : device `Myco`, service data UUID `0xFCD2`, données qui se mettent à jour toutes les ~5s.
- Logs UART si besoin : `CONFIG_LOG` est actif dans `prj.conf`, mais aucun UART n'est câblé sur ce montage — ajouter plus tard sur `uart20` si besoin de debug plus fin.

## 7. Test CR2032 (l'objectif de ce détour)

Une fois le firmware validé en direct via le Pico :

1. Débrancher **tout** le Pico (alim ET SWD — évite tout risque de mismatch de niveau logique entre 3V3 Pico et pile).
2. Brancher la CR2032 seule sur J1 pin 9 (+) / J1 pin 1 (−).
3. Scanner en BLE — si l'advertising démarre et persiste, le module tient sur pile bouton (contrairement au XIAO Sense).

## 8. Ce qui a changé côté firmware

Nouveau fichier `zephyr/boards/raytac_an54l15q_db_nrf54l15_cpuapp.overlay` (le `.overlay` XIAO existant n'est pas touché, les deux coexistent — Zephyr applique celui qui correspond à la board buildée) :

- Pinctrl I2C22 défini à la main (SDA=P1.10, SCL=P1.11) — la target `raytac_an54l15q_db` n'en a pas par défaut.
- Channels ADC 1/2 (AIN1/AIN2) définis à la main, même raison.
- `soil_pwr` en GPIO simple sur **P1.04 (J1 pin 12)** — déplacé depuis P2.02 (J4) pour rester du même côté que le bus I2C et l'ADC sol.
- `boost_en` et `user_led` **retirés** (noeud devicetree + alias + code `main.c`) : pas de MOSFET de gating, pas de LED pour ce test.
- `&spi00` désactivé — la eval board Raytac a une flash SPI externe sur ces pins par défaut, absente sur notre breakout. Sans conséquence pratique (`CONFIG_SPI` n'est pas actif) mais désactivé par propreté.
- Pas de noeud `rfsw_pwr`/`rfsw_ctl` (spécifique à l'antenne switchée du XIAO Sense) — le module AN54LQ-15 a une antenne céramique intégrée, rien à activer.
- `CONFIG_REGULATOR` retiré de `prj.conf` (n'était là que pour `rfsw_pwr`/`rfsw_ctl`).
- `src/main.c` : `boost_en`/`user_led` retirés (déclaration GPIO, config au boot, clignotement dans la boucle). Le reste (capteurs, ADC, BTHome, BLE) est inchangé.
- `CMakeLists.txt`, `Kconfig.sysbuild`, `sysbuild/ipc_radio/prj.conf` : **inchangés**.
