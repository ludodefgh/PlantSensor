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

### 2.5ter Changement (2026-08-10, fait par l'utilisateur directement dans l'éditeur de schéma) — VDD du SHT41 déplacé de VDD_NRF vers VOUT_3V3

U2 (SHT41) alimenté maintenant par `VOUT_3V3` (sortie boost, switchée) au lieu de `VDD_NRF` (rail toujours actif). `gen_schematic.py` mis à jour pour refléter ce câblage (ligne `stub_and_label(u2["VDD"], ...)`), **mais le fichier `.kicad_sch` n'a pas été régénéré** — l'utilisateur édite maintenant le schéma directement à la main en parallèle du PCB, donc même prudence que pour le placement PCB (cf. mémoire `feedback_pcb_placement`) : je ne relance plus `gen_schematic.py` sans demande explicite.

**Conséquence trouvée — la raison d'être de §2.5 n'existait plus telle quelle :** la justification originale de mettre R1/R2 sur VDD_NRF plutôt que VOUT_3V3 était "le SHT40 est toujours alimenté, indépendant du boost". Plus vrai après ce changement — U2 et U3 sont maintenant **tous les deux** sur VOUT_3V3, plus aucun device I2C sur VDD_NRF. Risque identique à celui déjà corrigé en §2.5bis pour DVI, mais cette fois sur le seuil `VIH` de communication I2C du SHT41 lui-même : pull-ups vers VDD_NRF (sagging en fin de pile) alors que le seuil `VIH` du SHT41 est référencé à son propre VDD (VOUT_3V3, fixe).

**Résolu (2026-08-10) :** R1/R2 déplacées vers VOUT_3V3 (fait par l'utilisateur directement dans l'éditeur de schéma, `gen_schematic.py` mis à jour en miroir, `.kicad_sch` non régénéré — cf. §2.5ter et mémoire feedback). Vérification supplémentaire faite à cette occasion : ce sens de correction est sûr pour les deux capteurs — le pull-up est maintenant sur le rail **fixe** (VOUT_3V3=3.3V) alors que les deux seuils de référence (VIH du SHT41 via son propre VDD, VIH1 du BH1750 via DVI resté sur VDD_NRF) sont tous les deux ≤ ce niveau, donc toujours franchis correctement quel que soit l'état de la pile — contrairement à l'ancien sens (pull-up sur rail sagging, seuil sur rail fixe) qui posait problème. Section §2.5 (titre/texte ci-dessus) obsolète, remplacée par cette résolution.

### 2.5quater (2026-08-11) — DVI du BH1750 déplacée à son tour de VDD_NRF vers VOUT_3V3

Suite à une question directe de l'utilisateur ("pourquoi U3 a un pad vers VOUT_3V3 et un vers VDD_NRF ?"), le résidu historique de §2.5bis/§2.5ter a été nettoyé : DVI passe de `VDD_NRF` à `VOUT_3V3`, donc **sur le même rail que VCC**. Fait par l'utilisateur directement dans l'éditeur de schéma, `gen_schematic.py` mis à jour en miroir (`stub_and_label(u3["DVI"], "VOUT_3V3")`), `.kicad_sch` non régénéré.

Deux bénéfices simultanés :

1. **Calcul de seuil trivial** : DVI et les pull-ups (R1/R2) sont maintenant sur le même rail exact — plus besoin de comparer deux tensions différentes, `VIH1 = 0.7×DVI` est automatiquement cohérent avec ce que les pull-ups tirent, par construction.
2. **Séquencement DVI/VCC résolu** : le point ouvert de §2.5bis (DVI, sur un rail toujours actif, était déjà haute *avant* la mise sous tension de VCC à chaque cycle — alors que le datasheet recommande DVI='L' *après* VCC) disparaît aussi, puisque DVI et VCC sont maintenant sur le **même** rail switché VOUT_3V3 et montent ensemble à chaque activation du boost. Ça correspond aussi à ce que font beaucoup de breakout boards BH1750 du commerce (DVI câblée directement sur VCC, sans réseau RC de reset séparé).

Les deux points ouverts de §2.5bis sont donc résolus par ce changement. Note pour la suite : U3 n'a plus aucune broche sur VDD_NRF — à vérifier si ça a un intérêt de laisser VDD_NRF exister comme rail séparé pour ce capteur ou si c'est devenu un non-sujet maintenant que U2/U3 sont tous deux entièrement sur VOUT_3V3.

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

