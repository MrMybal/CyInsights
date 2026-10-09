# Préparation multithread — Release-20261009.2

Cette révision historique est [CyInsightsApp.exe](../bin/Release-20261009.2/CyInsightsApp.exe). La préparation utilise maintenant plusieurs cœurs, avec un pool partagé de threads réutilisés. L'interface Dear ImGui et le rendu D3D restent sur le thread de l'interface.

La release actuelle et les diagnostics de frames sont décrits dans [FrameDiagnostics.md](FrameDiagnostics.md).

## Étapes parallèles

- **Décodage CSV** : lots de 8 192 lignes, décodés par morceaux indépendants. L'ingestion dans l'index reste dans l'ordre source, pour conserver les identités, les frames et les relations parent/enfant.
- **Lecture des fenêtres et regroupement visuel** : chaque piste/profondeur est un travail indépendant. Ses blocs, statistiques et groupes de pixels sont préparés localement ; aucun timer partagé n'est modifié par plusieurs workers.
- **Construction des pistes** : tri, index de profondeurs et relations parent/enfant se construisent indépendamment par piste, après l'attribution des identités.

Les résultats sont réunis dans un ordre fixe. L'ordre des événements affichés et les sommes par timer ne dépendent donc pas de l'ordre d'exécution des threads. Le cache de blocs est protégé ; chaque lecture disque possède son propre flux. En cas d'annulation ou d'erreur, tous les travaux démarrés sont terminés avant de libérer leurs données ; aucun modèle incomplet n'est publié.

Les petits travaux restent séquentiels. Le mode automatique utilise jusqu'à 4 participants pour les petites fenêtres, jusqu'à 8 pour les plus grandes, et tient compte du nombre de processeurs logiques disponibles. Le pool est commun aux analyses concurrentes. Ces budgets de travail et de cache ne limitent ni la taille des captures ni le nombre d'événements conservés.

**Session > Analysis workers** permet de sélectionner automatique, 1, 2, 4 ou 8 workers lorsque la machine le permet. Ce choix vaut pour la session de l'application. La préparation pendant un glissement peut désormais être demandée toutes les 16,7 ms, une fois le travail précédent terminé ; cela ne garantit pas un rendu à 60 images/s sur tous les systèmes.

## Mesures sur la trace utilisateur

Capture : `20261008_142920.utrace`, **49 648 645 octets**, **14 192 325 événements**. Le benchmark mesure 36 fenêtres par configuration, avec déplacements aller/retour, 1 800 pixels et scopes adjacents préchargés. Les temps comprennent lecture, modèle d'affichage, statistiques et résumé de comparaison ; ils excluent le rendu GPU et l'export Unreal.

| Configuration | Médiane des 36 préparations | 95e percentile |
| --- | ---: | ---: |
| Release précédente `20261009.1` | 29,6 ms | 172,2 ms |
| Nouvelle version, 1 worker | 17,1 ms | 142,8 ms |
| Nouvelle version, 2 workers | 10,6 ms | 85,6 ms |
| Nouvelle version, 4 workers | 7,8 ms | 61,2 ms |
| Nouvelle version, 8 workers | 7,7 ms | 65,3 ms |
| Nouvelle version, automatique | 7,7 ms | 65,4 ms |

Le mode automatique est **3,8 fois plus rapide que la release précédente** et **2,2 fois plus rapide que le mode 1 worker de la nouvelle version** sur ce benchmark. La nouvelle version réduit aussi les allocations inutiles et emploie un accès direct aux colonnes de pixels quand son coût mémoire est approprié ; tout le gain par rapport à la release précédente ne vient donc pas des threads.

Par étendue visible, les médianes précédente/automatique sont **6,5 → 2,7 ms** pour 50 ms, **29,6 → 7,7 ms** pour 500 ms et **167,0 → 63,9 ms** pour 5 secondes. Ajouter des threads au-delà de 4 n'améliore pas toutes les vues ; le choix automatique évite de systématiquement employer le maximum sur les petites fenêtres.

L'indexation du même CSV passe de **16,3 à 8,2 secondes** dans ces runs, incluant le décodage parallèle et la réduction des allocations. Ce temps n'est pas celui de l'ouverture complète de la `.utrace` : l'analyse/export par l'outil Epic reste une phase séparée et coûteuse. Les caches système et autres tâches peuvent faire varier ces résultats. La configuration mesurée possède 24 processeurs logiques ; aucun gain identique n'est promis sur toutes les machines.

## Vérification

Les neuf suites passent **288 vérifications**. Elles couvrent les travaux exécutés une seule fois, la concurrence réelle, le budget de participants, le mode séquentiel, les appels imbriqués, les analyses concurrentes, les erreurs et annulations avec drainage, l'ordre des lignes CSV, les champs entre guillemets, les identités et les modèles par piste. Les fenêtres testent aussi les chemins de pixels dense et sparse et la réinitialisation du cache.

Les **180 fenêtres** du benchmark ont des empreintes sémantiques strictement identiques entre 1, 2, 4, 8 workers et automatique : données affichées, identités, tous les totaux par timer/domaine, indices de timers et parents. Les quantités représentées et sommes comparées à la release précédente sont également identiques dans ces runs.

L'aperçu intégral représente les **14 192 325 événements** avec 52 416 enregistrements d'affichage, dans un index de 454 154 408 octets. Le test réel dans l'application vérifie le maintien du glissement sur **120 publications de modèles**, les frames alignées, les hauteurs dynamiques et les gestes de zoom/déplacement. L'application se ferme normalement et la trace source garde son empreinte.

Preuves : [mesures et comparaison](../build/release-20261009.2-audit/benchmark-comparison.json), [benchmark précédent](../build/release-20261009.2-audit/baseline.csv), [sweep final](../build/release-20261009.2-audit/sweep-final.csv), [tests](../build/release-20261009.2-audit/build.log), [aperçu intégral](../build/release-20261009.2-audit/overview.log), [interface et navigation](../build/release-20261009.2-audit/opening.json).

Reproduction : compiler `CyTimingBenchmark`, puis lancer `CyTimingBenchmark.exe timing.csv chemin-vers-un-index-jetable.cyindex --sweep`. Le deuxième argument est un fichier de cache de test qui sera remplacé, jamais la capture source.
