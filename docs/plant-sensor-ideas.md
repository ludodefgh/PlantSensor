# 🌱 Plant Sensor Ideas — Idées & Développements Futurs

> Journal de session : idées explorées, décisions prises/en attente, pistes futures.
> Pour les décisions **définitives** et le design de référence, voir les documents canoniques :
> `myco-hardware-design.md`, `myco-decision-log.md`, `myco-roadmap.md`.
> Dernière mise à jour : Août 2026

---

## 📅 Session Août 2026 — Reprise du projet, breakout nRF54L15, PCB host

### Contexte de reprise

Après quelques mois de pause, retour au projet avec deux développements matériels :
- Un **breakout board custom** pour le nRF54L15 en cours de livraison (`ludodefgh/an54lq-15-breakout`), qui règle proprement l'alimentation du module (contrairement au XIAO Seeed).
- Une **hotplate de soudage** en cours de livraison — change le calcul de faisabilité pour souder des composants nus (bare-die) à la main.

### ⚠️ Point ouvert — AN54LQ-15 vs AN54LV-15

Le breakout utilise le **AN54LQ-15** (13.7×9.5×1.8mm), alors que `DEC-002` du decision log avait retenu le **AN54LV-15** (5.9×6.4mm) pour sa compacité. Décision de l'utilisateur : c'est correct de partir avec le LQ pour l'instant ("pas beaucoup plus gros"), ajustement possible plus tard selon prix/disponibilité/expérience hotplate.

**Statut : point ouvert, pas encore tranché pour PCB v1 final.** À trancher avant la commande PCBA JLCPCB de production (impact boîtier + coût).

### Breakout board — interface retenue

Source : `INTEGRATION.md` du repo `an54lq-15-breakout`. Le breakout expose tout le module sur 2 headers mâles 1×17 (2.54mm pitch, espacement rangs 25.4mm exact) + un header 1×5 dédié SWD/alim (non utilisé sur ce host — flash prévu directement depuis le breakout).

Déjà géré sur le breakout (à ne PAS dupliquer côté host) :
- Découplage complet VDD_NRF (4.7µF + 1µF + 0.1µF)
- DC/DC converter mode actif (plus efficace que LDO)
- LFXO 32.768kHz déjà câblé en interne — ne pas en ajouter un deuxième

### PCB host — décisions de session

**Rôle du board :** (1) recevoir le breakout en socket, (2) servir de banc de validation pour les traces capacitives sol, (3) valider les footprints bare-die SHT40/BH1750 avant commande PCBA.

**Connecteur breakout :** 2× headers **femelles** 1×17, 2.54mm, espacement 25.4mm — socketé plutôt que soudé en dur, pour rester swappable pendant les itérations. Pas de header J5 (SWD) côté host.

**Capteurs SHT40/BH1750 :** bare-die (SHT40-AD1B, BH1750FVI-TR) soudés à la hotplate plutôt que dev boards — decision prise pour valider directement les footprints qui iront sur PCB v1/JLCPCB PCBA, pas juste par économie de coût. Pâte à souder appliquée manuellement (seringue/cure-dent), pas besoin de stencil pour 2 composants.

**Mesure batterie :** MCU en connexion directe CR2032 (comme Proto B) — `VDD_NRF` **est** la tension pile. À vérifier : le nRF54L15 a-t-il un canal SAADC interne de mesure VDD (comme les séries nRF52/53) ? Si oui, lecture batterie sans GPIO ni diviseur dédié. Si non, fallback diviseur résistif classique — mais attention au courant de fuite continu d'un diviseur non commuté.

**Protection inverse polarité (GitHub issue déjà ouvert, jamais implémenté) :** P-MOSFET côté haut (AO3401, déjà utilisé ailleurs dans le projet) en série sur VBAT+, gate à GND. Chute de tension négligeable (Iq × RDS(on)) vs 0.3–0.5V d'une diode Schottky série — important vu la marge de tension serrée avec CR2032 direct.

**DIP switches (nouveau — pour test de comportements firmware) :** bloc 4 positions sur `P2.00`–`P2.03`, pull-up interne (pas de résistances externes), lu au boot. Usage envisagé : activer/désactiver pairing, sélection protocole (voir point suivant).

### 🔍 Découverte importante — le nRF54L15 supporte 802.15.4 (Zigbee/Thread) nativement

