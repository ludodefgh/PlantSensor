# Myco Mini — Host PCB : journal de décisions (v0.1-draft)

> Ce document accompagne `hardware/myco-mini-host-pcb/`. C'est un **premier jet généré automatiquement**
> (schématique + PCB construits programmatiquement à partir de `docs/host-pcb-design-brief.md`), pensé
> comme base de discussion et d'itération — pas comme design prêt à fabriquer. Tout ce qui suit doit être
> relu dans KiCad avant toute commande.

---

## 1. Où sont les fichiers

```
hardware/myco-mini-host-pcb/
├── myco-mini-host-pcb.kicad_pro      # projet KiCad (KiCad 10)
├── myco-mini-host-pcb.kicad_sch      # schématique (feuille unique, plate)
├── myco-mini-host-pcb.kicad_pcb      # PCB — footprints placés, RIEN N'EST ROUTÉ
├── bom_jlcpcb.csv                    # BOM format JLCPCB/LCSC
├── erc_report.json / drc_report.json # derniers rapports ERC/DRC
├── netlist_export.json               # netlist intermédiaire (généré par le script schéma, consommé par le script PCB)
└── libs/
    ├── myco_host.kicad_sym           # symboles (récupérés via easyeda2kicad + 2 symboles custom)
    ├── myco_host.pretty/             # footprints (idem + sonde sol générée)
    └── myco_host.3dshapes/           # modèles 3D récupérés
```

