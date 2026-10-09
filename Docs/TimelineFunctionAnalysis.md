# Fonctions, histogrammes et sélection temporelle

Release de test : `bin/Release-20261009.5/CyInsightsApp.exe`, interface native C++/Dear ImGui.

## Depuis la timeline

- **Double-clic sur un scope natif** : filtre par son nom complet exact, sensible à la casse, sur tous les threads et tous les TimerId portant ce nom. Les occurrences conservent leur couleur, les autres scopes sont atténués pour garder le contexte. Le geste ne change pas le zoom. La table Timers suit ce filtre ; Callers et Callees conservent les autres noms nécessaires à l'inspection des relations.
- **Clic droit sur un scope natif** : menu de filtrage, **Histogram of this function**, ou suppression du filtre. Le même accès à l'histogramme existe dans les tables de timers. Le panneau Distributions s'ouvre sous Timing si les deux partageaient le même emplacement ; un panneau déjà placé séparément conserve sa position.
- **Shift + glissement gauche sur la règle temporelle** : sélection d'un intervalle. Avec **Follow timeline selection**, activé par défaut, Distributions et Tasks ouverts prennent cette plage après relâchement, avec un délai de regroupement de 120 ms. Une navigation ou un zoom seuls ne changent pas la plage d'analyse.

**Clear function filter** retire le filtre exact ; saisir une recherche textuelle le remplace. **Whole capture** désactive le suivi de sélection et analyse le domaine complet. **Use selected interval** le réactive ; sans sélection, cette commande prend la fenêtre visible. Échap efface la sélection ; le suivi revient alors à toute la capture.

Les agrégats de rendu et les lignes de frames n'ont pas une identité de fonction unique. Leur double-clic conserve le cadrage temporel. Un agrégat contenant une occurrence du nom filtré peut rester coloré, mais reste explicitement un agrégat dérivé, avec ses autres scopes ; il ne devient pas une nouvelle mesure ni un TimerId unique.

## Population de l'histogramme

L'action contextuelle sélectionne les durées des scopes natifs, **tous les threads et toutes les profondeurs**, pour le nom exact choisi. Les contrôles permettent ensuite de restreindre la piste ou les profondeurs. Les occurrences proviennent des enregistrements bruts, pas des rectangles agrégés affichés. Les identités natives restent distinctes dans les données même si le filtre regroupe leurs noms.

Un échantillon doit **commencer dans `[début, fin)`** ; sa durée complète est conservée. Une durée inachevée reste indisponible. La table Timers décrit les événements qui recouvrent la sélection, avec des durées découpées et des relations imbriquées : son nombre de scopes peut donc différer du nombre d'échantillons de l'histogramme. Les échantillons de scopes sont inclusifs ; leur somme n'est pas une mesure d'utilisation du thread.

Un clic sur une tranche parcourt toutes ses valeurs, par pages de 128, en conservant la distribution complète, sa moyenne, ses percentiles et sa CDF. La pagination ne limite pas les événements acceptés. Un double-clic sur une valeur revient à son intervalle dans Timing. Les percentiles scopes/tasks restent estimés et marqués `~`, avec leurs bornes au survol ; ceux des coûts par frame sont exacts. Voir [Distributions et Tasks](TraceAnalysis.md).

## Optimisation

L'index conserve les blocs contenant chaque timer natif. Une recherche de fonction visite uniquement l'union de ces blocs, dans l'ordre physique, et vérifie ensuite chaque identité et début d'événement. Une fonction absente ne lit aucun bloc. Les requêtes concurrentes et la recherche de l'occurrence la plus proche partagent un cache de blocs immuables. Les candidats CPU Tasks utilisent une visite commune aux pistes et limitée aux timers candidats.

La distribution complète est mise en cache selon la source, le nom exact, le thread, l'intervalle et les autres paramètres. Le choix d'une tranche ou d'une page lit les échantillons une seule fois au lieu de reconstruire la distribution puis relire la tranche. Le retour aux valeurs les plus lentes peut réutiliser le résumé complet. Les bornes et facteurs de l'axe sont préparés une fois par analyse. Sans recherche ni filtre exact, le rendu ne construit pas de masque de noms. Les analyses et les fenêtres visibles restent préparées en arrière-plan ; les résultats d'une ancienne demande ne remplacent pas ceux de la nouvelle sélection.

Mesures locales sur les **14 192 325 événements** de `20261008_142920.utrace` : médiane de trois requêtes, cache de blocs de l'application vidé avant chaque requête ; cache de fichiers Windows non contrôlé. Le benchmark vérifie l'égalité des comptes et des empreintes de tous les échantillons entre visite générale et indexée.

| Fonction | Enregistrements bruts | Visite générale | Avec index de fonction | Enregistrements parcourus avec index |
| --- | ---: | ---: | ---: | ---: |
| FEngineLoop::Tick | 6010 | 414,8 ms | 4,28 ms | 6013 |
| ExecuteForegroundTask | 373 686 | 487,7 ms | 60,35 ms | 632 828 |
| MobileSceneRender | 68 829 | 658,0 ms | 87,26 ms | 1 543 672 |

Les 6010 ticks incluent un scope inachevé : l'histogramme conserve **6009 durées valides**. Pour une tranche de MobileSceneRender, la requête passe de 140,5 à 78,7 ms avec le résumé en cache ; ExecuteForegroundTask passe de 39,6 à 36,4 ms. La petite population Tick ne présente pas de gain mesuré sur cette pagination (3,27 contre 3,69 ms). Ces résultats concernent les requêtes après indexation, pas le premier export/import CSV ni toutes les captures. Le premier chargement reste dépendant du pont Epic. L'index ajoute des métadonnées en mémoire ; aucun plafond de taille de trace ou de nombre d'événements n'est ajouté.

## Validation

**452 vérifications / 12 suites, zéro échec** : égalité des visites indexées, même nom sur plusieurs identités/threads, casse et noms proches, plage à borne supérieure exclusive, annulation, cache concurrent, agrégats, résumé inchangé lors d'une pagination, source modifiée et plages sous-normales.

Le test d'interaction utilise les entrées ImGui de l'application sur la trace utilisateur : double-clic sans zoom, clic droit/histogramme, sélection sur la règle, mise à jour de 6009 à une durée dans cette plage, histogramme visible avec Timing, clic de tranche conservant les statistiques. Les gestes de navigation prolongés pendant les chargements, les Frames alignées et la hauteur dynamique des pistes restent vérifiés. Distributions et Tasks sont également ouverts sur leurs traces réelles. Les traces sources gardent leurs empreintes.

Preuves : [interactions](../build/release-20261009.5-audit/interactions.json), [navigation](../build/release-20261009.5-audit/opening.json), [analyses](../build/release-20261009.5-audit/analysis-ui.json), [benchmark](../build/release-20261009.5-audit/histogram-benchmark.csv), [manifest](../bin/Release-20261009.5/release-manifest.json).
