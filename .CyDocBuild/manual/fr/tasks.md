---
id: tasks
title: Tâches et délais d’exécution
description: Phases natives, dépendances et candidats CPU déduits.
section: Guides
order: 8
parent: timeline
sourceRevision: 34baf854b597
---

Ouvrir Tasks et charger le fournisseur natif s'il existe. Il faut TaskTrace dans la trace ; les noms de scopes CPU ne reconstruisent pas les identités natives manquantes. Borner les requêtes par intervalle et piste avant les événements/liens détaillés.

Création, planification, attente en file, exécution et achèvement sont des phases distinctes ; certaines manquent parfois. Une fonction courte peut finir tard si sa tâche attend un worker. Temps d'exécution n'est pas délai en file.

Le repli par candidats CPU est déduit des scopes. Il n'a ni identifiant TaskTrace, ni délai de file mesuré, ni chaîne de dépendances faisant autorité. Ne pas présenter ces liens comme des dépendances natives enregistrées.

Examiner les liens d'une tâche longue, corréler son exécution aux attentes/budget et comparer une bonne zone. Une dépendance n'explique un goulot que si elle bloque réellement le travail. Ne pas changer le nombre de workers sur un lien seul.

La collecte est optionnelle dans [Profils](doc:profiles). Les phases absentes restent indisponibles. Voir [Fonctions](doc:functions) et [Frames](doc:frames).


## Investigation ciblée {#analysis-flow}

Résumés d’abord ; les événements appuient un constat.

```diagram
{"type": "flow", "title": "Investigation ciblée", "nodes": [{"id": "n0", "label": "Vérifier sources/unités", "x": 0, "y": 0}, {"id": "n1", "label": "Repérer les frames lentes", "x": 310, "y": 0}, {"id": "n2", "label": "Choisir une courte zone", "x": 0, "y": 100}, {"id": "n3", "label": "Examiner un domaine", "x": 310, "y": 100}, {"id": "n4", "label": "Comparer un bon cas", "x": 0, "y": 200}, {"id": "n5", "label": "Preuves + limites", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