Le schématique et le PCB ont été **générés par script** (S-expressions KiCad écrites directement, pas dessinées à la main dans l'éditeur), puis validés avec `kicad-cli sch erc` / `kicad-cli pcb drc` et inspectés visuellement via export SVG. C'est un choix délibéré pour ce premier jet — voir §6.

---

## 2. Décisions prises (avec justification)

### 2.1 Sourcing composants

| Réf | Part | LCSC | Statut |
|---|---|---|---|
| U2 | SHT40-AD1B-R2 | C2909890 | Confirmé disponible JLCPCB/LCSC, symbole+footprint récupérés (DFN-4-EP 1.5×1.5) |
| U3 | BH1750FVI-TR | C78960 | Confirmé, WSOF-6 |
| Q1, Q2 | AO3401A | C15127 | Confirmé, SOT-23, cohérent avec le reste du projet (déjà utilisé pour le boost control BH1750 en Phase 1/2) |
| BT1 | CR2032-BS-6-1 | C70377 | Support **traversant** (THT), pas SMD — voir §3.1 |
| SW1 | EM-04-Q (Diptronics) | C501635 | DIP switch 4 positions SMD, 8 broches |
| U1 | XC9145B33C0R-G | C6052816 | **Problème non résolu — voir §3.2** |

**Comment les symboles/footprints ont été obtenus :** via `easyeda2kicad` (déjà installé sur la machine, utilisé précédemment dans le projet — présence de `~/mes_composants.kicad_sym`). Le User-Agent par défaut de l'outil est bloqué par le WAF d'EasyEDA depuis une mise à jour côté serveur ; il a fallu patcher `~/.local/share/pipx/venvs/easyeda2kicad/.../easyeda_api.py` pour utiliser un User-Agent de navigateur. **Modification locale à l'outil, hors du repo, réversible par réinstallation pipx.**

### 2.2 Protection inverse de polarité (Q1)

Câblé exactement comme spécifié dans le brief : Source→BAT_RAW (CR2032+), Drain→VDD_NRF, Gate→GND. C'est la topologie standard "ideal diode" P-MOSFET high-side. Je n'ai pas eu besoin de trancher — le brief donnait déjà l'orientation exacte, avec la mise en garde explicite de ne pas répéter l'erreur d'orientation déjà commise ailleurs dans le projet sur un autre AO3401.

### 2.3 Pull-down sur CE (U1) — ajout non demandé par le brief

Le datasheet Torex XC9145 (lu directement, `product.torexsemi.com/system/files/series/xc9145.pdf`) est explicite : *"Do not leave the CE pin open"* — état indéfini si flottant. Comme le GPIO qui pilote CE (P1.13) est en haute impédance tant que le firmware ne l'a pas configuré au boot, j'ai ajouté **R6 (100k) en pull-down** entre CE et GND pour garantir un état OFF défini avant l'init firmware.

**Pourquoi c'est signalé ici :** ce n'est pas dans le brief. Décision d'ingénierie ajoutée pour respecter une contrainte datasheet explicite, pas une supposition arbitraire — mais à valider que ça n'entre pas en conflit avec le comportement voulu du firmware.

### 2.4 Pull-up sur le gate de Q2 (switch VCC sonde sol) — ajout non demandé

Même raisonnement que 2.3 : R5 (100k) entre le gate de Q2 et VOUT_3V3 (source), pour un état OFF par défaut avant que P1.14 soit configuré par le firmware. Risque plus faible que pour CE (VOUT_3V3 n'existe que si le boost est déjà actif), mais gardé par cohérence/sécurité.

### 2.5 Pull-ups I2C sur le rail VDD_NRF (toujours alimenté), pas sur VOUT_3V3

R1/R2 (4.7k, valeur standard non spécifiée dans le brief — cohérent avec la valeur mentionnée dans le datasheet ROHM du BH1750) tirent SDA/SCL vers **VDD_NRF** et non vers VOUT_3V3. Raison : le SHT40 est "toujours alimenté" (direct CR2032) alors que le BH1750 dépend du boost activé. Si les pull-ups étaient sur VOUT_3V3, le bus I2C n'aurait pas de tension de tirage quand le boost est éteint — cassant la communication avec le SHT40 dans ce mode, qui est justement censé fonctionner indépendamment du boost.

### 2.5bis Correction post-review — pin DVI du BH1750 déplacée de VOUT_3V3 vers VDD_NRF

En relisant le datasheet ROHM BH1750FVI en détail (suite à une question directe sur le câblage I2C), j'ai trouvé une vraie incohérence dans le premier jet : DVI ("I²C bus reference voltage") était câblée sur VOUT_3V3 (3.3V fixe) alors que les pull-ups SDA/SCL sont sur VDD_NRF (~3.0V nominal CR2032, descend vers ~2.0V en fin de vie pile). Le seuil `VIH1 = 0.7 × DVI` détermine le niveau "haut" reconnu par le BH1750 sur SDA/SCL. Avec DVI=3.3V fixe, `VIH1 = 2.31V` — si VDD_NRF descend sous ce seuil en fin de vie de pile, le BH1750 risque de ne plus reconnaître un "haut" valide sur le bus, alors que le SHT40 (sur le même rail que les pull-ups) continuerait de fonctionner. **Corrigé : DVI câblée sur VDD_NRF**, le même rail que les pull-ups, pour que le seuil suive la tension réelle du bus quel que soit l'état de la pile.

**Point encore ouvert (pas re-tranché, présenté ici pour transparence) :** DVI est aussi la broche de reset asynchrone du BH1750. Le datasheet recommande que DVI passe à 'L' après la mise sous tension de VCC (VOUT_3V3, ici piloté par le boost), avec un "reset term" ≥ 1µs, plutôt que d'être déjà haute avant que VCC ne s'active — ce qui est exactement notre cas maintenant puisque VDD_NRF est toujours actif alors que VOUT_3V3 s'allume/s'éteint à chaque cycle de lecture. Le datasheet qualifie ce cas de "don't care state" pour ADDR/SDA/SCL. En pratique, beaucoup de breakout boards BH1750 du commerce câblent DVI directement sur VCC sans réseau RC de reset et fonctionnent quand même — donc le risque réel est probablement faible, mais **non vérifié empiriquement**. Si des lectures BH1750 se révèlent peu fiables au premier boot après chaque activation du boost, ajouter un réseau RC de reset sur DVI (ou reconsidérer ce câblage) est la première chose à essayer.

### 2.6 Topologie du front-end analogique de la sonde sol — **interprétation, pas une spec du brief**

Le brief dit seulement : *"chaque segment a sa propre paire de traces de retour, routées séparément vers 2 pins SAADC-capables distincts"* — sans donner le circuit exact. J'ai implémenté un pont diviseur RC classique par segment :

```
SOIL_VCC (switché par Q2/P1.14) ──[R_bias 1MΩ]──┬── peigne A (comb A) ── (diélectrique = sol) ── peigne B (comb B) ── GND
                                                  └── vers pin SAADC
```

R3/R4 = 1MΩ, valeur choisie arbitrairement pour une constante de temps RC raisonnable avec une capacité de l'ordre du pF — **non validée empiriquement**, à ajuster une fois le vrai capteur testé (c'est explicitement l'objectif de ce PCB de proto). Si le vrai front-end prévu est différent (oscillateur 555, CVD, etc.), tout ce bloc est à refaire.

**Mise à jour — session de recherche approfondie (2026-08-09), suite à une question directe sur le 555.**

*Pourquoi les sondes du commerce (v1.2/v2.0) utilisent un 555 :* c'est un module autonome pensé pour n'importe quel MCU hôte avec un ADC générique — le 555 convertit la capacité en tension DC directement en hardware (oscillateur + redresseur + lissage), donc pas de logique de timing à implémenter côté MCU. Chez nous, c'est le **même** nRF54L15 qui pilote l'alim (Q2/P1.14) et lit le SAADC — la conversion capacité→signal peut se faire en firmware (charge du RC, échantillonnage à un délai contrôlé), sans puce dédiée.

*Calcul de la capacité attendue (formule Mamishev/Igreja-Dias pour IDC coplanaire, approximation de Hilberg pour le ratio K(k)/K'(k)) :* pour la géométrie actuelle (doigts 0.3mm, espacement 0.15mm, longueur utile 15.6mm, 20 doigts/segment) :

| Scénario | εr effectif (FR4+milieu)/2 | C estimée/segment |
|---|---|---|
| Sec (air) | 2.75 | ~18 pF |
| Sol humide (εr~15) | 9.75 | ~65 pF |
| Sol mouillé (εr~25) | 14.75 | ~99 pF |
| Saturé/eau (εr~80) | 42.25 | ~283 pF |

Formule engineering-grade (±précision correcte pour du dimensionnement, pas pour de la fabrication finale — cf. limitation similaire notée par les calculateurs IDC en ligne). Comparé à des mesures réelles trouvées en ligne : un capteur v1.2 commercial (TLC555) mesure ~30pF (air) à ~400pF (eau) sur sa propre géométrie — même ordre de grandeur, rassurant. À l'inverse, un hacker DIY (Hackaday.io, projet "A cheap capacitive soil moisture sensor") qui a tenté exactement notre approche (RC direct + Arduino) sur sa propre sonde n'a obtenu que **~14pF de variation totale — jugé insuffisant** pour une détection fiable en lecture directe. Notre géométrie, avec son estimation ~18→99-283pF, est nettement plus favorable que cette tentative ratée — bon signe pour la viabilité de l'approche RC directe, mais reste à confirmer sur la vraie sonde (parasites de trace/connecteur non inclus dans ce calcul).

*Contrainte SAADC trouvée dans le datasheet nRF54L15 (§11.17, p.912) :* la résistance source maximum caractérisée est **800kΩ**, même au temps d'acquisition max (40µs) — nos **R3/R4=1MΩ (valeur initiale) dépassaient cette limite caractérisée**. Nuance : le datasheet caractérise probablement le cas d'une simple résistance sans réservoir de charge local, alors que notre nœud a justement un gros condensateur local (la sonde elle-même, 18-283pF, bien plus gros que la capacité d'échantillonnage interne typique d'un SAR ADC) qui pourrait atténuer ce problème en pratique — nuance **pas vérifiable sans mesure réelle**.

*Choix final R3/R4 = 220kΩ (round 2, suite à une question directe "pourquoi pas plus proche de 800k") :* vérification du tableau `tACQ` du datasheet — le ratio `tACQ/R` est **constant** (≈0.05µs/kΩ pour R≥10k). Donc `tACQ/τ_ext = tACQ/(R×C) = 0.05/C`, **indépendant de R** : se rapprocher de 800k n'apporte strictement aucun gain de précision, le ratio "fenêtre d'acquisition ADC vs constante de temps propre du signal" est fixé uniquement par la capacité de la sonde. Ce qui compte réellement : (1) garder de la marge sous le plafond absolu (tolérance résistance, dérive thermique), (2) un nœud à **plus basse impédance est plus robuste au bruit RF capacitivement couplé** — argument qui pousse vers un R plus bas, pas plus haut, ce qui est directement pertinent vu le risque radio ci-dessous. D'où le choix de **220kΩ** (valeur standard E24) plutôt que 470-800k : marge confortable, τ encore facilement gérable en firmware (~4µs sec → ~62µs saturé), meilleure immunité EMI.

Point résiduel indépendant du choix de R : au cas le plus sec (~18pF), `tACQ/τ_ext ≈ 2.71` — la fenêtre d'acquisition de l'ADC est mécaniquement plus longue que notre propre constante de temps, donc la lecture en sol très sec sera naturellement un peu "lissée" par l'acquisition elle-même. Pas forcément gênant (peut même aider à moyenner le bruit) tant que la lecture reste monotone avec l'humidité — à vérifier empiriquement.

*Risque non mitigé, à tester sur le vrai prototype :* le même projet Hackaday.io rapporte que sa mesure RC directe était **"comme une antenne"** — bruit RF sévère jusqu'à désactiver Bluetooth/WiFi (mode avion) pour stabiliser les lectures. Notre carte a une **antenne BLE/802.15.4 active empilée directement au-dessus** (le module AN54LQ-15 du breakout, cf. discussion sur le placement du boost) — risque concret de couplage RF sur ce nœud haute-impédance pendant les rafales d'advertising BLE. Pas de mitigation matérielle prévue pour l'instant (pas de garde/blindage autour du nœud RC) — piste de repli firmware si ça pose problème : synchroniser les lectures sol pour éviter les fenêtres de TX radio, ou moyenner davantage d'échantillons.

**Conclusion de cette session de validation : l'approche RC directe reste privilégiée (pas besoin de 555). Actions faites : R3/R4 abaissées de 1MΩ à 220kΩ (schéma régénéré, ERC propre, BOM mise à jour). Action restante : prévoir dans le firmware de test une vérification explicite de la sensibilité au bruit radio (lectures avec/sans BLE actif) puisque c'est le risque le plus concret identifié et non résolu par le calcul seul.**

Sources consultées : [Nordic nRF54L15 Datasheet v1.0](file:///home/ludovic/Documents/Projects/PlantSensor/misc/Nordic_nRF54L15_Datasheet_v1.0.pdf) §11.17 (SAADC Electrical Specification, p.912) ; [Igreja & Dias 2004, analytical IDC capacitance model](https://www.sciencedirect.com/science/article/abs/pii/S0924424704000779) ; formule Mamishev citée via recherche (review "Interdigital Sensors and Transducers", IEEE Proc. 2004) ; [Cave Pearl Project — Hacking a Capacitive Soil Moisture Sensor for Frequency Output](https://thecavepearlproject.org/2020/10/27/hacking-a-capacitive-soil-moisture-sensor-for-frequency-output/) (mesures réelles 30-400pF, 555 TLC555 1.5MHz) ; [Hackaday.io — A cheap capacitive soil moisture sensor](https://hackaday.io/project/12813-a-cheap-capacitive-soil-moisture-sensor) (tentative RC directe, ~14pF insuffisant, bruit RF sévère) ; [Chirp / I2C Soil Moisture Sensor, Catnip Electronics (Tindie)](https://www.tindie.com/products/miceuz/i2c-soil-moisture-sensor/).

### 2.7 Géométrie de la sonde — peigne droit, pas en "U"

Le brief demande une "géométrie interdigitée en U" en référence à un design PCB v1 non présent dans ce repo. J'ai implémenté un **peigne interdigité droit** (2 segments, 10mm de large, doigts 0.3mm / espacement 0.15mm comme demandé), généré par script Python en pads `custom` KiCad (2 pads par segment = 4 pads au total, chacun un ensemble de rectangles cuivre fusionnés). **La forme "U" exacte n'a pas pu être reproduite faute de référence** — à ajuster si cette forme a une justification technique précise (ex. maximiser la longueur de doigt dans un espace contraint).

**Mise à jour taille (2026-08-09) :** sonde agrandie à la demande de l'utilisateur — segments passés de 17mm à **29mm chacun** (60mm de longueur totale, `SEG_GAP`=2mm inchangé). Effet secondaire favorable, vérifié par calcul (formule Mamishev/Igreja-Dias, cf. §2.6) : la capacité augmente avec la longueur de doigt (∝L), donc la plage attendue passe d'environ 18-283pF à **~33-501pF** (sec→saturé), et le ratio `tACQ/τ` au cas le plus sec (le point faible identifié en §2.6) s'améliore de 2.71 à **1.53** — signal plus fort, mesure moins "lissée" par la fenêtre d'acquisition de l'ADC.

**Mise à jour orientation des doigts (2026-08-09, suite à une question directe) :** rotation à 90° — spines maintenant sur les bords **gauche/droite** de chaque segment (au lieu de haut/bas), doigts horizontaux interleavés **en descendant la hauteur** (au lieu de verticaux interleavés en largeur). Raison : une fois la sonde passée à 60mm, l'ancienne orientation donnait des doigts de **27.6mm de long × 0.3mm de large, en porte-à-faux, ancrés à une seule extrémité** — fragile mécaniquement pour une sonde qu'on enfonce physiquement dans le sol (risque de décollement/craquage du cuivre, l'ENIG protège contre la corrosion mais pas contre ce type de contrainte mécanique). Vérifié par calcul que la capacité totale ((N-1)×L) est quasi identique entre les deux orientations (524mm vs 533mm, +1.7%, négligeable — la capacité dépend surtout de l'aire couverte / le pitch, pas de l'axe des doigts) : donc rotation sans compromis électrique. Nouveaux doigts : **8.6mm de long** (limité par la largeur 10mm du probe) au lieu de 27.6mm — **3.2× plus courts**, plus robustes. 63 doigts/segment au lieu de 20 (plus nombreux mais plus courts, `gen_soil_probe_footprint.py` v1.2). Rendu visuellement vérifié (export SVG).

**Mise à jour pitch/largeur — profondeur de pénétration du champ (2026-08-10, suite à une question directe "t'es sûr de la densité des doigts ?") :** le pitch initial (doigt 0.3mm + gap 0.15mm, hérité du brief) donnait une profondeur de pénétration du champ de franges dans le sol d'environ **0.3mm seulement** — calculé via la relation empirique établie dans la littérature IDC : profondeur ≈ λ/3 où λ = 2×pitch (source : ResearchGate, "Optimization of the Coplanar Interdigital Capacitive Sensor" et papiers connexes). À cette échelle, le capteur mesure essentiellement le contact de surface, pas l'humidité réelle du sol à quelques mm. Confirmé par un papier IEEE Sensors Journal peer-reviewed (McIntosh & Casada, 2008, "Fringing Field Capacitance Sensor for Measuring the Moisture Content of Agricultural Commodities") : *"A disadvantage of interdigitated capacitive sensors is the small size and narrow spacing of the electrode fingers: this restricts the region in which a measurement is made to a thin, material layer at the surface of the sensor; errors can occur when measuring granular materials."* — directement applicable au sol (matériau granulaire).

Le "spacing 0.15mm" du brief était vraisemblablement une réflexion initiale sans ce calcul de profondeur en tête (confirmé par l'utilisateur) — pas de PCB v1 de référence retrouvé pour trancher son origine exacte.

**Compromis profondeur vs capacité :** élargir le pitch améliore la profondeur mais réduit le nombre de doigts qui rentrent dans le segment, donc réduit la capacité totale — au-delà de pitch~1.5mm sans compensation, le cas sec retombe sous le seuil ~14pF jugé insuffisant par la référence Hackaday.io (cf. §2.6). Table de compromis calculée (formule Mamishev/Igreja-Dias, probe 10mm large × 29mm/segment) :

| doigt/gap | pitch | profondeur | doigts | C sec | C mouillé |
|---|---|---|---|---|---|
| 0.3/0.15 (original) | 0.45mm | 0.3mm | 63 | 33pF | 175pF |
| 0.75/0.75 | 1.5mm | ~1.0mm | 18 | 7.1pF | 38pF |
| 1.5/1.5 | 3.0mm | ~2.0mm | 9 | 3.3pF | 18pF |

**Décision finale : doigt 0.75mm / gap 0.75mm (pitch 1.5mm, profondeur ~1mm), largeur du probe élargie de 10mm à 18mm** pour compenser la perte de capacité côté cas sec (le levier "largeur" allonge chaque doigt sans jouer contre la profondeur, contrairement au pitch). Résultat avec 18mm de large : **C_sec≈13.7pF, C_mouillé≈73.6pF, C_saturé≈211pF** — proche du seuil de référence Hackaday (~14pF) mais pas en dessous, et avec un R3/R4 et une compréhension SAADC bien meilleurs que cette référence DIY. `gen_soil_probe_footprint.py` mis à jour (v1.2), footprint régénéré et vérifié visuellement.

**Le footprint dans le PCB existant n'est pas synchronisé automatiquement** — comme pour tout changement de footprint pendant que l'utilisateur place à la main, ça attend un "Update PCB from Schematic" (ou "Update Footprint from Library" pour ce composant précis) de son côté. À noter : le probe est maintenant **18mm × 60mm** (bien plus grand que les 10mm × 36mm d'origine) — prévoir de la place en conséquence lors de la synchronisation.

Sources consultées : [McIntosh & Casada 2008, IEEE Sensors Journal — Fringing Field Capacitance Sensor](https://www.ars.usda.gov/ARSUserFiles/30200525/392FringingFieldcapacitancesensor.pdf) ; recherche sur la relation profondeur de pénétration / pitch des IDC (littérature IDC, plusieurs papiers ResearchGate/IOP) ; comparaison géométrie réelle DIY (traces parallèles ~2-3mm d'espacement).

**Correction post-review (suite à une question directe sur la géométrie) :** la première version alternait des **rangées entières** entre les deux peignes (chaque rangée = un seul peigne, un doigt ne couvrant que la moitié de la largeur, avec un espace au milieu). Résultat : les doigts de A et B ne se faisaient jamais face sur une longueur significative — décalés en diagonale, très peu de bord parallèle proche entre eux, donc peu de couplage capacitif effectif malgré l'apparence de "peigne". Corrigé vers la topologie IDC (interdigitated capacitor) standard : barres bus en haut et en bas de chaque segment, doigts **entrelacés côte à côte** en balayant la largeur (A, espace, B, espace, A...), chaque doigt courant sur **presque toute la hauteur du segment** (~15.6mm sur 17mm, juste un dégagement de 0.4mm à la pointe) plutôt que la moitié. Ça donne 20 doigts/segment (10 par peigne) au lieu de 35 (mal répartis), mais avec un vrai couplage par bord parallèle sur ~15mm de longueur par paire de doigts adjacents — c'est ce qui fait fonctionner un capteur capacitif interdigité. Vérifié visuellement après coup (export SVG zoomé sur le footprint).

**Effet de bord découvert en même temps :** la clearance cuivre-cuivre par défaut du board (0.2mm) est plus stricte que l'espacement doigt-à-doigt voulu (0.15mm, imposé par le brief) — DRC le signalait comme erreur. Le script fixe maintenant la clearance par défaut du board à 0.127mm (5mil, minimum standard JLCPCB) pour l'accommoder. À vérifier que 0.127mm convient bien au reste du routage une fois qu'il sera fait (c'est une clearance globale, pas juste pour la sonde).

### 2.8 DIP switch — mapping des paires de broches

EM-04-Q (8 broches). Le mapping switch↔broches (1↔8, 2↔7, 3↔6, 4↔5) a été **déduit directement de la géométrie du symbole** récupéré via easyeda2kicad (les broches de chaque paire partagent la même coordonnée X dans le symbole, reliées par le glyphe "switch" dessiné entre elles) — pas une supposition, une lecture directe des données. Confiance haute, mais gardez en tête que c'est déduit, pas confirmé sur le datasheet texte du composant.

### 2.9 SHT40 → SHT41 (U2)

Remplacement à l'identique décidé avec l'utilisateur : `SHT41-AD1B-R2` (LCSC **C7461861**) au lieu de `SHT40-AD1B-R2` (C2909890). Vérifié sur le datasheet Sensirion SHT4x officiel (v7.1, mars 2025) — **même boîtier DFN-4 1.5×1.5mm, même pinout, même adresse I2C 0x44**, donc zéro impact footprint/layout. Seule différence : précision RH aux extrêmes (0-10% et 90-100%RH) — ±3%RH typ. max pour le SHT40 contre ±2%RH pour le SHT41 sur toute la plage 0-80°C (le point central 20-80%RH reste ±1.8% identique pour les deux). Pertinent pour un capteur de plante exposé à des conditions humides/sèches extrêmes. Symbole dupliqué dans `libs/myco_host.kicad_sym` (`SHT41-AD1B-R2`), `gen_schematic.py` et `bom_jlcpcb.csv` mis à jour, schéma régénéré (PCB non touché, cf. §3.2).

---

## 3. Problèmes non résolus — à trancher avant fabrication

### 3.1 Support CR2032 : traversant (THT), pas SMD

Le seul modèle avec données EasyEDA récupérables était `CR2032-BS-6-1` (C70377), qui est un connecteur **traversant**, pas SMD. Le brief section 8 dit "assemblage manuel (hotplate)" ce qui suggère du SMD pour tout, mais un support batterie THT est en réalité assez facile à souder à la main séparément (pas besoin de hotplate pour 6 grosses pattes). À confirmer si c'est acceptable ou s'il faut chercher un support CR2032 SMD spécifiquement.

### 3.2 ✅ RÉSOLU — XC9145B33CMR-G en SOT-25, hand-solderable

**Historique de l'erreur** : j'avais initialement conclu (sur la base d'un datasheet Torex daté 2011/Rev.D) que le XC9145 n'existait qu'en USP-6C et WLP-6-05 (BGA, non soudable à la main), et traité le part number `XC9145B33CMR-G` du brief comme un probable typo. L'utilisateur a corrigé ce point ("bein ce boost existe au format sot-25") et fourni le lien LCSC direct confirmant l'existence réelle de cette variante — une vraie erreur de recherche de ma part (je m'étais arrêté au premier résultat LCSC "proche" trouvé au lieu de vérifier le part number exact du brief).

**Confirmé** : `XC9145B33CMR-G`, LCSC **C19261414**, boîtier **SOT-25** (2.8×2.9×1.3mm, pattes visibles standard), en stock (1648 unités au moment de la vérification). Pinout confirmé à la fois via easyeda2kicad et via le schéma de référence eval-board Torex officiel (`misc/xc9141_42_45_sot-25.zip`) : CE=1, GND=2, BAT=3, VOUT=4, LX=5.

**Actions faites** :
- Symbole `XC9145B33CMR-G` (5 broches) recréé dans `libs/myco_host.kicad_sym`, remplaçant l'ancien placeholder 6 broches WLP-6-05/USP-6C.
- Footprint réel SOT-25 récupéré via easyeda2kicad, copié dans `libs/myco_host.pretty/`, ancien placeholder `USON-6_XC9145_PLACEHOLDER` supprimé.
- `gen_schematic.py` et `bom_jlcpcb.csv` mis à jour en conséquence. Schéma régénéré, ERC toujours propre (56 violations attendues/bénignes, inchangé).
- **PCB non synchronisé délibérément** : U1 référence encore l'ancien footprint dans `myco-mini-host-pcb.kicad_pcb` tant que l'utilisateur place les composants à la main dans l'éditeur (cf. feedback mémoire — ne jamais relancer `gen_pcb.py` pendant un placement manuel en cours). À synchroniser via "Update PCB from Schematic" côté utilisateur (préserve le placement), ou sur demande explicite.
- Référence de layout disponible : le schéma eval-board Torex (3 caps entrée CBLK/CIN2/CIN1 + 3 caps sortie CL1/CL2/CL3) est plus riche que la config actuelle (1 cap entrée C2 + 2 caps sortie C4/C5), mais correspond au "Typical Application Circuit" du corps du datasheet (caractérisation banc, pas une exigence BOM) — config actuelle conservée, cohérente avec le datasheet principal.
- Le schéma de référence montre aussi un point de test dédié sur le nœud LX (nœud de commutation, le plus bruyant du circuit) — pratique standard eval-board pour sonder à l'oscillo, pas nécessaire électriquement. Non ajouté (optionnel, à la discrétion de l'utilisateur selon la place disponible).

### 3.3 Pins SAADC non confirmés (soil ADC2, VDD interne batterie)

Comme prévu dans le brief : le 2e canal SAADC pour le segment profond (`P1_04_SOIL_ADC2` dans le schéma) est le candidat proposé en premier (parmi P1.04/P1.05/P1.06) mais **pas vérifié contre le datasheet nRF54L15** pour confirmer qu'il est bien SAADC-capable. Idem pour l'existence d'un canal SAADC interne de mesure VDD — je n'ai pas eu le temps de vérifier cette section précise du datasheet (13MB, structure complexe) durant cette session. Le schéma actuel ne câble d'ailleurs **aucune mesure de tension batterie** — ni GPIO dédié, ni diviseur — cette partie du brief (section 3, ligne "Batterie") n'a pas été implémentée du tout dans ce premier jet, faute de réponse tranchée sur le canal interne. À ajouter une fois la question résolue.

### 3.4 CR2032-BS-6-1 : polarité pin1/pin2 non vérifiée

J'ai supposé pin1=BAT+ / pin2=BAT- pour câbler le schéma, sans avoir pu confirmer contre le datasheet/sérigraphie exacte du composant. Vérifier avant assemblage — inverser sinon.

---

## 4. Ce qui n'est PAS fait dans ce premier jet

- **Aucun routage cuivre.** Le PCB contient les footprints placés et le netlist assigné (donc le "ratsnest"/chevelu est correct), mais aucune piste n'est tracée. Le DRC rapporte 59 "unconnected items" — c'est normal et attendu à ce stade, pas un bug.
- **Placement approximatif.** Les positions sont un point de départ raisonnable (pas de chevauchement de courtyard/trous après plusieurs itérations DRC), mais pas optimisées pour la taille finale ni pour la facilité de routage. Le placement est probablement le premier chantier d'itération avec vous.
- **Chevauchements de texte silkscreen** (14 sur le PCB, quelques-uns sur le schéma) — cosmétique, pas électrique, à nettoyer en glissant les textes dans l'éditeur KiCad.
- **Aucune découpe de la sonde en "languette"** — la sonde est un pattern de cuivre sur la surface du PCB principal, pas une languette détachée par fraisage. L'épaisseur zonée 0.8mm sous la sonde (§5 du brief) n'est pas non plus implémentée — nécessite une discussion avec le fabricant sur un stackup zoné, pas quelque chose qu'un script peut décider seul.
- **Titre/cartouche PCB** : la page a une taille custom ("User", 190×115mm) et le cartouche par défaut de KiCad ne s'adapte pas bien visuellement — cosmétique, à corriger dans Pagelayout Editor si voulu.
- **Board outline arbitraire** (190×115mm, rectangle simple) — pas dimensionné pour un boîtier ou une contrainte mécanique réelle, juste assez grand pour loger les composants sans chevauchement.

---

## 5. Vérifications faites

- ERC (`kicad-cli sch erc`) : 0 erreur bloquante. 57 avertissements restants, tous dans deux catégories attendues : broches "unspecified" (caractéristique connue des symboles importés via easyeda2kicad, pas une vraie ambiguïté électrique) et broches de header non utilisées (GPIO libres laissés non câblés délibérément).
- DRC (`kicad-cli pcb drc`) : 0 erreur de chevauchement de courtyard, trou-à-trou, ou court-circuit après plusieurs itérations de placement. 14 avertissements silk_overlap restants (cosmétique). 59 "unconnected items" attendus (rien n'est routé).
- Le footprint sonde sol et le rendu PCB complet ont été exportés en SVG et inspectés visuellement (voir historique de session) pour confirmer l'absence de chevauchement grossier et la géométrie correcte du peigne interdigité.

---

## 6. Note méthodologique — pourquoi généré par script plutôt que dessiné dans l'éditeur

Je n'ai pas d'accès interactif à l'interface graphique KiCad. Le schématique et le PCB ont donc été construits en écrivant directement les fichiers S-expression KiCad (format texte natif), avec un petit générateur Python pour éviter les erreurs de recopie manuelle (positions de pins extraites programmatiquement des bibliothèques plutôt que devinées). La validation s'est faite via `kicad-cli` (ERC, DRC, export SVG) et des rendus rasterisés inspectés visuellement.

**Conséquence pratique :** tout est un fichier KiCad standard, ouvrable/éditable normalement dans l'éditeur — mais le placement, l'espacement des textes, et l'organisation visuelle n'ont pas eu le bénéfice d'un œil humain en temps réel pendant la conception. Attendez-vous à vouloir réorganiser pas mal de choses visuellement dès l'ouverture.
