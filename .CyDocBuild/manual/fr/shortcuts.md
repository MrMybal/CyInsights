---
id: shortcuts
title: Commandes et raccourcis
description: Timeline, images, clavier et défilement Quest.
section: Guides
order: 22
parent: reference
sourceRevision: 749fce63a1f0
---

| Contexte | Commande | Résultat |
| --- | --- | --- |
| PC | F1 / Help > User manual | Ouvrir ce guide |
| PC | Ctrl+O | Ouvrir les captures |
| Timing | Glisser droit / molette | Déplacer pistes/temps / zoom au pointeur |
| Règle fine de mesure | Glisser gauche | Sélectionner l'intervalle |
| Timing | F / Home | Cadrer la sélection / toute la capture |
| Timing | Flèche gauche / droite | Frame précédente / suivante |
| Timing | Échap | Effacer l’intervalle sélectionné |
| Scope | Clic / double clic | Sélection / filtre exact |
| Fond vide | Double clic | Retirer le filtre de vue |
| Scope | Clic droit | Histogramme/actions |
| Onglet | Menu contextuel | Détacher ou fermer |
| Image | Molette / glisser central / double clic | Zoom / déplacement / cadrage |
| Manuel | Retour / Suivant | Historique des chapitres |
| Manuel | Ctrl+K | Recherche globale |
| Image du manuel | Échap | Réduire l’image agrandie |

Les raccourcis concernent la vue focalisée ; la saisie de texte ne doit pas naviguer dans les documents. Les actions de barre d'outils restent accessibles sans clavier.

Sur Quest, le contrôleur/pointeur sélectionne et fait défiler par glissement sans cocher accidentellement. Clavier et défilement générique/molette sont acceptés quand disponibles. Help ouvre le lecteur CyDoc local avec défilement tactile, sommaire repliable dans les petites fenêtres et agrandissement des images dans la fenêtre. Échap réduit le média agrandi.

Position des statistiques : [HUD](doc:quest-hud). Position/opacité des tuiles : [Heatmap overlay](doc:quest-heatmap).


## Navigation et mesure {#timeline-navigation}

Commandes schématiques, pas une trace. Les règles partagent un axe.

```diagram
{"type": "flow", "title": "Navigation et mesure", "nodes": [{"id": "n0", "label": "Glisser droit : déplacer", "x": 0, "y": 0}, {"id": "n1", "label": "Molette : zoom", "x": 310, "y": 0}, {"id": "n2", "label": "Règle fine : A vers B", "x": 0, "y": 100}, {"id": "n3", "label": "Lire la durée", "x": 310, "y": 100}, {"id": "n4", "label": "L’intervalle filtre", "x": 0, "y": 200}, {"id": "n5", "label": "F : cadrer la zone", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
