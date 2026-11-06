---
id: functions
title: Fonctions, appels et histogrammes
description: Noms exacts, distributions des durées et exemples.
section: Guides
order: 6
parent: timeline
sourceRevision: a948f47d5d3c
---

Timers donne effectifs et durées agrégées. L'inclusif comprend les enfants ; l'exclusif retire leurs intervalles connus. L'exclusif calculé dépend de l'imbrication enregistrée. Additionner tous les inclusifs ne donne pas la durée de frame.

## Examiner une fonction {#investigate-one-function}
1. Choisir un court intervalle suspect dans les timings.
2. Double-cliquer le scope ou utiliser son action de filtrage exact.
3. Au clic droit, ouvrir Histogram of this function.
4. Lire effectif, distribution et percentiles. Choisir une classe demande des exemples exacts à retrouver sur la timeline.
5. Changer l'intervalle ; vérifier bornes/filtre affichés avant comparaison.

Un histogramme de durées décrit des occurrences, pas une courbe temporelle. Une moyenne basse peut cacher quelques appels coûteux. Les percentiles portent sur la population choisie et sont instables avec peu d'appels. Les relations d'appel dépendent de la source/imbrication ; une vue agrégée peut demander de zoomer.

Double-cliquer le fond vide retire le filtre exact. Voir [Comparaison](doc:comparison) entre captures et [Tâches](doc:tasks) pour l'attente en file.


## Vrai histogramme de fonction {#histogram}

Vue réelle : distribution d’une fonction dans un intervalle. Les valeurs appartiennent à cette trace d’exemple.

```image
{"id": "histogram", "src": "assets/cyinsights-function-histogram.png", "alt": "Vrai histogramme de fonction", "controls": "minimal"}
```
