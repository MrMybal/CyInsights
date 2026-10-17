# Diagnostic par frame — Release-20261009.3

Application : [CyInsightsApp.exe](../bin/Release-20261009.3/CyInsightsApp.exe). Interface native C++/Dear ImGui.

## Lecture des couleurs

Dans **Timing**, seul le fond des pistes du domaine candidat est teinté pour les frames lentes : **rose = GameThread**, **bleu = RenderThread**, **vert = GPU**. Les autres pistes restent neutres. Plusieurs candidats proches et dépassant chacun le budget gardent leur couleur propre sur leurs pistes ; la barre récapitulative dans **Frames** devient violette. Les attentes/pacing, les causes indéterminées, les données incomplètes et les frames dans le budget restent neutres, avec leur diagnostic détaillé au survol. Le graphe Frames n'applique plus de teinte à tout son fond. Les scopes traversant une frame restent continus.

**Timing > View Mode > Target fps** règle le budget (60 par défaut, soit 16,67 ms ; 72/90/120 sont possibles). Le même menu permet de désactiver les fonds. **Next slow frame** cherche le prochain dépassement, en donnant priorité aux candidats avec du travail observé supérieur au budget. Le survol d'une barre affiche la durée, l'identité native ou le tick candidat, la cible, la cause probable et les coûts de chaque domaine.

## Calcul et limites du diagnostic

Les timestamps proviennent des scopes natifs complets, avant toute agrégation de pixels. Les durées présentées sont des **estimations dérivées** par union d'intervalles mesurés et classification des noms d'attente. Le coût total observé est l'union des scopes dans l'intervalle : les scopes imbriqués ne sont jamais additionnés. Une frame sans couverture instrumentée n'est pas une mesure de zéro travail.

Pour le CPU, les familles explicites `WaitFor*`, `WaitUntil*`, `WaitFrame`, `Wait`, `Sleep`, `SleepNoStats`, `FrameLimit`, `Idle` et `PresentWait` sont reconnues, y compris après un préfixe de classe. Un nom contenant simplement « wait » ne suffit pas. Les tâches observées à l'intérieur d'un scope d'attente restent du travail : les intervalles suivent la catégorie du scope le plus profond, avec reprise de l'attente du parent après la tâche. Les attentes imbriquées de cette tâche sont encore soustraites. Une attente non instrumentée ou un nom non reconnu peut rester dans le travail estimé ; ce résultat n'est pas une mesure de temps réellement planifié sur un cœur.

Une structure de scopes désordonnée/croisée revient à une estimation conservatrice des attentes et rend la cause indéterminée. Les scopes sans fin restent incomplets, sans endpoint ni coût inventé. Les lignes de frames exportées par les analyseurs récents servent à identifier les frames et sont exclues du travail.

Le GPU utilise l'union de ses scopes natifs observés ; ses noms de passes ne sont pas classés comme des attentes CPU. Le détail GPU avec/sans attente n'est pas disponible. Pour plusieurs pistes du même domaine ou plusieurs queues GPU, le diagnostic retient la piste au plus grand travail observé, avec son total et ses attentes, plutôt que d'additionner des queues parallèles. Il ne prétend pas mesurer l'occupation complète du matériel.

Une frame dans le budget n'est pas signalée comme lente. Au-delà du budget, le travail observé du candidat doit lui-même dépasser la cible. Des coûts concurrents à moins de 10 % du maximum donnent plusieurs candidats. Si aucun travail ne dépasse le budget mais qu'une attente CPU couvre au moins 25 % de la frame, le résultat est **Wait/pacing** ; sinon la cause reste indéterminée. Ces seuils sont des règles heuristiques, pas une preuve de chemin critique.

**L'association RenderThread/GPU utilise le recouvrement temporel du GameThread.** Le pipeline peut exécuter une autre frame : ce diagnostic ne constitue pas une correspondance exacte entre frames de rendu, frames GPU et frames jeu. Les compteurs Meta à fréquence réduite ne sont pas convertis en temps GPU par frame. Les données ReShade sans calibration commune ne sont pas utilisées pour ces couleurs. Si les scopes GPU manquent, le survol affiche « unavailable » et aucun coût GPU n'est inventé.

