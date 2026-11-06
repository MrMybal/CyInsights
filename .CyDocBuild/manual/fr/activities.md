---
id: activities
title: Réseau, stockage, chargement et mémoire
description: Pistes natives optionnelles et prérequis de capture.
section: Guides
order: 9
parent: timeline
sourceRevision: c24479c87d39
---

Ouvrir Network / storage / memory (Activities) et charger le fournisseur utile. Les lecteurs d'activités sont des adaptateurs optionnels distincts. Une trace de timings ne garantit pas ces canaux. Filtrer nature/piste et choisir un court intervalle.

- Réseau : paquets/contenus enregistrés nécessitent NetTrace et son instrumentation. Les bits de contenu ne sont pas automatiquement tous les octets sur le réseau, tout le trafic socket ou une capture externe complète.
- Stockage : opérations de fichiers Unreal, pas toutes les opérations disque de tous les processus.
- Chargement : événements de ressources du moteur quand présents, distincts des fichiers bruts.
- Mémoire : jauges/tags disponibles. LLM exige son instrumentation ; les allocations détaillées, les flags au lancement. L'exploration complète des piles mémoire n'est pas terminée ici.

Commencer par les résumés et détailler une piste suspecte. Comparer les recouvrements CPU/GPU en gardant horloges/disponibilité. Un paquet près d'un à-coup est une corrélation, pas une preuve.

Activer les sources avant enregistrement dans [Profils](doc:profiles). Ouvrir le panneau ou une requête IA n'instrumente pas une ancienne trace. Messages contient les diagnostics du logiciel ; le lecteur natif complet des logs Unreal n'est pas fourni.


## De la capture à l’analyse {#capture-flow}

Schéma de parcours. Seules les sources enregistrées sont analysables.

```diagram
{"type": "flow", "title": "De la capture à l’analyse", "nodes": [{"id": "n0", "label": "Choisir les sources", "x": 0, "y": 0}, {"id": "n1", "label": "Enregistrer la cible", "x": 310, "y": 0}, {"id": "n2", "label": "Arrêter et finaliser", "x": 0, "y": 100}, {"id": "n3", "label": "Ouvrir le conteneur", "x": 310, "y": 100}, {"id": "n4", "label": "Charger les fournisseurs", "x": 0, "y": 200}, {"id": "n5", "label": "Sélectionner et examiner", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
