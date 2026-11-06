---
id: quest-counters
title: Draw calls, triangles et fréquence des courbes
description: Géométrie soumise, complétude et limites des frames XR.
section: Guides
order: 15
parent: quest-hud
sourceRevision: 3e18daa185b5
---

Choisir le package Development Vulkan exact. Start HUD prépare la cible ; Overview propose aussi Prepare draw counter layer et Start draw counters. Fermer/rouvrir complètement après préparation ou mise à jour de couche. Les APK commerciaux non débogables ne sont pas pris en charge.

Les draws soumis connus comptent les commandes suivies à la soumission des buffers, y compris réutilisations, exécutions secondaires répétées et instances. Les triangles d'entrée sont avant culling, clipping, géométrie créée par shader et expansion des vues : ni triangles visibles, objets, ni total par œil. Nos draws de heatmap sont exclus.

L'indirect peut donner des draws connus sans triangles connus. Nombres GPU, primitive restart, topologie inconnue et buffers manquants rendent les totaux incomplets. Vérifier complétude et intervalle, pas seulement le nombre.

## Débit ou frame {#rate-versus-frame}
Les compteurs principaux sont draw/s et triangles/s. Les variantes par frame présentée exigent une vraie présentation Vulkan. Beaucoup de chemins XR n'utilisent pas vkQueuePresentKHR ; les soumissions ne deviennent pas des frames inventées. Ces variantes peuvent rester N/A tandis que les débits fonctionnent.

Curve updates propose 1/5/10/20 Hz, 10 initialement. Lire la cadence mesurée. Les compteurs système lents ne sont pas interpolés. Le HUD Meta entier utilise K triangles pour la précision ; source/Overview conservent leurs unités. Les sources périmées créent des trous. Métadonnées et IPC ajoutent une charge ; le surcoût en jeu doit être mesuré.


## Choisir chaque destination {#hud-selection}

Graphe/texte indépendants. Overview seul ne demande jamais de graphe en jeu.

```diagram
{"type": "flow", "title": "Choisir chaque destination", "nodes": [{"id": "n0", "label": "Trouver la métrique", "x": 0, "y": 0}, {"id": "n1", "label": "HUD graph : courbe", "x": 310, "y": 0}, {"id": "n2", "label": "HUD value : texte", "x": 0, "y": 100}, {"id": "n3", "label": "Overview : app seule", "x": 310, "y": 100}, {"id": "n4", "label": "Start / Apply HUD", "x": 0, "y": 200}, {"id": "n5", "label": "Vérifier source/affichage", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
