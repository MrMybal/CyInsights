# Statistiques Unreal par frame

Le plugin enregistre les statistiques dans la même `.utrace` que le timing CPU/GPU, y compris lors d'une capture réseau. Aucun logiciel de monitoring supplémentaire et aucune modification du moteur ne sont nécessaires. La collecte est activée par défaut pendant une capture CyInsights dans les configurations autorisées : Editor, Debug, DebugGame et Development.

## Capture

```text
CyInsights.RecordStats 1
CyInsights.RecordStatGroups 0
```

Le défaut collecte uniquement sept observations de base, sans activer le master Stats ni les lectures mémoire du processus. Les réglages sont accessibles dans Unreal et depuis le PC : [CaptureProfiles](CaptureProfiles.md). Pour ajouter les groupes compilés et actifs disponibles auprès du système Stats d'Unreal, choisir les groupes avant de démarrer la capture :

```text
CyInsights.RecordStatGroups 1
CyInsights.StatsGroups Game,UObjects,GPU
```

Le choix est pris au début de chaque capture. `*` sélectionne tous les groupes actifs. `CyInsights.RecordStats 0` désactive ce collecteur pour la capture suivante. Le canal `cyinsightsstats` suit l'option snapshots ; `stats` est ajouté seulement avec l'agrégation optionnelle. Le collecteur n'exécute pas automatiquement les commandes HUD `stat ...` et ne réactive pas les groupes désactivés ou supprimés à la compilation. Activer les groupes utiles dans le jeu avant de capturer si nécessaire.

Les observations de base sont enregistrées indépendamment du filtre de groupes : derniers temps GameThread, RenderThread, RHIThread et GPU disponibles, intervalle de frame observé et slots UObject alloués/capacité du tableau. Les slots UObject sont désactivables séparément ; ils ne constituent pas un décompte garanti de tous les objets vivants. Les lectures de mémoire physique et virtuelle du processus sont optionnelles, désactivées par défaut.

Les autres groupes fournissent les agrégats de frame disponibles : temps inclusifs/exclusifs en millisecondes, nombre d'appels, compteurs entiers, mémoire en octets et valeurs flottantes. L'unité des flottants génériques est inconnue et reste `value`. Ce n'est pas une reproduction de chaque affichage HUD : certains graphiques HUD ont des moyennes lissées ou des calculs propres qui ne sont pas publiés comme statistiques.

## Application

Ouvrir **Frame stats** dans la barre de navigation. Le panneau se place sous le timing et charge les snapshots en arrière-plan. Choisir un groupe, rechercher une statistique et sélectionner la métrique. **Curve in timeline** ajoute sa courbe au-dessus des pistes CPU/GPU, avec le même axe temporel. Les boutons **Unit / GPU / UObjects / Memory curves** ajoutent les métriques du groupe. Chaque courbe conserve sa propre échelle et son unité.

Le tableau affiche une valeur par frame native, l'heure de début, l'unité, les identités game/stats de la source, l'heure d'observation et la provenance. Sans sélection, il suit la fenêtre visible. Maintenir **Shift** et glisser sur la règle de timing pour sélectionner un intervalle : le tableau et les statistiques Counters suivent ce range. Cliquer une ligne sélectionne sa frame ; double-cliquer zoome dessus.

Une frame sans observation affiche **Unavailable**, sans reprendre la valeur précédente. Les valeurs entières restent exactes en 64 bits. Les courbes denses conservent leurs extrema par colonne de pixels ; leurs coordonnées et les segments reliant les points sont un affichage dérivé. Les segments des snapshots ne traversent pas une discontinuité connue des frames sources.

Les anciens `.utrace` peuvent utiliser **Load frame statistics** pour consulter leurs compteurs natifs existants par frame. Les données qui n'ont jamais été enregistrées ne peuvent pas être reconstruites. **Load native counters too** complète les nouveaux snapshots avec les compteurs classiques.

## Synchronisation et disponibilité

Chaque snapshot conserve séparément début/fin de l'intervalle source, heure de réception du résultat, numéro game frame et numéro stats frame. Les identités de frame affichées par le provider natif ne sont pas assimilées aux compteurs game/stats. Les résultats Stats tardifs sont placés au milieu de leur intervalle source, pas dans la frame courante à leur réception.

Le collecteur conserve les correspondances des 256 dernières frames, puis laisse la correspondance indisponible si un résultat arrive trop tard. Ce tampon de synchronisation ne limite ni la durée de capture, ni sa taille, ni le nombre d'événements. Les premières/dernières frames d'une capture peuvent avoir des correspondances ou résultats absents.

Les scalaires de type `stat unit` sont les dernières lectures publiques du moteur. Le moteur ne fournit pas leur identité exacte de frame d'exécution ; Render/RHI/GPU peuvent être retardés. Ils restent marqués **Execution frame unknown**, même lorsqu'un intervalle d'observation game frame existe. Les durées agrégées sont marquées **Derived aggregate**. Les lectures cycle à zéro sans preuve de disponibilité restent indisponibles. Les métriques Meta ont leurs propres règles d'observation décrites dans [CounterAnalysis](CounterAnalysis.md).

## Lecture et cache

Les événements `CyInsightsStats.Spec/Sample` résident dans la trace originale. Le lecteur natif local 5.5–5.8 les décode en flux dans `frame-stats.cystats` (`CYFS0001`), un cache jetable et reconstruisible. Son chargement ne relance pas Unreal Insights. Les valeurs actuellement chargées restent en mémoire ; leur pagination est encore à réaliser. Aucun plafond de taille de trace ou d'échantillons n'est ajouté.

Un miroir Counters garde les métriques consultables avec les analyseurs habituels. Sans le décodeur spécialisé, ce miroir conserve seulement l'heure d'observation et ne fournit pas l'association exacte à l'intervalle source.

## Validation du 11 octobre 2026

Les 16 suites CTest passent, dont 62 vérifications de compteurs. Deux captures réseau successives d'un hôte UE 5.8/NullRHI détenu par le test enregistrent 1 738 455 et 2 055 069 observations, avec le même nombre d'échantillons après décodage. La première trace est ouverte dans l'application : 1 926 métriques de snapshots, courbe synchronisée et tableau avec les mêmes indices de frame que la timeline. Le test `--frame-stats-range-smoke` sélectionne réellement la règle à la souris : le tableau passe de cinq frames visibles à une frame sélectionnée et conserve sa courbe. Le test de navigation passe aussi avec ces valeurs chargées. NullRHI ne valide aucun temps GPU matériel.

Le plugin compile et lie sous Win64 UE 5.5, 5.6, 5.7 et 5.8 ; les nouveaux modules compilent aussi pour Android ARM64 UE 5.8/NDK r27c. Aucun jeu Android n'est lié ou exécuté pour ce contrôle. Le module Runtime compile avec les headers UE 4.27, mais la validation Editor complète est bloquée par des headers générés et bibliothèques d'import manquants dans l'installation locale. Les quatre lecteurs de snapshots 5.5–5.8 compilent ; la lecture de cette nouvelle capture est validée avec l'analyseur 5.8. Les autres versions et une capture sur Quest restent à tester.
