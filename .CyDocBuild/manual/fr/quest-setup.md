---
id: quest-setup
title: Installer CyInsightOVR
description: Variantes APK, fonctionnement autonome et droits uniques.
section: Guides
order: 13
parent: quest
sourceRevision: 812ab254a04b
---

Installer un APK : Standard pour le compagnon de base, Vulkan pour la couche Development optionnelle, dont compteurs draw/triangles et heatmap expérimentale. Conserver la clé de signature pour les mises à jour ; ne pas désinstaller pour corriger une signature différente sans sauvegarder les données.

Ouvrir Bibliothèque > Sources inconnues > CyInsightOVR. Les raccourcis dépendent d'Horizon OS ; l'accès rapide n'en demande que si pris en charge. La grande configuration est distincte du HUD VR compact.

CyInsightOVR démarre son propre service GPU au premier plan à l'ouverture. Connection propose Start / Stop GPU service et l'état. Aucun CyInsightCompagnon séparé ni collecteur PC n'est requis normalement. Après reboot, rouvrir l'application pour démarrer son service.

Contrôle du HUD Meta et couche Vulkan externe exigent l'autorisation documentée WRITE_SECURE_SETTINGS accordée une fois. L'installateur Quest complet optionnel le fait via ADB autorisé. Cette préparation ne signifie pas qu'un PC est nécessaire à chaque HUD. Réinstallation/remise à zéro des droits peut la demander à nouveau. Les images exigent un consentement Android distinct.

Pages : Overview, GPU heatmap, Display & HUD, Heatmap overlay, Connection, Updates et Help. Start HUD conserve la configuration ouverte. Voir [HUD](doc:quest-hud), [Compteurs](doc:quest-counters) et [Enregistrement](doc:tile-recording).


## Sources et affichages Quest {#quest-architecture}

Composants complémentaires, avec prérequis et preuves distincts.

```diagram
{"type": "flow", "title": "Sources et affichages Quest", "nodes": [{"id": "n0", "label": "Unreal : trace native", "x": 0, "y": 0}, {"id": "n1", "label": "OVR : service local", "x": 310, "y": 0}, {"id": "n2", "label": "Couche de jeu optionnelle", "x": 0, "y": 100}, {"id": "n3", "label": "Sources GPU Meta", "x": 310, "y": 100}, {"id": "n4", "label": "HUD / overlay de tuiles", "x": 0, "y": 200}, {"id": "n5", "label": "PC : capture/analyse", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
