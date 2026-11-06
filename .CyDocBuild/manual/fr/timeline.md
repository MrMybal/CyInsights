---
id: timeline
title: Naviguer et mesurer dans la timeline
description: Déplacement, zoom, frames synchronisées et filtre temporel.
section: Guides
order: 4
parent: desktop
sourceRevision: ecda4e753dd0
---

## Naviguer {#navigate}
- Glisser droit déplace le temps horizontalement et les pistes verticalement.
- La molette zoome autour du pointeur. Fit cadre la capture ; Frame parcourt les frames ; F cadre la sélection.
- Replier les pistes inutiles. Le GPU est au-dessus de GameThread. La hauteur suit l'imbrication visible au lieu de réserver du vide.

La règle supérieure sert à naviguer. La règle fine supplémentaire dessous mesure : glisser gauche de A à B choisit bornes et durée. Un glissement inversé produit aussi un intervalle ordonné. Frames et timings partagent la vue.

## Sélectionner et filtrer {#select-and-filter}
Un clic sur un scope sélectionne l'événement. Le double clic filtre tous les éléments de ce nom exact. Le double clic sur fond vide retire ce filtre. Le clic droit ouvre histogramme/filtre exact ; un glissement droit reste un déplacement.

Distributions, statistiques de fonctions, frames, compteurs et activités compatibles utilisent l'intervalle sélectionné ou indiqué dans le panneau. Vérifier les bornes avant comparaison. Fit selection change la vue, pas les timestamps. Recherche et filtrage exact sont distincts. Aucune sélection ne crée les données absentes.

Voir [Fonctions](doc:functions), [Frames](doc:frames) et [Commandes](doc:shortcuts).


## Vraie timeline CPU/GPU {#timeline}

Vue réelle : frames, scopes et stages GPU disponibles partagent un axe. Trace d’exemple, pas votre résultat actuel.

```image
{"id": "timeline", "src": "assets/cyinsights-timeline.png", "alt": "Vraie timeline CPU/GPU", "controls": "minimal"}
```


## Navigation et mesure {#timeline-navigation}

Commandes schématiques, pas une trace. Les règles partagent un axe.

```diagram
{"type": "flow", "title": "Navigation et mesure", "nodes": [{"id": "n0", "label": "Glisser droit : déplacer", "x": 0, "y": 0}, {"id": "n1", "label": "Molette : zoom", "x": 310, "y": 0}, {"id": "n2", "label": "Règle fine : A vers B", "x": 0, "y": 100}, {"id": "n3", "label": "Lire la durée", "x": 310, "y": 100}, {"id": "n4", "label": "L’intervalle filtre", "x": 0, "y": 200}, {"id": "n5", "label": "F : cadrer la zone", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
