# Plusieurs captures et fenêtres natives

`File > Open capture...`, `Ctrl+O` et `Open captures...` permettent de
sélectionner plusieurs fichiers `.utrace` ou `.cycapture`. Chaque fichier
s'ouvre dans un onglet distinct ; les captures précédentes restent ouvertes.
Plusieurs chemins peuvent également être passés à l'exécutable au lancement.

`Detach capture`, ou le menu contextuel de l'onglet, déplace toute la session
d'analyse dans une fenêtre native Dear ImGui. `Return to main window` remet
la même session dans un onglet. La timeline, les filtres, la sélection, les
distributions et les panneaux restent propres à cette capture. Les panneaux
individuels peuvent toujours être détachés ; leurs classes de docking empêchent
de les mélanger avec les panneaux d'une autre trace.

Les index restent paginés sur disque. Le chargement continue dans les onglets
inactifs, sans préparer leurs graphiques à chaque frame ; les panneaux flottants
restent affichés. Le pool d'analyse est partagé. Fermer un onglet annule et
termine ses lecteurs sur un worker, pour ne pas attendre sur le thread UI.
Une session qui reçoit une capture réseau doit être arrêtée avant fermeture.
Il n'y a pas de limite ajoutée au nombre ou à la taille des captures : les
ressources disponibles sur le PC restent nécessaires à chaque index ouvert.

## IA / MCP

Chaque document possède son endpoint local et ses autorisations. Copier la
configuration dans son propre `AI Inspector > Connection` : elle contient
`CYINSIGHTS_APP_PID` et `CYINSIGHTS_DOCUMENT_ID`. La configuration reste liée
à ce document quand on change d'onglet ou de fenêtre. Pour le document initial,
l'ancien endpoint basé sur le PID reste compatible. `open_trace` depuis ce
pont remplace le document explicitement ciblé ; les ouvertures par l'interface
créent de nouveaux onglets. Fermer le document coupe son endpoint.

## Vérification locale

`--multi-trace-smoke --snapshot <image> <capture-A> <capture-B> <capture-C>`
vérifie des filtres indépendants, le détachement, le retour en onglet et la
fermeture du troisième document. Le rapport `<image>.documents.json` décrit
les documents et les viewports natifs. Les fixtures de test sont synthétiques ;
elles ne constituent pas une mesure matérielle Quest.
