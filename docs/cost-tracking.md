# Suivi des coûts — Myco Mini

Doc vivant. Chaque section se met à jour au fil des vrais prix vérifiés (panier LCSC réel, devis fab, devis fournisseurs) plutôt que des estimations. Dater chaque mise à jour.

## ⚠️ Observation transversale : le shipping domine, pas les composants

Confirmé sur **3 commandes séparées** maintenant — à chaque fois le port dépasse ou approche le coût des biens eux-mêmes :

| Commande | Biens | Shipping | % du port |
|---|---|---|---|
| Breakout PCB+stencil (10 pcs) | $13.49 | $29.37 | 68% |
| Host PCB+stencil (10 pcs) | $41.89 | $30.27 | 72% |
| SHT40 (10 pcs) | $15.84 | $20.00 | **126%** |

Le vrai levier de réduction de coût à ce stade n'est pas d'optimiser encore le BOM composant par composant — c'est de **grouper les commandes** (même fournisseur, même envoi) plutôt que de les passer séparément au fil des besoins.

## 1. Host PCB (`hardware/myco-mini-host-pcb/`)

Source de vérité pour la BOM : `hardware/myco-mini-host-pcb/bom_jlcpcb.csv`.

### Composants (panier LCSC réel, complété pour 10 cartes)

| | Coût |
|---|---|
| Panier initial (couvre 3 cartes, goulot = AO3401A) — 2026-08-26 | $38.86 |
| Complément pour 5 cartes pile (+5× AO3401A) — 2026-08-26 | +$0.49 |
| Commande complémentaire, reste des passifs pour compléter à 10 cartes — 2026-08-27 (merchandise $53.42 + shipping $12.63 − remise $2.64) | +$63.41 |
| **Total composants pour 10 cartes** | **$102.76** → **$10.28/carte** |

Hors panier (à sourcer séparément) :
- `J1`/`J2` connecteurs 1×17 — estimé ~$0.40/carte
- Pile CR2032 elle-même (le BOM ne référence que le support) — estimé ~$0.40/carte

### Fabrication PCB (devis JLCPCB, 5 pcs, 2026-08-26)

| Option | Total (5 pcs) | $/carte |
|---|---|---|
| Base (HASL) | $16 | $3.20 |
| ENIG | $32 | $6.40 |
| ENIG + edge plating | $84 | $16.80 |

**Retenu : ENIG, sans edge plating** — le fin-pitch de `U2`/`U3` justifie l'ENIG, l'edge plating coûte +162% pour un problème déjà couvert par du conformal coating manuel (quasi gratuit, prévu au post-traitement).

⚠️ Le tableau ci-dessus (5 pcs) ne compte que le PCB — pas de stencil, pas de shipping.

### Devis réel complet, 10 pcs + stencil (2026-08-27)

| | Coût |
|---|---|
| Merchandise (PCB + stencil, ENIG) | $41.89 |
| Shipping | $30.27 |
| **Grand Total** | **$72.16** → **$7.22/carte** |

⚠️ **Le shipping représente 72% du merchandise** — même constat que pour le breakout (§2, 68%). Motif qui se confirme : à ce volume, grouper les commandes (PCB host + breakout + autre besoin futur dans le même envoi) amortirait très fortement ce poste plutôt que de payer le port à chaque commande séparée.

### Total host PCB (10 cartes, devis complet)

| Poste | $/carte |
|---|---|
| Composants (panier LCSC complet, §1 ci-dessus) | $10.28 |
| Fab + stencil + port ENIG (§ ci-dessus) | $7.22 |
| Connecteurs J1/J2 + pile CR2032 (estimé) | ~$0.80 |
| **Total** | **≈ $18.30/carte** |

## 2. Breakout AN54LQ-15 (2026-08-26, achats réels)

| Poste | Coût |
|---|---|
| PCB JLCPCB (10 pcs) | $6.40 |
| Stencil | $7.09 |
| Shipping JLCPCB | $29.37 |
| Composants passifs LCSC (`C2,C3,C10,L1,R1,Y1,Reset1,J1,J4,J5` — qtés 20-100) | $10.67 |
| Shipping LCSC | $7.52 |
| **Sous-total fixe (lot)** | **$61.05** |
| Module `AN54LQ-15` ×6 (eBay) | $36.00 → **$6.00/u** |

