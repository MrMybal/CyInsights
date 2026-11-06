---
id: unreal-plugin
title: Plugin Unreal et connexion
description: Installer les sources pour le bon build et connecter la capture.
section: Guides
order: 2
parent: capture
sourceRevision: 7d53a4ed6140
---

Copier les sources du plugin CyInsights dans Plugins du projet, l'activer et recompiler pour le moteur/la plateforme. Le plugin est CyInsights ; son collecteur Windows interne optionnel ne nécessite pas l'application CyGPUInspector séparée.

## Connecter {#connect}
1. Ouvrir Project Settings > Plugins > CyInsights, ou Capture settings dans le plugin.
2. Lancer un build autorisé Editor, Debug, DebugGame ou Development. Ne pas supposer les services présents en Shipping.
3. Dans Session PC, saisir l'IPv4 locale de la cible puis Connect to device. Le port habituel est 41855.
4. Vérifier défauts/capacités annoncés, puis Start remote trace. Stop remote trace finalise la capture ; l'analyse commence après l'arrêt.

Le plugin vise UE 4.27–5.8 dans les builds clients Editor, Debug, DebugGame et Development. Recompilez-le pour votre version mineure du moteur et votre plateforme cible ; un APK ou un binaire provenant d’une autre installation du moteur n’est pas interchangeable. Les lecteurs natifs du PC nécessitent une installation UE 5.5–5.8 compatible et enregistrée dans l’application.

En cas d'échec, vérifier jeu lancé, service autorisé dans ce build, IP et réseau/pare-feu. GPU Profiling vert dans Meta ne prouve pas que le service Unreal répond. Voir [Dépannage](doc:troubleshooting).


## De la capture à l’analyse {#capture-flow}

Schéma de parcours. Seules les sources enregistrées sont analysables.

```diagram
{"type": "flow", "title": "De la capture à l’analyse", "nodes": [{"id": "n0", "label": "Choisir les sources", "x": 0, "y": 0}, {"id": "n1", "label": "Enregistrer la cible", "x": 310, "y": 0}, {"id": "n2", "label": "Arrêter et finaliser", "x": 0, "y": 100}, {"id": "n3", "label": "Ouvrir le conteneur", "x": 310, "y": 100}, {"id": "n4", "label": "Charger les fournisseurs", "x": 0, "y": 200}, {"id": "n5", "label": "Sélectionner et examiner", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
