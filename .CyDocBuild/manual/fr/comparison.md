---
id: comparison
title: Comparer deux captures
description: Intervalles indépendants, métadonnées et normalisation.
section: Guides
order: 10
parent: timeline
sourceRevision: 0ece170ef911
---

Ouvrir Capture comparison ou Analysis > Compare native capture puis choisir B. Les originaux restent en lecture seule et peuvent aussi être ouverts dans des onglets/fenêtres séparés.

## Comparer équitablement {#compare-fairly}
1. Vérifier plateforme, build, fournisseurs disponibles et profil de chaque côté.
2. Choisir séparément les zones A/B. Une situation de jeu équivalente compte plus que des timestamps identiques.
3. Comparer effectifs, distributions, travail actif et attentes.
4. Normaliser par seconde ou occurrence si durée/effectif diffèrent. Le total d'une capture plus longue n'est pas automatiquement une régression.
5. Examiner les fonctions/métriques expliquant l'écart.

Les noms/domaines sont des correspondances déduites, pas des identités d'objets partagées. Les canaux absents ne valent pas zéro. Une instrumentation différente change couverture et parfois surcoût ; préférer des réglages équivalents.

Conserver identité, bornes, population/effectif, unité et normalisation dans le rapport. La comparaison GPU optionnelle utilise les preuves du collecteur présentes, sans inventer de passes communes.

Voir [Histogrammes](doc:functions), [Frames](doc:frames) et [Rapports IA](doc:ai).


## Vrai histogramme de fonction {#histogram}

Vue réelle : distribution d’une fonction dans un intervalle. Les valeurs appartiennent à cette trace d’exemple.

```image
{"id": "histogram", "src": "assets/cyinsights-function-histogram.png", "alt": "Vrai histogramme de fonction", "controls": "minimal"}
```