⚠️ **Goulot : seulement 6 modules achetés pour 10 PCBs.** Même schéma que pour le host PCB (AO3401A) mais ici le complément n'est pas anodin — $6/module, pas quelques cents.

⚠️ **eBay plus cher que Digikey ici** : $6.00/u vs $4.10/u à l'unité chez Digikey (§1 du projet). À revérifier avant de racheter les 4 modules manquants — eBay n'était pas l'option la moins chère cette fois.

⚠️ **Le shipping JLCPCB domine la facture** : $29.37 de port pour $13.49 de PCB+stencil (68% du sous-total JLCPCB). Grouper une prochaine commande amortirait fortement ce poste.

| Breakouts construits | Total | $/breakout |
|---|---|---|
| 6 (avec ce qui est acheté) | $97.05 | $16.18 |
| 10 (si 4 modules de plus achetés) | $121.05 | $12.11 |

## 3. Autres composants matériels

| | Statut |
|---|---|
| SHT40-AD1B-R2 (sourcé hors BOM JLCPCB, probablement Digikey) | Subtotal $15.84 + shipping $20.00 = **$35.84 pour 10 → $3.58/u** (vérifié 2026-08-27) — remplace le $1.60/u (subtotal seul, sans port) et l'estimation $1.79 encore antérieure utilisée dans la BOM |
| Boîtier (issue [#13](https://github.com/ludodefgh/PlantSensor/issues/13), Fusion 360/PETG) | pas chiffré |

## 4. Overhead business (coûts fixes à amortir, pas par-unité intrinsèquement)

| | Issue | Statut |
|---|---|---|
| Certification IC Canada (dispositif BLE) | [#15](https://github.com/ludodefgh/PlantSensor/issues/15) | pas chiffré — probablement plusieurs milliers $, à amortir sur le volume total vendu |
| Incorporation entreprise (Québec) | [#14](https://github.com/ludodefgh/PlantSensor/issues/14) | pas chiffré |

Sensibilité de la certif selon le volume (exemple à $5000/$10000, hypothétique tant que non chiffré) :

| Unités vendues | @$5000 | @$10 000 |
|---|---|---|
| 100 | $50/u | $100/u |
| 500 | $10/u | $20/u |
| 2000 | $2.50/u | $5/u |

## 5. Total par unité — historique

| Date | Composition | Total |
|---|---|---|
| 2026-08-26 | Host PCB (estimé) + breakout (estimé, **sans PCB fab du breakout — sous-estimé**) + DIP switch retiré | ~$19.87 → ~$19.06 |
| 2026-08-26 | Host PCB (réel, 5 cartes, ENIG) + breakout (réel, 6-10 unités) | **~$27-31/capteur** |
| 2026-08-27 | Host PCB ($18.30, 10 cartes, panier composants complet) + breakout ($12.11, 10 unités) + SHT40 ($3.58) — hors boîtier | **~$34.00/capteur** (dont ~$10.00/capteur de port connu → **~$24.00/capteur hors port**) |

Le saut de ~$19 à ~$27-31 vient entièrement du fait que l'estimation précédente du breakout oubliait PCB+stencil+shipping (non chiffrés à l'époque) — pas d'un vrai changement de design. Le chiffre du 2026-08-27 resserre la fourchette précédente en remplaçant les estimations composants restantes par des devis réels (panier LCSC complet à 10 cartes pour le host PCB, prix réel SHT40) — toujours hors boîtier, connecteurs J1/J2 et pile CR2032 (déjà comptés séparément au §1).

**Hardware direct seul ne dit pas grand-chose sur le prix de vente viable** — règle générale hardware : prix de vente ≈ 3-5× coût matériel direct pour couvrir boîtier/assemblage/SAV/marge, *avant même* d'amortir la certification (§4). Repère à garder : sous ~$60-90 de prix de vente, la structure de coût actuelle ne laisse pas de marge réelle pour un vrai produit commercialisé (cf. discussion 2026-08-26) — reste viable comme projet à prix coûtant pour la communauté si c'est l'objectif visé.
