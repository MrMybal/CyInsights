---
id: gpu-desktop
title: Lire les données GPU
description: Scopes mesurés, passes déduites et collecteurs optionnels.
section: Guides
order: 11
parent: timeline
sourceRevision: 342badc87e59
---

Commencer par les groupes GPU natifs mesurés. Une seule barre MobileSceneRender signifie que seul ce niveau existe. Un autre panneau ne recrée pas les timestamps enfants absents.

## Distinguer les sources {#distinguish-sources}
- Scopes GPU Unreal : mesurés quand RHI/build fournit timestamps et instrumentation.
- Contenus de passes/subpasses Vulkan : arguments, pièces jointes et structure. Les commandes côté CPU ne sont pas des coûts matériels par subpass.
- Collecteur Windows optionnel : commandes, pipelines, shaders, ressources et timestamps disponibles. Activation explicite ; la lecture .utrace normale n'exige pas CyGPUInspector séparé.
- Stages/tuiles Meta : [source Quest](doc:quest-capture) distincte.

GPU frame peut montrer passes, nombres draw/dispatch et dépendances reconstruites. Passes/liens déduits sont étiquetés. Les timestamps de commandes sont optionnels et parfois locaux à la frame. Décrire une ressource/shader ne mesure pas sa durée.

Un Render élevé seul ne prouve pas un goulot pixel shader : il peut inclure vertex et fragment. Croiser parts, attentes texture/mémoire, dimensions, tuiles et changements contrôlés. Binning est une enveloppe, pas un supplément indépendant à ajouter aux enfants. Attentes et recouvrements comptent.

Les lectures d'images/textures/buffers sont désactivées initialement. Le détail peut perturber le GPU : garder le diagnostic court puis retester en [profil léger](doc:profiles). Voir [Glossaire](doc:glossary).


## Vrai inspecteur GPU optionnel {#gpu-passes}

Exemple Windows : passes et dépendances reconstruites. Les relations déduites sont étiquetées.

```image
{"id": "gpu-passes", "src": "assets/cyinsights-gpu-passes.png", "alt": "Vrai inspecteur GPU optionnel", "controls": "minimal"}
```


## Pipeline et attentes {#cpu-gpu}

Pipeline conceptuel. Des barres voisines ne sont pas forcément la même frame exécutée.

```diagram
{"type": "flow", "title": "Pipeline et attentes", "nodes": [{"id": "n0", "label": "Frame de jeu N", "x": 0, "y": 0}, {"id": "n1", "label": "Rendu décalé possible", "x": 310, "y": 0}, {"id": "n2", "label": "GPU encore décalé", "x": 0, "y": 100}, {"id": "n3", "label": "Attente ≠ travail", "x": 310, "y": 100}, {"id": "n4", "label": "Vérifier les horloges", "x": 0, "y": 200}, {"id": "n5", "label": "Vérifier la cause", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
