# Fluidité de l'interface native

Les graphes restent dans Dear ImGui, avec un axe temporel continu partagé par
Frames, Timing, les compteurs et les activités natives. Le déplacement conserve
le grab pendant les publications asynchrones ; les profondeurs des pistes suivent
les événements présents dans l'intervalle visible.

## Préparation des données

- Les activités accumulent leurs recouvrements par plages de pixels et propagent
  les extrema après lecture. Elles évitent de parcourir chaque pixel pour chaque
  intervalle long. Comptages, valeurs signées, points et bornes de sélection sont
  conservés ; la recherche sans filtre évite de parcourir le dictionnaire de noms.
- Le graphe et le tableau d'activités ont des clés de requête distinctes. Une
  recherche, un changement de page ou de piste dans le tableau ne relit pas un
  graphe inchangé. Une lecture en cours peut aboutir pendant un déplacement ; les
  demandes intermédiaires sont regroupées. Les détails attendent une sélection
  stable pendant 75 ms, et les demandes de graphe sont espacées de 33 ms. Le rendu
  et l'entrée souris continuent à chaque frame.
- Le dernier graphe d'activité terminé reste projeté sur l'axe courant pendant
  la préparation du suivant, avec une indication explicite. Ses colonnes conservent
  leur intervalle d'origine ; cette densité temporaire ne constitue pas une nouvelle
  mesure à la résolution du zoom. Les détails périmés ne sont pas publiés pour une
  autre sélection, piste ou recherche.
- Les compteurs disposent d'un index de résumés de blocs construit à l'import,
  hors du thread de rendu. Les extrema conservent les indices des échantillons
  et comparent les entiers sans conversion en double. Les requêtes combinent les
  blocs complets et lisent les bordures ; les courbes denses utilisent le même
  index. Les courbes clairsemées gardent un parcours simple. La moyenne reste une
  valeur déduite et son arrondi peut varier légèrement avec l'ordre de réduction.
- Les filtres et le tri du tableau des compteurs sont réutilisés tant que leurs
  entrées ne changent pas. Les pistes CPU hors écran calculent leur hauteur, mais
  ne reconstruisent leurs listes d'événements qu'au moment de devenir visibles.
- L'ancien modèle de timing est conservé jusqu'au prochain travail de préparation,
  qui le libère en arrière-plan. Cela évite de détruire ses chaînes et vecteurs
  pendant une frame de déplacement ; un seul ancien modèle est conservé.

Ces mécanismes limitent le travail d'affichage et ses caches, jamais le nombre
d'événements accepté dans une capture. L'index des compteurs ajoute de la mémoire
proportionnelle aux échantillons ; le stockage des événements d'activité reste sur
disque. Modifier les échantillons via le SDK interne exige de reconstruire leur
index avec `CounterModel::Index()`.

## Navigation

Quand Timing a le focus et qu'aucun champ texte ou popup n'est actif :

| Commande | Action |
| --- | --- |
| `F` ou **Fit selection** | Cadrer la sélection ; sans sélection, cadrer trois frames |
| `Home` | Cadrer la capture entière |
| Flèches gauche/droite | Passer à la frame précédente/suivante |
| `Escape` | Effacer la sélection temporelle |
| `Ctrl+F` | Rechercher dans le panneau actif Timing, Counters ou Activities |

La molette zoome autour de la souris. Le déplacement par glissement conserve
les interactions existantes ; Shift + glisser dans la règle sélectionne une plage.
Le double-clic filtre une fonction par nom exact et le menu contextuel ouvre sa
distribution, toujours liée à la sélection temporelle.

## Mesurer

`CyUiPreparationBenchmark` produit un JSON de médianes de sept requêtes sur des
données **synthétiques** : 100 000 intervalles mémoire superposés, un million
d'échantillons de compteur, et 2 048 colonnes. Il fournit aussi des empreintes de
résultats pour comparer les versions. Ce cas volontairement dense ne mesure pas
le gain général sur toute trace ou sur tout matériel.

```text
cmake --build build/Release --target CyUiPreparationBenchmark
build/Release/CyUiPreparationBenchmark.exe
```

L'exécutable accepte aussi `--profile-ui <rapport.json>`, associé par exemple à
`--navigation-smoke --snapshot <image.png> <trace.utrace>`. Le rapport distingue
les frames chargées et celles pendant un grab. Il mesure le temps CPU de
préparation et de soumission du viewport principal ; il exclut `Present`, le
temps GPU et les fenêtres détachées. Le mode de mesure est facultatif et
n'accumule aucune donnée lorsque son option est absente.

Les tests de référence comparent les graphes d'activité et les requêtes de
compteurs à des parcours complets, avec recouvrements, bornes, valeurs invalides,
timestamps identiques et entiers au-delà de la précision du double.

Mesures locales avant/après, même exécutable de benchmark et mêmes données :

| Préparation synthétique | Avant, médiane | Après, médiane |
| --- | ---: | ---: |
| Activités superposées | 172,39 ms | 4,38 ms |
| Statistiques du compteur | 2,93 ms | 0,0012 ms |
| Courbe dense du compteur | 2,97 ms | 1,69 ms |

Les empreintes de résultats du benchmark sont identiques. Sur la trace réelle
de 14,2 millions d'événements, un essai de navigation sans compilation parallèle
mesure environ **0,24 ms** de préparation CPU du viewport principal en médiane,
avant comme après ; le percentile 95 reste autour de **0,42 ms**. Cette mesure
n'établit pas un gain général de FPS. Les gestes continus, le zoom, les frames
alignées et les hauteurs dynamiques sont vérifiés par le test de navigation natif.
