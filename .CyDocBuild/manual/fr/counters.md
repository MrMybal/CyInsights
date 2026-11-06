---
id: counters
title: Compteurs et statistiques par frame
description: Charger les sources et lire courbes ou tables par frame.
section: Guides
order: 7
parent: timeline
sourceRevision: 80b5ee98e1da
---

Counters et Frame stats exposent des sources distinctes. Déclarer un compteur prouve nom/type, pas la capture d'échantillons. Charger les valeurs à la demande, filtrer par recherche/disponibilité puis examiner ou épingler les séries utiles.

Les snapshots légers du plugin comprennent observations GameThread/RenderThread/RHI/GPU, intervalle et slots/capacité UObject. L'agrégation Stats optionnelle collecte les groupes actifs configurés, par exemple Game, UObjects et GPU. L'activer avant la capture. Le plugin ne recrée pas les groupes absents du build et n'exécute pas toutes les commandes HUD stat.

Frame stats offre courbes synchronisées et tables par frame. Vérifier unité, disponibilité, frame source et instant d'observation avant de limiter l'intervalle. Les frames absentes restent absentes ; un scalaire GPU/RT retardé ne prouve pas l'identité de la frame GPU.

Une jauge (mémoire utilisée, occupation) n'est pas un cumul d'octets/événements. De grands entiers dépassent la précision flottante ; le SDK conserve leur forme exacte quand disponible. Les classes visuelles réduisent les données sans créer de mesures. Une interface à 20 Hz ne transforme pas une source Meta de 1 Hz en mesures à 20 Hz.

Voir [Profils](doc:profiles), [Activités](doc:activities) et [Compteurs Quest](doc:quest-counters).


## Investigation ciblée {#analysis-flow}

Résumés d’abord ; les événements appuient un constat.

```diagram
{"type": "flow", "title": "Investigation ciblée", "nodes": [{"id": "n0", "label": "Vérifier sources/unités", "x": 0, "y": 0}, {"id": "n1", "label": "Repérer les frames lentes", "x": 310, "y": 0}, {"id": "n2", "label": "Choisir une courte zone", "x": 0, "y": 100}, {"id": "n3", "label": "Examiner un domaine", "x": 310, "y": 100}, {"id": "n4", "label": "Comparer un bon cas", "x": 0, "y": 200}, {"id": "n5", "label": "Preuves + limites", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