## Optimisations

La couverture est construite pendant l'ingestion, avec des unions compactes et une pile de scopes ; les événements complets restent dans l'index. La finalisation des pistes et le calcul par lots de frames utilisent le pool de workers existant. Les unions et leurs intégrales permettent des requêtes de coût sans reparcourir tous les événements. Les unions temporaires sont libérées après la préparation ; seuls les résultats par frame restent pour l'affichage.

Les barres de frames sont mises en cache tant que l'intervalle et la largeur sont identiques. Les guides de timing réutilisent le regroupement par pixel de Frames pour éviter une ligne graphique par frame lors d'un dézoom intégral. Dans un pixel contenant plusieurs frames, la couleur et le survol correspondent à la frame de plus grande durée, explicitement signalée ; ils ne résument pas les causes de toutes ces frames. Une première lecture de fenêtre native, ensuite jetée avant la lecture réelle de l'application ou de la comparaison B, a été supprimée.

Aucun plafond de taille, nombre d'événements ou profondeur de capture n'a été ajouté. Les budgets de cache/affichage concernent uniquement la préparation et le rendu, sans perte des événements ou des statistiques.

## Validation

- **328 vérifications / 10 suites**, dont unions imbriquées, attentes, tâches exécutées dans une attente, bornes identiques, scopes croisés, queues parallèles, GPU absent, GPU mesuré, frontières de frames, annulation et équivalence séquentielle/parallèle. Un oracle indépendant vérifie 200 requêtes sur 700 intervalles aléatoires.
- Trace utilisateur de **49 648 645 octets**, **14 192 325 événements** et **6 009 ticks candidats** : ouverture, rendu et fermeture normale ; SHA-256 source inchangé. Cette trace ne fournit pas de scopes GPU.
- À 60 fps : 5 066 intervalles dans le budget, 185 candidats RenderThread, 12 GameThread, 25 mixtes, 696 attentes/pacing, 25 indéterminés. Ces résultats sont dérivés ; les candidats ne prouvent pas une causalité exacte.
- **180 fenêtres** : données, identités, parents et totaux strictement identiques à la release précédente et entre 1/2/4/8 workers et automatique.
- Indexation CSV mesurée : **8,39 s avant / 8,60 s avec diagnostic**. Finalisation des diagnostics : **3,47 ms**, hors construction de la couverture pendant l'ingestion. Médiane automatique des fenêtres : **9,06 ms**, contre 11,12 ms dans le run précédent ; le chemin de calcul des fenêtres est inchangé, cette variation n'est pas attribuée à une nouvelle accélération. L'export Epic reste nécessaire et n'est pas compris dans ces temps.
- Navigation réelle : déplacement/zoom, sortie de règle, déplacement sur scopes, maintien du glissement sur **130 publications**, frames alignées et hauteurs dynamiques. Test complet avec ouverture, gestes et recherche d'une frame lente : **61,05 s**, mémoire de l'application échantillonnée à **126,6 MiB** au maximum. Cela ne représente ni un benchmark de l'ouverture seule, ni la mémoire du helper Epic.
- Nouveau binaire scanné par Kaspersky le 9 octobre à 20:08:22 : zéro détection, suspicion, objet ignoré, corrompu ou erreur. Non signé. L'alerte historique de `bin/Release` reste non résolue.

La capture Quest matérielle, les queues GPU réelles et la trace de 1,4 Go n'ont pas été retestées sur cette révision ; la validation antérieure du stockage volumineux reste historique.

![Interface réelle : candidat RenderThread en bleu et attente en ambre](../build/release-20261009.3-audit/ui-navigation.png)

Preuves : [benchmark et répartition](../build/release-20261009.3-audit/benchmark-comparison.json), [diagnostics complets](../build/release-20261009.3-audit/current.cyindex.frames.csv), [navigation](../build/release-20261009.3-audit/opening.json), [tests](../build/release-20261009.3-audit/tests-detailed.log), [scan](../build/release-20261009.3-audit/kaspersky-scan.txt).
