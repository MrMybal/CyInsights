---
id: profiles
title: Profils de capture et surcoût
description: Défauts légers et sources de diagnostic optionnelles.
section: Guides
order: 18
parent: capture
sourceRevision: 367c4bd98bc9
---

Commencer léger puis ajouter les sources utiles. Les défauts gardent scopes CPU/GPU, frames, ancrages et observations légères. Agrégation Stats, mémoire du processus, canaux tâches/réseau/fichiers/chargement/logs/LLM, données Windows détaillées et lectures d'images/textures sont distincts.

Project Settings > Plugins > CyInsights conserve les défauts pour les prochaines captures. Session > Capture options utilise Use plugin project defaults. Connect les récupère ; Copy plugin defaults for this capture permet une surcharge sans modifier le projet. Les options SDK suivent le même principe.

Les groupes Stats doivent exister, être compilés et actifs. LLM exige son instrumentation ; les allocations détaillées, des flags au lancement. Les groupes Quest nécessitent timestamps RHI et scopes compilés. Observer par frame ne prouve pas l'identité de frame exécutée.

## Détailler si nécessaire {#escalate-only-as-needed}
1. Scopes/frames de base : localiser les zones suspectes.
2. Stats/activités choisies : tester l'hypothèse.
3. Stages/commandes GPU détaillés : approfondir.
4. Images/readbacks : diagnostic supplémentaire explicite.
5. Revérifier les performances finales avec un profil léger.

Aucune promesse de surcoût nul ou fixe ne vaut pour tous les jeux. Noter le profil dans les rapports. Polling rapide n'accélère pas une source lente. Borner les modes détaillés et distinguer leur coût du niveau normal du jeu.


## Détailler si nécessaire {#profile-flow}

Chaque source peut coûter plus ; aucun pourcentage universel n’est affirmé.

```diagram
{"type": "flow", "title": "Détailler si nécessaire", "nodes": [{"id": "n0", "label": "Scopes/frames de base", "x": 0, "y": 0}, {"id": "n1", "label": "Activités choisies", "x": 310, "y": 0}, {"id": "n2", "label": "Stages GPU détaillés", "x": 0, "y": 100}, {"id": "n3", "label": "Images optionnelles", "x": 310, "y": 100}, {"id": "n4", "label": "Diagnostic court", "x": 0, "y": 200}, {"id": "n5", "label": "Retester en léger", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
