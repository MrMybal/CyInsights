---
id: glossary
title: Unités, preuves et glossaire
description: Disponibilité, horloges, subpasses et lecture des tuiles.
section: Guides
order: 23
parent: reference
sourceRevision: 97043b5f68a2
---

| Terme | Sens |
| --- | --- |
| Mesuré | Observation source avec limites de couverture/horloge |
| Déduit | Calcul/reconstruction, pas une autre mesure matérielle |
| Indisponible / N/A | Absent, non pris en charge, périmé ou inutilisable ; pas zéro |
| Partiel / connu | Seulement la portion instrumentée connue |
| Inclusif / exclusif | Avec enfants / sans leurs intervalles connus |
| Instant d'observation | Réception/observation, pas forcément exécution |
| Identité de frame | Attributions CPU, rendu et GPU peuvent différer |
| ms / us | 1 milliseconde = 1000 microsecondes |
| Hz / FPS | Mises à jour / frames par seconde ; polling n'est pas mesure |
| Draw call | Commande de dessin, pas nombre de meshes/objets visibles |
| Triangles d'entrée | Géométrie avant culling et amplification |
| Render pass / subpass | Travail Vulkan sur pièces jointes / étape interne |
| Tuile GPU | Région de surface avec coûts de stages échantillonnés |
| Binning / Render | Enveloppe / rendu pouvant inclure vertex et fragment |
| .utrace / .cycapture / .cyovr | Trace Unreal / conteneur combiné / snapshot tuiles |

Trois subpasses sont trois étapes internes, pas trois barres GPU chronométrées. Contenus/arguments ne donnent ni coût matériel par étape ni début de rasterisation. Tuile chaude et image situent une zone, pas un mesh/shader unique ou œil exact.

Frame neutre, histogramme vide et compteur absent peuvent signifier preuves insuffisantes. Garder unités, bornes, source, disponibilité et incertitudes. Retour à [Commencer ici](doc:welcome) ou [Dépannage](doc:troubleshooting).


## Coûts de tuiles illustratifs {#tile-grid}

EXEMPLE : couleurs et microsecondes illustratives, sans mesure live. Pas d’identité unique de mesh/shader/œil.

![EXEMPLE : couleurs et microsecondes illustratives, sans mesure live. Pas d’identité unique de mesh/shader/œil.](assets/tile-grid-fr.svg)


## Pipeline et attentes {#cpu-gpu}

Pipeline conceptuel. Des barres voisines ne sont pas forcément la même frame exécutée.

```diagram
{"type": "flow", "title": "Pipeline et attentes", "nodes": [{"id": "n0", "label": "Frame de jeu N", "x": 0, "y": 0}, {"id": "n1", "label": "Rendu décalé possible", "x": 310, "y": 0}, {"id": "n2", "label": "GPU encore décalé", "x": 0, "y": 100}, {"id": "n3", "label": "Attente ≠ travail", "x": 310, "y": 100}, {"id": "n4", "label": "Vérifier les horloges", "x": 0, "y": 200}, {"id": "n5", "label": "Vérifier la cause", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
