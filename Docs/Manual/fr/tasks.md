# Tâches et délais d’exécution {#title}

Ouvrir Tasks et charger le fournisseur natif s'il existe. Il faut TaskTrace dans la trace ; les noms de scopes CPU ne reconstruisent pas les identités natives manquantes. Borner les requêtes par intervalle et piste avant les événements/liens détaillés.

Création, planification, attente en file, exécution et achèvement sont des phases distinctes ; certaines manquent parfois. Une fonction courte peut finir tard si sa tâche attend un worker. Temps d'exécution n'est pas délai en file.

Le repli par candidats CPU est déduit des scopes. Il n'a ni identifiant TaskTrace, ni délai de file mesuré, ni chaîne de dépendances faisant autorité. Ne pas présenter ces liens comme des dépendances natives enregistrées.

Examiner les liens d'une tâche longue, corréler son exécution aux attentes/budget et comparer une bonne zone. Une dépendance n'explique un goulot que si elle bloque réellement le travail. Ne pas changer le nombre de workers sur un lien seul.

La collecte est optionnelle dans [Profils](doc:profiles). Les phases absentes restent indisponibles. Voir [Fonctions](doc:functions) et [Frames](doc:frames).