### 2.10 LED RGB de feedback (D1) sur VOUT_3V3

Ajoutée suite à discussion (budget courant négligeable en rail switché, largement assez de GPIO libres — 15+ disponibles sur J1/J4). Composant : `XL-5050RGBC` (LCSC **C2843868**, boîtier SMD5050-6P, anode commune). **Câblage fait directement dans le `.kicad_sch` live par script chirurgical** (`scripts/add_rgb_led.py`), pas par régénération complète — l'utilisateur édite le schéma à la main, cf. §2.5ter et mémoire feedback. Script exécuté avec succès après un premier essai raté et corrigé en cours de route (bug d'indexation de lignes qui cassait la structure du fichier — détecté avant tout dégât grâce à une sauvegarde préalable + vérification systématique post-écriture : équilibre des parenthèses, diff pur, re-parsing, ERC, rendu visuel). Fichier final validé sain sur tous ces points.

**Câblage** : anode commune (pins 2/4/6) → `VOUT_3V3`. Cathodes R/G/B (pins 3/1/5) → résistances série R7 (220Ω)/R8 (330Ω)/R9 (330Ω) → GPIO libres `P1.09`/`P1.10`/`P1.11` (J1 pins 2/3/4).

**✅ Pinout corrigé (2026-08-10) — l'utilisateur a fourni la vraie datasheet Xinglight** (`misc/c7b4bb8841da5973ba31f9cd1a1b23d8.pdf`). Le pinout assumé initialement était **faux** : ce n'est pas une LED à anode commune, ce sont **3 LED indépendantes, chacune avec sa propre anode ET sa propre cathode**, aucune broche partagée en interne :
- G : anode=broche4, cathode=broche3
- R : anode=broche5, cathode=broche2
- B : anode=broche6, cathode=broche1

Mon hypothèse initiale ("1=G,2=Anode,3=R,4=Anode,5=B,6=Anode") avait les broches 2 et 5 complètement inversées (anode ↔ cathode) — si fabriqué tel quel, ça aurait câblé une cathode directement sur VOUT_3V3 sans résistance et une anode à travers une résistance vers un GPIO, un court-circuit de fait. Le brochage physique réel est aussi différent : 3 pads sur le bord **gauche** + 3 sur le bord **droit** (pas haut/bas comme construit initialement).

**Corrections appliquées** : symbole (`myco_host.kicad_sym`) et footprint (`LED_SMD5050-6P.kicad_mod`) refaits avec le vrai mapping. Dans le `.kicad_sch` live, comme seuls les *noms* de broches avaient changé (pas leur position physique dans le symbole), les fils existants pointaient toujours sur le bon numéro de broche mais avec le mauvais net attaché — corrigé chirurgicalement : 4 labels retextés aux coordonnées exactes de chaque broche concernée (broches 1,2,3,5 ; les broches 4 et 6 étaient déjà correctes par coïncidence, toutes deux anodes → VOUT_3V3). La copie du symbole embarquée dans le `.kicad_sch` (section `lib_symbols`, distincte du fichier librairie) a aussi dû être resynchronisée manuellement — sinon KiCad l'aurait affichée avec les anciens noms de broches malgré le câblage corrigé. Revalidé après coup : équilibre des parenthèses, re-parsing avec confirmation du mapping broche→net, ERC (retour à 59 violations de la même nature qu'avant, aucune nouvelle).

