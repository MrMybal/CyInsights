# Compteurs natifs et métriques Quest dans l'application

L'application C++/Dear ImGui lit les compteurs Unreal présents dans une `.utrace` ou dans la trace native d'un `.cycapture`, y compris les métriques Meta embarquées par le plugin. L'ouverture d'une capture reçue par réseau suit le même parcours. L'analyse commence après l'arrêt et la fermeture du flux natif.

## Utilisation

- Ouvrir **Counters** depuis la barre de navigation ou le menu View.
- Rechercher un nom ; **Quest / Meta only** limite la liste aux métriques du plugin. Les compteurs sans échantillons et les compagnons de disponibilité sont masqués par défaut, avec des options pour les afficher.
- Sélectionner une ligne et cocher **Show in timeline**, ou double-cliquer sur la ligne. Les quatre premières métriques Meta exploitables sont ajoutées automatiquement.
- Zoomer et déplacer la timeline comme pour les pistes CPU/GPU. Les graphiques de compteurs partagent le même axe temporel continu ; ils ne sont pas découpés artificiellement par frame.
- Sélectionner un intervalle sur la règle pour recalculer nombre de valeurs valides, minimum, maximum et moyenne. Sans sélection, les statistiques portent sur l'intervalle visible. Les colonnes sont triables.

Chaque piste représente des observations de valeurs. À forte densité, une colonne de pixels conserve le minimum, le maximum et la dernière valeur ; les échantillons sources restent en mémoire. Le survol affiche la dernière observation valide, son temps natif et l'unité lorsqu'elle est connue. Les marqueurs rouges indiquent les états de disponibilité invalides enregistrés.

## Mesures, calculs et disponibilité

Les temps sont ceux du provider Counters d'Unreal, sur l'axe natif utilisé pour les événements de timing. L'heure de réception réseau ne sert pas à positionner les observations. Les identités `CounterId`, types `Int64`/`Double` et propriété `ResetEveryFrame` sont conservés.

Les entiers sont conservés et affichés en 64 bits, y compris au-delà de la précision exacte d'un `double`. Les extrema et leur tri utilisent les valeurs sources. La moyenne est une **moyenne dérivée des échantillons**, sans pondération temporelle ; les coordonnées des graphiques utilisent une approximation flottante. Les unités des compteurs génériques ne sont pas fournies par cet export et restent indisponibles. Les métriques Meta gardent l'unité native de leur nom, sans conversion devinée.

Pour Meta, le plugin émet le compagnon global `<metric>/available` et les compagnons `<metric>/uint/available` et `<metric>/float/available`. L'application utilise la disponibilité du type concerné. Une valeur entière valide ne rend donc pas valide une ancienne valeur flottante. Les anciennes captures avec un seul type peuvent utiliser le compagnon global ; si les deux types existent sans compagnons par type, leur disponibilité reste ambiguë et les valeurs brutes sont conservées sans être présentées comme des observations exploitables.

Les valeurs non finies et indisponibles sont exclues des statistiques et du graphique. Un zéro réellement mesuré reste valide. Une reprise du flag de disponibilité ne rafraîchit pas une ancienne valeur : il faut une nouvelle observation valide. Un compagnon absent est explicitement signalé.

Le temps GPU scalaire fourni par Meta reste une observation du runtime. La frame GPU d'exécution et la période sous-jacente de mesure ne sont pas connues ; aucune barre d'exécution n'est créée à partir de ce scalaire. Les événements détaillés `CyInsightsQuest.Source`, `CyInsightsQuest.Sample` et `CyInsights.Sync` restent dans la trace ; leur décodeur dédié n'est pas encore intégré.

## Pont d'analyse et limites

Cette étape utilise encore le `UnrealInsights.exe` installé, choisi dans Session. L'application lance son export sans interface en arrière-plan : catalogue `TimingInsights.ExportCounters`, puis valeurs `TimingInsights.ExportCounterValues`. Aucun moteur ni analyseur Epic n'est embarqué ou recompilé. La trace originale est préservée ; les CSV sont des caches d'analyse.

Les plafonds de taille, de définitions et d'échantillons ont été supprimés. L'ouverture du timing importe seulement le catalogue : **Load counter samples** lance ensuite l'analyse des valeurs à la demande, avec annulation. Les valeurs sont encore conservées en mémoire ; leur stockage paginé reste à réaliser. Une série présentant des lignes invalides devient indisponible avec une raison ; aucun sous-ensemble partiel de cette série n'est publié. Les autres séries déjà validées restent consultables. Les timings utilisent maintenant un [index sur disque](LargeTraceLoading.md).

L'export Epic sélectionne les compteurs par nom plutôt que par identifiant. Les noms dupliqués ou ne permettant pas une requête exacte (jokers, listes, guillemets, caractères de contrôle) ne sont pas associés arbitrairement. Les fichiers de valeurs portent uniquement un identifiant numérique. Un helper d'analyse versionné, l'indexation sur disque, les comparaisons de compteurs et l'analyse en direct restent à développer. L'export des valeurs dispose d'un délai maximal de dix minutes ; son annulation ne termine que le processus d'analyse détenu par l'application.

## Validation

Les sept suites de composants passent **232 vérifications**, dont **45** pour les compteurs : identités/types/unités, entiers exacts, invalidité indépendante par type, disponibilité et reprise, statistiques d'intervalle, conservation des extrema à forte densité, CSV malformés et terminaison de l'import asynchrone.

La même capture réseau réelle produite avec UE 5.5 a été exportée/importée avec les analyseurs installés **5.5, 5.6, 5.7 et 5.8**. La trace comporte **1 657 829 scopes de timing, 2 002 définitions, 274 compteurs avec valeurs et 1 997 547 échantillons**. Le nombre brut de lignes de timing varie selon l’analyseur : l’export 5.8 ajoute des frames mesurées, désormais reconnues séparément des timers ; ce supplément ne constitue pas des scopes CPU supplémentaires. Les chemins de cache contiennent volontairement des espaces. Cela vérifie ce pont d'export sur cette capture ; cela ne valide pas des captures produites par chacune des quatre versions, ni des mesures prises sur Quest.

Les sources de l'interface compilent en objets et bibliothèques statiques. Le module Quest modifié compile pour Android ARM64 Development sous UE 5.5–5.8, avec les réponses de compilation UBT existantes (`build/counter-quest-5.5.log` à `5.8.log`). Aucun nouvel exécutable d'interface n'est lié ou lancé : la vérification visuelle reste suspendue à l'alerte Kaspersky non résolue décrite dans [SecurityReview.md](SecurityReview.md). Aucun test sur un jeu packagé sur casque n'a encore été effectué.
