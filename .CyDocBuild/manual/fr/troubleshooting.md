---
id: troubleshooting
title: Dépannage
description: Vérifier séparément connexion, source et rendu.
section: Guides
order: 21
parent: reference
sourceRevision: 1ce41032a70f
---

## Capture à zéro octet {#capture-stays-at-zero-bytes}
Vérifier cible/build/IP et port 41855. Un jeu visible ne prouve pas le service autorisé. Lire Session / Messages et le log cible. Arrêter la capture détenue en échec avant de réessayer ; ne pas relancer en boucle un jeu sans lien.

## Quest bloqué ou aucun HUD {#quest-is-stuck-or-no-hud}
Confirmer la scène voulue, ni accueil, chargement ni dialogue contrôleurs. Garder le casque éveillé. ADB offline est distinct d'un rendu défaillant. Ouvrir Connection > Start GPU service dans CyInsightOVR et lire séparément la notice de droits/préparation. Aucun autre compagnon/PC permanent n'est requis. Commande acceptée n'est pas visibilité prouvée.

## Graphe draw/triangle absent {#no-draw-triangle-graph}
Choisir HUD graph/value, pas seulement Overview. Employer APK Vulkan et cible Development exacte ; fermer/rouvrir après préparation/mise à jour. Lire connexion, fraîcheur, cadence et erreurs de publication. Préférer les débits ; les variantes par frame peuvent être N/A en XR. Voir [HUD](doc:quest-hud).

## Une barre GPU ou aucune tuile {#one-gpu-bar-or-no-tiles}
Une ancienne trace ne gagne pas des scopes absents. Scopes natifs, métriques et stages Meta sont distincts. Confirmer profiling avant contexte GPU et événements réellement reçus, pas seulement réglages activés. Mauvaise cible/source périmée/ancien boot restent indisponibles.

## Stats/réseau/mémoire absents {#missing-stats-network-memory}
Vérifier profil, instrumentation, lecteur et chargement. Un compteur déclaré peut n'avoir aucune valeur. Absence n'est pas zéro. Logs natifs et piles mémoire complètes ont des limites.

## Image noire ou grille décalée {#black-image-or-misaligned-grid}
Consentement ne prouve pas les pixels. Examiner image et cible au premier plan. Garder le noir signalé ; ne pas affirmer mesh/œil exact. Les instants image/tuile peuvent différer.

## Antivirus/mise à jour {#antivirus-update-failure}
Conserver détection exacte, chemin et identité. Ne pas désactiver la protection ni supposer qu'une signature suffit. Un exécutable supprimé après compilation ne se lance pas. Une autre clé Android exige la bonne clé ou une sauvegarde/réinstallation préparée, jamais un effacement silencieux.

Pour signaler un problème, indiquez le message exact, la version de l’application, la cible choisie, l’état du fournisseur et quelques étapes permettant de le reproduire.


## Sources et affichages Quest {#quest-architecture}

Composants complémentaires, avec prérequis et preuves distincts.

```diagram
{"type": "flow", "title": "Sources et affichages Quest", "nodes": [{"id": "n0", "label": "Unreal : trace native", "x": 0, "y": 0}, {"id": "n1", "label": "OVR : service local", "x": 310, "y": 0}, {"id": "n2", "label": "Couche de jeu optionnelle", "x": 0, "y": 100}, {"id": "n3", "label": "Sources GPU Meta", "x": 310, "y": 100}, {"id": "n4", "label": "HUD / overlay de tuiles", "x": 0, "y": 200}, {"id": "n5", "label": "PC : capture/analyse", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