Recherche confirmée : le radio 2.4GHz du nRF54L15 est **multiprotocole** — Bluetooth LE **et** 802.15.4 (Thread, Zigbee, Matter), pas juste BLE. Un seul radio, bascule entre protocoles (pas de fonctionnement simultané), mais pour un capteur en broadcast périodique ce n'est probablement pas limitant.

**Impact direct :** ça contredit `DEC-004` du decision log ("nRF54L15 BLE only — not suitable for Pro"), qui justifiait le choix EFR32MG21 pour Myco Pro. Si un seul MCU peut couvrir Mini (BLE) et Pro (Zigbee/Thread), la question ouverte "PCB partagé Mini/Pro" (actuellement loggée à "probabilité faible") mérite d'être rouverte — la différence Mini/Pro se réduirait à l'alimentation (CR2032 vs LiPo+USB-C) plutôt qu'au choix de silicium.

**Action à faire :** mettre à jour `DEC-004` dans le decision log pour refléter cette découverte, et rouvrir la question "PCB partagé" dans la roadmap Phase 4 avec un niveau de confiance revu à la hausse.

### Idée explorée — capteur capacitif sol segmenté (mesure par profondeur)

Question posée : ajouter des points de mesure intermédiaires sur la trace capacitive pour obtenir une lecture par profondeur (surface vs racines profondes).

**Conclusion technique :** un simple "tap" le long d'une seule paire de doigts interdigités ne fonctionne pas — la capacité mesurée à un point donné reflète tout le réseau en parallèle, pas un segment local. Pour une vraie résolution en profondeur, il faut **segmenter physiquement l'électrode** : plusieurs zones interdigitées distinctes empilées verticalement, chacune avec sa propre paire de traces de retour, lues indépendamment (soit canaux séparés, soit mux analogique type CD4051).

**Valeur produit potentielle :** détecter si l'arrosage atteint les racines profondes vs juste la surface — différenciateur réel, absent des solutions BTHome/HA existantes.

**Plan de test proposé :** commencer avec **2 segments seulement** sur ce PCB host (pas de mux, juste 2 paires de traces vers 2 canaux SAADC séparés — pins disponibles en abondance sur le breakout). Si les lectures se différencient de façon cohérente en test réel, ça valide l'approche pour PCB v1 ; sinon, retour à une zone unique sans perte significative.

**Statut : idée à tester sur ce proto, pas encore engagée pour PCB v1.**

---

## 📋 Décisions de cette session (résumé)

| Sujet | Décision | Statut |
|---|---|---|
| Module MCU breakout | AN54LQ-15 | Accepté pour ce cycle, révisable |
| Connecteur breakout → host | 2× header femelle 1×17, socketé | Décidé |
| SWD sur host | Non — flash via breakout directement | Décidé |
| Capteurs SHT40/BH1750 | Bare-die + hotplate (pas dev boards) | Décidé |
| Mesure batterie | Canal SAADC interne VDD (à vérifier) | En attente de vérification datasheet |
| Protection inverse polarité | P-MOSFET high-side (AO3401), gate→GND | Décidé, à implémenter |
| DIP switches | 4 positions, P2.00–P2.03, pull-up interne | Décidé |
| Capteur sol segmenté (profondeur) | Tester 2 segments sur ce proto | À l'essai, pas engagé pour v1 |
| MCU Myco Pro (EFR32MG21 vs nRF54L15) | Remis en question (nRF54L15 supporte 802.15.4) | **Point à rouvrir dans DEC-004** |
| PCB partagé Mini/Pro | Probabilité revue à la hausse | **Point à rouvrir dans roadmap Phase 4** |

---

## 📌 Prochaine session

- [ ] Vérifier canal SAADC VDD interne dans le datasheet nRF54L15 (Product Specification, chapitre SAADC)
- [ ] Vérifier quels pins physiques sont SAADC-capables (pour les 2 segments du capteur sol)
- [ ] Confirmer la faisabilité mécanique des 2 segments capacitifs dans l'espace disponible (35–40mm de sonde)
- [ ] Recevoir et tester le breakout board
- [ ] Recevoir et tester la hotplate (premiers essais bare-die SHT40/BH1750)
- [ ] Mettre à jour `DEC-004` et la roadmap Phase 4 (question PCB partagé)
- [ ] Document de brief technique remis à Claude Code pour le design KiCad du PCB host (voir fichier séparé)
