---
id: frames
title: Frames et causes probables
description: Budgets, travail actif, attentes et couleurs de diagnostic.
section: Guides
order: 5
parent: timeline
sourceRevision: 2b1627b1042d
---

Frames suit les timings. Choisir le budget : 60 FPS vaut environ 16,67 ms, 72 FPS 13,89 ms, 90 FPS 11,11 ms. La navigation trouve des candidats lents, sans prouver la cause.

## Lire les couleurs {#interpret-colors}
Le fond indique un goulot présumé GameThread, RenderThread, GPU ou mixte quand les preuves suffisantes dépassent le budget. Neutre signifie sans cause attribuée, pas que tout est rapide. Les couleurs de catégories des scopes restent distinctes du fond de diagnostic.

Attentes/cadencements reconnus sont séparés du travail actif. EngineLoop/Frame peut inclure synchronisation ou inactivité. Comparer intervalles actifs et attentes connues. Une attente longue peut être la conséquence d'un autre thread ou du GPU.

## Limites du pipeline {#pipeline-limits}
CPU et GPU peuvent traiter des frames différentes simultanément. Un scalaire GPU observé pendant une frame de jeu peut être retardé, sans identité exacte de frame GPU. Parents et enfants se recouvrent. Les inclusifs imbriqués sur plusieurs threads ne sont pas un pourcentage d'occupation.

Choisir un intervalle court, examiner le domaine suspect, comparer plusieurs mauvaises frames à une bonne zone puis vérifier par capture contrôlée. Voir [GPU](doc:gpu-desktop) et [Comparaison](doc:comparison).


## Pipeline et attentes {#cpu-gpu}

Pipeline conceptuel. Des barres voisines ne sont pas forcément la même frame exécutée.

```diagram
{"type": "flow", "title": "Pipeline et attentes", "nodes": [{"id": "n0", "label": "Frame de jeu N", "x": 0, "y": 0}, {"id": "n1", "label": "Rendu décalé possible", "x": 310, "y": 0}, {"id": "n2", "label": "GPU encore décalé", "x": 0, "y": 100}, {"id": "n3", "label": "Attente ≠ travail", "x": 310, "y": 100}, {"id": "n4", "label": "Vérifier les horloges", "x": 0, "y": 200}, {"id": "n5", "label": "Vérifier la cause", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