**Vf réels (datasheet Xinglight, IF=20mA) :** R min 1.9V/max 2.3V ; G et B min 2.8V/max 3.4V (légèrement meilleur que l'estimation générique initiale de 3.6V max). Sur VOUT_3V3=3.3V :
- Rouge : marge +1.0 à +1.4V — OK, courant 4-7mA avec R7=220Ω.
- Vert/Bleu : marge +0.5V à **-0.1V** selon le binning réel (table de gradation Xinglight : N13-3=2.8-3.0V, N13-4=3.0-3.2V, N13-5=3.2-3.4V). **Toujours un risque réel** dans le binning le plus défavorable (N13-5, Vf jusqu'à 3.4V) — marge quasi nulle voire négative, la LED pourrait ne pas s'allumer ou rester très faible. Moins dramatique qu'estimé initialement mais **non résolu** — à vérifier à la mise sous tension réelle. Fallback identique si problème confirmé : LED avec binning Vf plus bas, ou accepter la limitation (rouge fonctionnera dans tous les cas).

### 2.11 Découpe thermique autour de U2 (SHT41) et symétrisation du contour de la carte

**Découpe thermique** : ajoutée sur demande, justifiée par le guide officiel Sensirion "Design Guide for Humidity and Temperature Sensors" (v2, mars 2024) — citation clé : à 90%RH, 1°C d'écart de température donne 5%RH d'écart sur la lecture d'humidité ; la conduction thermique via le PCB depuis les composants voisins est explicitement citée comme la source la plus courante et sévère de ces écarts, corrigée par des fentes usinées ("milled slits") autour du capteur. Le compromis "isolation vs temps de réponse" mentionné dans leur guide concerne uniquement les capteurs analogiques — le SHT41 est digital I2C, non concerné, donc l'isolation est un gain pur ici sans contrepartie.

Géométrie : 3 fentes rectangulaires fermées (haut/bas/gauche, 0.8mm de large), ~0.5mm de dégagement des vraies pastilles cuivre de U2 (pas juste le courtyard), côté droit laissé plein comme pont mécanique/de routage. **Point technique important découvert en le faisant** : KiCad n'accepte pas une fente "ouverte" (une simple polyligne en U sur Edge.Cuts) — toute géométrie sur ce calque doit faire partie d'un contour fermé, sinon DRC lève "malformed outline (not a closed shape)". La solution standard (et plus proche de la réalité d'un usinage) est 3 rectangles fermés séparés, avec de petits ponts aux coins entre eux plutôt qu'une fente continue. Règle de conception custom (`.kicad_dru`) mise à jour pour exclure U2 de la règle "courtyard to board edge" (comme J1/J2/PROBE1 déjà exclus) puisque cette proximité est ici intentionnelle.

**Symétrisation du contour** : l'utilisateur avait dessiné un contour hexagonal/fuselé à la main dans l'éditeur PCB sans réussir à le rendre symétrique par rapport à l'axe vertical. Axe de référence choisi : centre de J1/J2 (X=77.75mm, seule référence géométriquement significative et fixe du design, puisque ces headers doivent s'aligner avec le breakout). **Premier essai raté** : moyenne des demi-largeurs gauche/droite de chaque paire de points — a rétréci le contour par endroits, découpant dans des composants déjà placés (U3, D1, R7-R9) qui s'appuyaient sur l'ancien contour asymétrique. Restauré depuis sauvegarde, recalculé correctement : demi-largeur par palier = distance max nécessaire pour dégager tous les composants réels de ce palier (bounding box complet, pas les anciens points du contour) + marge de 1.5mm, vérifié programmatiquement (test de confinement point-dans-polygone pour les 4 coins de chaque empreinte) avant d'appliquer quoi que ce soit. Résultat : contour visuellement symétrique, DRC repassé de 68 violations (dont plusieurs `copper_edge_clearance` réelles après le premier essai raté) à 39 (le bruit habituel silk/via, rien de nouveau).

**Méthode/outillage** : contrairement au schéma (édité par script chirurgical sur texte S-expression), le PCB a été édité directement via l'API `pcbnew` (charger le board, modifier les objets géométriques, `board.Save()`) — plus fiable ici puisque c'est le moteur natif du format `.kicad_pcb`, contrairement à `sexpr.py` qui est un parseur maison pour le schéma. Diff vérifié après coup pour confirmer qu'aucune autre partie du fichier n'a été touchée (~30-35 lignes ajoutées à chaque étape, tout le reste identique caractère pour caractère).

### 2.12 C6 (100µF) — fix issue GitHub #18, creux transitoire VDD_NRF en TX radio

Ajouté suite à l'issue [#18](https://github.com/ludodefgh/PlantSensor/issues/18) (diagnostic vérifié contre le vrai datasheet nRF54L15 : le courant TX à +4dBm cité, 6.6mA, interpole correctement entre les points connus du tableau §11.13.2 (0dBm=3.7mA) et §11.13.4 (max QFN=+7dBm→9.1mA) — cohérent). Le risque : l'ESR de la CR2032 grimpe fortement en fin de vie/au froid (15-20Ω neuve → potentiellement plusieurs centaines d'Ω), un pic de courant TX à travers cette résistance peut créer un creux transitoire de 1-2V sur VDD_NRF, largement suffisant pour déclencher un brownout (seuil nRF54L15 ~1.56-1.64V) même si la tension "au repos" semblait correcte.

**Décision : condensateur séparé, pas remplacement de C3.** C3 (100nF) et un gros réservoir (100µF+) ne jouent pas le même rôle en fréquence — un MLCC de forte capacité a sa résonance propre à basse fréquence (bon pour absorber un creux lent de plusieurs centaines de µs) alors qu'un petit 100nF résonne plus haut (bon pour le bruit HF/edges numériques rapides) ; au-delà de sa propre résonance, chaque condensateur redevient un mauvais filtre (comportement inductif). Remplacer C3 par 100µF aurait donc perdu le filtrage HF qu'il assurait. Vérification de dimensionnement (indépendante de l'issue) : pour un burst TX de ~0.5-2ms à ~10mA avec un budget de creux de ~150-200mV, C ≈ 25-100µF — cohérent avec la fourchette proposée.

C6 = 100µF, footprint `C_0805_2012Metric` (comme C1/C2/C4/C5), placé électriquement en parallèle de C3 sur `VDD_NRF`/GND. Ajouté par script chirurgical sur le `.kicad_sch` live (`scripts/add_c6.py`, même méthode que pour D1/R7-R9), validé : équilibre des parenthèses, diff pur (0 ligne supprimée), re-parsing confirmant la présence de C6, ERC inchangé (59 violations, aucune nouvelle — `Device:C` a un type de pin "passive" correct, contrairement aux symboles custom "unspecified" de ce projet, donc zéro bruit ERC supplémentaire).

**Mise à jour (2026-08-11) :** l'utilisateur a synchronisé le PCB de son côté (Update PCB from Schematic ou équivalent) et placé C6 physiquement près de C3. DRC signale un `courtyards_overlap` mineur entre C6 et C3 (0.35mm vs 0.5mm requis par la règle "hand assembly clearance") — pas corrigé, c'est un placement manuel de l'utilisateur, pas à moi d'y toucher sans demande.

### 2.13 D1/R7/R8/R9 déplacés côté J2 (2026-08-11) — remap GPIO pour faciliter le routage

Sur demande utilisateur : la LED RGB et ses résistances série, initialement câblées sur des GPIO de J1 (P1.09/P1.10/P1.11), déplacées côté J2 pour se rapprocher physiquement de ce connecteur. Nouveaux GPIO choisis parmi les broches libres de J2 (J4 dans la nomenclature du brief) : **P2.04→R (pin13), P2.05→G (pin12), P2.06→B (pin11)** — confirmés libres (aucun autre usage sur le schéma avant ce changement), broches physiquement adjacentes sur le connecteur pour un routage propre.

**Schéma** : les 3 anciens stubs+labels J1 (pins 2/3/4, `P1_09/10/11_LED_X`) supprimés ; les labels côté résistances (R7.2/R8.2/R9.2) retextés vers les nouveaux noms de net ; 3 nouveaux stubs+labels ajoutés sur J2 (pins 13/12/11). Édité chirurgicalement sur le `.kicad_sch` live (suppression de blocs identifiés par scan de profondeur de parenthèses + insertion, dans un seul script). Validé : équilibre des parenthèses, re-parsing confirmant le nouveau netlist, ERC inchangé (59 violations, même composition qu'avant — le remap ne change ni n'ajoute de bruit).

**PCB** : D1/R7/R8/R9 déplacés physiquement près de J2 (empreinte réelle des pads de J2 utilisée pour trouver l'espace libre, pas la bounding box gonflée par le texte de sérigraphie — même piège que pour U2 en §2.11). Nets GPIO des pads réassignés directement via l'API `pcbnew` (`pad.SetNet(...)`) en même temps que le déplacement. Placement vérifié programmatiquement avant application (confinement dans le contour de carte + non-chevauchement avec les autres empreintes + non-chevauchement mutuel entre D1/R7/R8/R9 eux-mêmes — ce dernier point a d'abord échoué avec un pas de 2mm entre R7/R8/R9, corrigé à 2.51mm comme dans le placement d'origine).

**Point important découvert en cours de route :** un plan de masse GND (9 zones au total) existe maintenant sur le board — ajouté par l'utilisateur depuis la dernière fois que j'avais vérifié (à un moment antérieur de cette session, aucun pour n'existait). Déplacer des footprints ne recalcule pas automatiquement le remplissage des zones existantes : le fill était resté figé à l'ancien état, donc les nouveaux emplacements de D1/R7/R8/R9 se sont retrouvés sans dégagement anti-pad, provoquant des violations `solder_mask_bridge` et `clearance` à 0.0mm. Corrigé avec `pcbnew.ZONE_FILLER(board).Fill(board.Zones())` puis sauvegarde — DRC repassé de 66 à 42 violations (redescendu aux catégories habituelles bénignes + le `courtyards_overlap` C6/C3 préexistant, sans lien avec ce changement).

### 2.14 I2C (SDA/SCL) déplacé de J2 vers J1 (2026-08-11) — remap GPIO #2, avec vrai routage cette fois

Même logique que §2.13 : U2/U3 (les deux capteurs I2C) sont physiquement proches de J1, mais le bus SDA/SCL sortait vers J2 (P0.02/P0.03) — long trajet de piste inutile. **Contrainte matérielle réelle vérifiée avant de agir** : J1 n'expose aucune broche P0.xx (uniquement P1.xx/P2.xx partiel + alim/masse) — impossible de garder SDA/SCL sur P0.02/P0.03 tout en sortant par J1. Solution : reroutage du périphérique TWIM du nRF54L15 vers **P1.10 (J1 pin3, SDA) et P1.11 (J1 pin4, SCL)** — confirmé possible par le datasheet nRF54L15 §8.23.8 ("The SCL and SDA signals are mapped to physical pins using the PSEL.SCL and PSEL.SDA registers", donc n'importe quel GPIO convient, pas de table de fonctions alternatives fixes comme sur d'autres familles de MCU). **Conséquence firmware à ne pas oublier** : l'overlay Zephyr (devicetree, bus `i2c22` ou équivalent) devra pointer vers ces nouvelles broches — pas fait dans cette session (hors scope PCB), mais sans incidence nouvelle puisque ce board a déjà son propre overlay différent de la dev board Phase 2 XIAO (cf. mémoire projet).

**Découverte cruciale en cours de route : du vrai routage cuivre existait déjà.** Contrairement à l'hypothèse de travail initiale ("aucune piste tracée", cf. §4), l'utilisateur avait déjà routé ~10mm de pistes réelles entre U2/U3 et J2 pour ce bus. Renommer juste le net des pads (comme en §2.13) aurait laissé les pads sur le nouveau net et les pistes existantes sur l'ancien — DRC a immédiatement signalé des courts-circuits (`shorting_items`). **Premier réflexe correct : ne pas forcer, informer l'utilisateur et proposer des options** plutôt que de supprimer son travail de routage sans le dire. Après son accord, tentative de reroutage moi-même (première fois sur ce projet) :

- Supprimé uniquement le "tronc" sortant vers J2 (2 segments par net), gardé intact l'interconnexion locale U2↔U3 déjà routée.
- Nouveau tracé SDA : direct sur F.Cu jusqu'à J1 pin3.
- Nouveau tracé SCL : **via + tronçon sur B.Cu** jusqu'à J1 pin4 — nécessaire car les deux nets se croisent géométriquement entre le point de jonction et J1 (SDA doit monter, SCL doit descendre, sur la même plage de X) ; les faire cohabiter sur la même couche aurait shorté les deux nets. Vérifié par calcul avant de router (Y de départ/arrivée de chaque net, confirmation qu'un croisement était réellement inévitable) plutôt que découvert après coup.
- Oubli initial corrigé en cours de route : le net des **pads J1 pin3/pin4 eux-mêmes** doit aussi être réassigné (pas juste les pads côté U2/U3/R1/R2) — sinon la piste touche physiquement le pad mais DRC les voit comme deux nets différents (le pad THT non assigné garde un net `unconnected-(...)` généré automatiquement par KiCad).
- Un script combinant suppression de pistes + réassignation de nets + ajout de nouvelles pistes dans le **même** processus Python a planté (`AttributeError` sur un objet retourné mal typé par `FindFootprintByReference` après des `board.Remove()` sur des pistes) — reproductible avec les bindings pcbnew, probablement un problème de cycle de vie d'objet SWIG. **Corrigé en séparant chaque catégorie d'opération dans son propre processus `python3`** (charger le board, faire une seule chose, sauvegarder, sortir) — plus verbeux mais fiable, à refaire comme ça si un futur script mélange suppression+création+requête sur le même board en une seule exécution.

DRC final : 42 violations, mêmes catégories bénignes qu'ailleurs dans ce document (silkscreen, vias en attente de routage), plus les 2 `courtyards_overlap` préexistants sans rapport. Vérifié aussi visuellement (export SVG) : piste SDA (rouge, F.Cu) et piste SCL (bleue, B.Cu) se croisent sans se toucher, comme prévu.

**Correction de §4 (documentée là-bas aussi) :** l'affirmation "aucun routage cuivre" n'est plus vraie depuis que l'utilisateur a commencé à router ce bus I2C — mise à jour pour refléter l'état réel (routage partiel, pas nul).

### 2.15 Corrections du second audit (2026-08-23) — Q1 mal orienté, découplage, garde-fous

Second audit indépendant (agent Opus 5) après le remaniement de l'alimentation. Il a confirmé comme réellement corrigés : les 4 règles JLCPCB, l'inversion SDA/SCL du BH1750, et les deux points de routage de la sonde sol (traités par l'utilisateur : SEG2 déplacée sur B.Cu hors de la zone d'électrodes de SEG1, plan de masse arrière retiré sous la lame). Il a aussi trouvé plusieurs problèmes nouveaux, dont un critique.

**🔴 Q1 était orienté à l'envers — aucune protection contre l'inversion de polarité.** Câblage initial : `S=BAT_RAW, D=VDD_PROT, G=GND`. Sur un MOSFET canal P, la diode de corps a **anode=Drain, cathode=Source** — vérifié sur le symbole du projet (le triangle pointe vers la Source) et cohérent avec le brochage réel AO3401A (pin1=G, pin2=S, pin3=D, confirmé datasheet). Avec la Source côté pile, une pile insérée à l'envers (BAT_RAW ≈ −3V) laisse la diode de corps **passante** (anode côté VDD_PROT ≈ 0V > cathode à −3V) : le courant remonte de la masse système vers la pile, probablement via la diode ESD interne de U4, qui verrait ≈ −2.3V pour un maximum absolu de −0.3V.

Le bon repère, contre-intuitif mais décisif : **dans un montage correct, la diode de corps conduit en fonctionnement NORMAL** (c'est elle qui amorce l'alimentation de la charge, le canal prend ensuite le relais et court-circuite ses 0.7V) ; elle ne se bloque qu'à l'inversion. Ici c'était exactement l'inverse. Correction : `D=BAT_RAW, S=VDD_PROT`, grille inchangée sur GND — réalisé en échangeant les deux labels des stubs de Q1, aucune modification de géométrie.

**Cause racine à retenir :** un *load switch* (Q2/Q3/Q4) veut la Source **côté alimentation** — c'est correct pour eux, et c'est précisément ce qui fait que leur diode de corps bloque quand ils sont OFF. Une protection d'inversion veut la Source **côté charge**. Les deux montages sont visuellement identiques et opposés fonctionnellement. Q1 avait hérité du motif des load switches. Le brief §4 contient la même erreur ("Source → CR2032+"), ce qui explique que le premier audit soit passé à côté : il validait la conformité au brief, pas la physique.

**Autres corrections appliquées :**

- **C7/C8 (100nF) + C9 (1µF) sur SENSOR_3V3.** Le rail commuté par Q3 n'avait **aucune** capacité locale : tout le découplage (C3/C4/C5/C6) était resté en amont sur VOUT_3V3. Or ROHM spécifie 0.1µF sur le VCC du BH1750 et Sensirion liste le découplage comme obligatoire pour le SHT4x — et ce rail se recharge depuis zéro à chaque cycle de mesure.
- **R13 (100k) sur P1_12_SENSOR_EN → VOUT_3V3.** Les grilles de Q3/Q4 flottaient au démarrage : le boost étant désormais toujours actif, VOUT_3V3 est vivant dès l'insertion de la pile alors que la GPIO du nRF est encore en haute impédance. R5 joue déjà ce rôle pour Q2 ; l'ancienne R6 le jouait pour CE avant d'être supprimée.
- **Noms LED rouge/vert corrigés.** `P2_04_LED_R` atteignait en réalité D1.3 (puce verte) et `P2_05_LED_G` atteignait D1.2 (puce rouge). Les **valeurs** des résistances série étaient déjà correctes par couleur (220Ω sur le rouge) — seuls les noms de net étaient croisés, donc renommés en `P2_04_LED_G` / `P2_05_LED_R`.
- **`VDD_NRF` renommé `BOOST_IN`.** Depuis que J1 pin9 est passé sur VOUT_3V3, ce net n'alimente plus du tout le nRF54 : c'est uniquement le rail d'entrée du boost (C2, L1, U1.BAT, U1.CE, U4.OUT). Le nom induisait en erreur — l'auditeur s'y est d'ailleurs repris à deux fois pour reconstituer la vraie chaîne d'alimentation.
- **Stubs I2C orphelins supprimés** (`P0_02_SDA` / `P0_03_SCL` sur J2 pins 3/4), reliquats du déplacement de §2.14, remplacés par des drapeaux no-connect comme les autres broches libres.
- **Sérigraphie de D1 corrigée dans la librairie.** Les lignes de contour étaient à x=±2.5 alors que les pads s'étendent de ±1.55 à ±2.85 : la sérigraphie traversait ses propres pads. L'instance sur le PCB avait déjà la version correcte (±3.4) — c'est la **librairie** qui était fausse, d'où l'avertissement DRC `lib_footprint_mismatch`. **Piège évité de justesse :** re-synchroniser D1 depuis la librairie pour récupérer son modèle 3D aurait régressé la sérigraphie sur les pads. Librairie corrigée en premier.
- **BOM resynchronisée** : R6 fantôme retirée, commentaire R11/R12 corrigé (`P1_13_BAT_SENSE`, J1 pin6), C7/C8/C9/R13 ajoutés. Vérifiée programmatiquement : 37 composants au schéma = 37 à la BOM, aucun manquant ni fantôme.

**Faux positif du premier audit, corrigé :** le "courtyard manquant sur D1" n'existait pas. Le script de vérification comparait `GetLayerName()` à `'F.CrtYd'`/`'F.SilkS'` alors que l'API pcbnew retourne les noms longs `'F.Courtyard'`/`'F.Silkscreen'`. En "corrigeant" ce faux problème, un courtyard dupliqué **et mal transformé** (coordonnées locales du footprint écrites telles quelles, donc positionnées près de l'origine du PCB) avait été ajouté — détecté avant publication et annulé. À retenir : toujours vérifier qu'un "élément manquant" l'est vraiment avant de l'ajouter, surtout quand le test repose sur une comparaison de chaînes issue d'une API.

**Points laissés à l'utilisateur (décisions, pas corrections) :** pads thermiques de U2/U3 (schéma dit GND, PCB dit sans net — 2 `unconnected` en DRC ; Sensirion déconseille de souder celui du SHT4x, donc c'est peut-être intentionnel, mais le schéma doit alors être aligné) ; résistance ≈100kΩ sur ADDR du BH1750 (mitigation explicite de ROHM quand aucune impulsion basse de 1µs n'est appliquée sur DVI) ; discipline firmware consistant à mettre P1.10/P1.11 en entrée déconnectée quand SENSOR_EN est désactivé, sous peine de fantôme-alimenter les capteurs éteints via leurs diodes ESD I2C.

**État après corrections :** ERC 88 → 71 (2 erreurs restantes = pattes redondantes de SW2, intentionnelles), DRC **0 violation** (contre 1 avant), 2 `unconnected` = les pads thermiques ci-dessus. Les 14 avertissements "off grid" restants proviennent de l'édition manuelle de la zone SW1 par l'utilisateur, pas des corrections.

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

- **Routage cuivre très partiel, pas nul.** ~~Aucune piste n'est tracée~~ (obsolète depuis 2026-08-11) : l'utilisateur a commencé à router certains bus à la main (I2C SDA/SCL entre U2/U3), et §2.14 documente un reroutage de ce même bus vers J1. La grande majorité du board reste au stade ratsnest/netlist assigné sans piste — le DRC continue de rapporter des "unconnected items" (15 au dernier comptage) pour tout le reste, normal à ce stade.
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
