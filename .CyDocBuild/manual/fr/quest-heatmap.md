---
id: quest-heatmap
title: Tuiles GPU et surimpression heatmap
description: Stages, HUD supplémentaire et plein écran approximatif.
section: Guides
order: 16
parent: quest
sourceRevision: 92bfae0c38f3
---

GPU heatmap montre un snapshot. Heatmap overlay contrôle une surimpression de développement en direct, distincte des statistiques. Il faut APK Vulkan, cible débogable choisie, couche externe préparée, données Meta détaillées compatibles et réouverture du jeu après préparation.

1. Vérifier cible/disponibilité dans Heatmap overlay.
2. Préparer couche/profiling avant le nouveau contexte GPU ; rouvrir le jeu manuellement une fois.
3. Choisir Extra HUD ou Fullscreen (approximate), position, plafond de coût et opacité.
4. Start démarre ; Apply ajuste. Stop masque sans retirer la préparation. Uninstall target integration la retire et impose préparation/réouverture la fois suivante.

La couleur suit coût échantillonné/échelle ; faible opacité préserve l'image. L'alignement plein écran reste approximatif. Des dimensions de surface n'identifient ni œil, frame ni projection. La grille du manuel est indiquée comme exemple, sans données live.

Binning est une enveloppe. Render inclut vertex et fragment. LoadColor/LoadDepthStencil, StoreColor/StoreDepthStencil décrivent les transferts quand disponibles. Une tuile chaude localise un travail suspect, pas un mesh/shader unique. Ne pas additionner les enveloppes qui se recouvrent.

Aucun overlay personnalisé universel n'est validé pour tous les APK commerciaux non modifiés. Source absente/périmée/mauvaise cible : indisponible, pas simulée. Voir [Enregistrement](doc:tile-recording) et [GPU](doc:gpu-desktop).


## Coûts de tuiles illustratifs {#tile-grid}

EXEMPLE : couleurs et microsecondes illustratives, sans mesure live. Pas d’identité unique de mesh/shader/œil.

![EXEMPLE : couleurs et microsecondes illustratives, sans mesure live. Pas d’identité unique de mesh/shader/œil.](assets/tile-grid-fr.svg)


## Sources et affichages Quest {#quest-architecture}

Composants complémentaires, avec prérequis et preuves distincts.

```diagram
{"type": "flow", "title": "Sources et affichages Quest", "nodes": [{"id": "n0", "label": "Unreal : trace native", "x": 0, "y": 0}, {"id": "n1", "label": "OVR : service local", "x": 310, "y": 0}, {"id": "n2", "label": "Couche de jeu optionnelle", "x": 0, "y": 100}, {"id": "n3", "label": "Sources GPU Meta", "x": 310, "y": 100}, {"id": "n4", "label": "HUD / overlay de tuiles", "x": 0, "y": 200}, {"id": "n5", "label": "PC : capture/analyse", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
