# Vitesse du graphique — Release-20261009.1

La nouvelle application est [CyInsightsApp.exe](../bin/Release-20261009.1/CyInsightsApp.exe). Le graphique continue à afficher les scopes natifs continus, y compris ceux qui traversent des frames. Tous les événements restent dans l'index sur disque et les statistiques utilisent les enregistrements complets.

## Changements

- Recherche des blocs par piste et profondeur, avec bornes temporelles indexées. Une longue portée extérieure ne force plus le parcours des blocs de toutes les autres profondeurs.
- Cache des blocs récemment lus : 32 Mio de données réutilisables, avec éviction des entrées anciennes. Ce budget concerne le cache en mémoire ; il ne limite ni les captures ni les événements acceptés. Les métadonnées et les blocs utilisés par les analyses en cours utilisent aussi de la mémoire.
- Classification CPU/GPU calculée une fois par piste ; identités des groupes de comparaison préparées à l'indexation.
- Regroupement des scopes sous un pixel sans construire des chaînes et objets d'affichage pour chaque événement source. Les groupes conservent le nombre représenté et restent signalés comme dérivés.
- Couleurs, profondeurs visibles, recherche et tri des tableaux mis en cache. Les lignes de scopes hors écran ne sont pas soumises au rendu.
- Pendant un glissement, une nouvelle préparation peut commencer toutes les 33 ms, une fois la précédente terminée. Le pointeur et les frames continuent de suivre l'axe pendant les lectures.

## Mesures

Le benchmark utilise l'export existant de `20261008_142920.utrace` : **14 192 325 événements**, capture native de **49 648 645 octets**. Il mesure 36 fenêtres autour du milieu de la capture, avec déplacement aller/retour, trois étendues temporelles et 1 800 pixels. Chaque requête prépare également les scopes des intervalles adjacents pour la navigation.

Les temps incluent la lecture, le modèle d'affichage, les statistiques complètes et le résumé de comparaison. Ils ne mesurent pas le rendu GPU ni l'export initial d'Unreal.

| Fenêtre visible | Médiane avant | Médiane après |
| --- | ---: | ---: |
| 50 ms | 16,1 ms | 10,4 ms |
| 500 ms | 60,5 ms | 38,1 ms |
| 5 s | 357,0 ms | 180,4 ms |

Sur les 36 requêtes, la médiane passe de **60,5 à 38,1 ms** (−37 %) et le 95e percentile de **370,4 à 203,5 ms** (−45 %). La requête répétée de 50 ms réutilise ses 108 blocs sans lecture disque supplémentaire. Les quantités affichées, événements représentés, occurrences, scopes inachevés et sommes inclusives/exclusives sont identiques pour les 36 requêtes avant/après.

Les mesures dépendent de la machine, de la trace, des caches système et des tâches concurrentes. La première analyse reste dominée par l'export Epic et l'indexation du CSV ; cette révision ne fournit pas encore de cache persistant entre ouvertures. Dans les deux runs du benchmark, l'indexation seule a pris 23,2 et 25,3 secondes : aucun gain d'ouverture initiale n'est revendiqué.

## Vérification

Les huit suites passent 266 vérifications. Les nouveaux cas couvrent les blocs multiples, intervalles d'affichage/statistiques séparés, portées longues/inachevées, frontières exactes, réutilisation du cache, données CPU/GPU avec le même TimerId, conservation des coûts exclusifs, changement de zoom et annulation.

L'aperçu intégral représente les **14 192 325 événements** avec 52 416 enregistrements d'affichage à 2 000 pixels, dans un index de 454 154 408 octets. Le test d'interface vérifie à nouveau le glissement maintenu pendant des publications du modèle, les frames alignées, les hauteurs dynamiques et les différents gestes de zoom/déplacement. L'application se ferme normalement et l'empreinte de la `.utrace` reste inchangée.

Preuves : [résultat du benchmark](../build/release-20261009.1-audit/benchmark-comparison.json), [avant](../build/release-20261009.1-audit/baseline.csv), [après](../build/release-20261009.1-audit/optimized.csv), [conservation intégrale](../build/release-20261009.1-audit/overview.log), [tests](../build/release-20261009.1-audit/build.log), [test d'interface](../build/release-20261009.1-audit/opening.json).

Reproduction : compiler la cible `CyTimingBenchmark`, puis lancer `CyTimingBenchmark.exe timing.csv chemin-vers-un-index-jetable.cyindex`. Le second argument désigne un cache de test qui sera remplacé, jamais une capture source.
