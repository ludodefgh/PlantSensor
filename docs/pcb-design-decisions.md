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

R1/R2 (4.7k, valeur standard non spécifiée dans le brief) tirent SDA/SCL vers **VDD_NRF** et non vers VOUT_3V3. Raison : le SHT40 est "toujours alimenté" (direct CR2032) alors que le BH1750 dépend du boost activé. Si les pull-ups étaient sur VOUT_3V3, le bus I2C n'aurait pas de tension de tirage quand le boost est éteint — cassant la communication avec le SHT40 dans ce mode, qui est justement censé fonctionner indépendamment du boost.

### 2.6 Topologie du front-end analogique de la sonde sol — **interprétation, pas une spec du brief**

Le brief dit seulement : *"chaque segment a sa propre paire de traces de retour, routées séparément vers 2 pins SAADC-capables distincts"* — sans donner le circuit exact. J'ai implémenté un pont diviseur RC classique par segment :

```
SOIL_VCC (switché par Q2/P1.14) ──[R_bias 1MΩ]──┬── peigne A (comb A) ── (diélectrique = sol) ── peigne B (comb B) ── GND
                                                  └── vers pin SAADC
```

R3/R4 = 1MΩ, valeur choisie arbitrairement pour une constante de temps RC raisonnable avec une capacité de l'ordre du pF — **non validée empiriquement**, à ajuster une fois le vrai capteur testé (c'est explicitement l'objectif de ce PCB de proto). Si le vrai front-end prévu est différent (oscillateur 555, CVD, etc.), tout ce bloc est à refaire.

### 2.7 Géométrie de la sonde — peigne droit, pas en "U"

Le brief demande une "géométrie interdigitée en U" en référence à un design PCB v1 non présent dans ce repo. J'ai implémenté un **peigne interdigité droit** (2 segments de 17mm de haut, 10mm de large, doigts 0.3mm / espacement 0.15mm comme demandé, ~35 doigts par segment), généré par script Python en pads `custom` KiCad (2 pads par segment = 4 pads au total, chacun un ensemble de rectangles cuivre fusionnés). Rendu vérifié visuellement (voir capture pendant la session). **La forme "U" exacte n'a pas pu être reproduite faute de référence** — à ajuster si cette forme a une justification technique précise (ex. maximiser la longueur de doigt dans un espace contraint).

### 2.8 DIP switch — mapping des paires de broches

EM-04-Q (8 broches). Le mapping switch↔broches (1↔8, 2↔7, 3↔6, 4↔5) a été **déduit directement de la géométrie du symbole** récupéré via easyeda2kicad (les broches de chaque paire partagent la même coordonnée X dans le symbole, reliées par le glyphe "switch" dessiné entre elles) — pas une supposition, une lecture directe des données. Confiance haute, mais gardez en tête que c'est déduit, pas confirmé sur le datasheet texte du composant.

---

## 3. Problèmes non résolus — à trancher avant fabrication

### 3.1 Support CR2032 : traversant (THT), pas SMD

Le seul modèle avec données EasyEDA récupérables était `CR2032-BS-6-1` (C70377), qui est un connecteur **traversant**, pas SMD. Le brief section 8 dit "assemblage manuel (hotplate)" ce qui suggère du SMD pour tout, mais un support batterie THT est en réalité assez facile à souder à la main séparément (pas besoin de hotplate pour 6 grosses pattes). À confirmer si c'est acceptable ou s'il faut chercher un support CR2032 SMD spécifiquement.

### 3.2 ⚠️ XC9145B33C0R-G — package non compatible avec l'assemblage manuel prévu

**C'est le problème le plus sérieux de ce brouillon.** J'ai lu le datasheet Torex directement (pas juste des snippets de recherche) : le XC9145 série n'existe **que** en USP-6C (1.8×2.0×0.6mm, QFN-like avec pad exposé) et WLP-6-05 (1.08×1.28×0.4mm, **billes de soudure BGA, pas de pattes accessibles**). Le seul des deux disponible en stock JLCPCB/LCSC est la variante WLP-6-05 (C6052816) — **celle qui n'est justement pas soudable à la main via pâte + hotplate** (contrairement au DFN-4 du SHT40, qui a au moins des pads plats en bordure).

Le footprint utilisé dans ce brouillon (`USON-6_XC9145_PLACEHOLDER`) est un **placeholder générique**, pas le vrai land pattern WLP-6-05 — je n'avais pas les cotes mécaniques exactes du vrai footprint BGA.

**Options à trancher :**
1. Sourcer la variante USP-6C ailleurs (Mouser/Digikey/Torex direct, pas JLCPCB) et l'assembler à la main quand même — plus gros que le WLP mais toujours petit.
2. Sous-traiter uniquement ce composant à l'assemblage JLCPCB (SMT partiel) tout en soudant le reste à la main.
3. Remplacer le XC9145 par un autre boost converter avec les mêmes specs clés (ultra-low IQ, true load disconnect, EN direct GPIO) mais disponible en SOT-23-5/SOT-89-5 — plus facile à souder à la main.

Je n'ai pas tranché ce point — c'est explicitement le genre de décision que le brief demande de signaler plutôt que supposer.

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
