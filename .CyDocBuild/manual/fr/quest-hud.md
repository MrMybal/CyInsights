---
id: quest-hud
title: Choisir les graphes et textes du HUD
description: Destinations indépendantes, application et neuf positions.
section: Guides
order: 14
parent: quest
sourceRevision: 4e325ce604e9
---

Ouvrir Display & HUD > Graphs & HUD.

| Colonne | Destination |
| --- | --- |
| HUD graph | Courbe dans le jeu |
| HUD value | Texte numérique dans le jeu |
| Overview | Courbe dans la configuration seulement |

Choisir graphe seul, valeur seule ou les deux. Add draw calls + triangles ajoute les deux pour les compteurs principaux par seconde sans effacer les autres choix ; ce ne sont pas seulement les variantes par frame. All HUD graphs choisit les courbes listées ; Clear HUD efface les choix. Recherche et filtres GPU/CPU/Memory/Device réduisent le tableau.

Start HUD applique et garde la configuration ouverte. Les cases modifiées pendant le HUD sont enregistrées/appliquées après un court délai ; Apply HUD réessaie explicitement. Stop HUD est distinct de la heatmap. Neuf positions incluent Top center, Center et Bottom center. Taille/position des statistiques et opacité/position des tuiles sont indépendantes.

Les valeurs natives Meta utilisent son HUD. Les métriques supplémentaires passent par l’interface publique personnalisée depuis la couche Vulkan du jeu Development choisi : Meta les associe à l’application VR. Utilisez l’APK Vulkan, la cible exacte et une réouverture complète après préparation ou mise à jour. Si une courbe personnalisée manque, vérifiez la connexion à la couche du jeu et l’erreur de publication affichée ; l’acceptation des réglages ne signifie pas que le jeu publie des valeurs.

[N/A] signifie absent/périmé/inutilisable, pas zéro. [partial] signifie total connu incomplet. Voir [Compteurs](doc:quest-counters) et [Dépannage](doc:troubleshooting).


## Choisir chaque destination {#hud-selection}

Graphe/texte indépendants. Overview seul ne demande jamais de graphe en jeu.

```diagram
{"type": "flow", "title": "Choisir chaque destination", "nodes": [{"id": "n0", "label": "Trouver la métrique", "x": 0, "y": 0}, {"id": "n1", "label": "HUD graph : courbe", "x": 310, "y": 0}, {"id": "n2", "label": "HUD value : texte", "x": 0, "y": 100}, {"id": "n3", "label": "Overview : app seule", "x": 310, "y": 100}, {"id": "n4", "label": "Start / Apply HUD", "x": 0, "y": 200}, {"id": "n5", "label": "Vérifier source/affichage", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
