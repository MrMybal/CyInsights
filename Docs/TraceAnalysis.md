# Distributions de temps et tâches

Release de test : `bin/Release-20261009.5/CyInsightsApp.exe`. Interface native C++/Dear ImGui, avec deux nouveaux panneaux dockables dans **Timing Insights** : **Distributions** et **Tasks**. Les analyses travaillent en arrière-plan, avec annulation ; leur publication ne réinitialise pas les gestes de navigation de la timeline.

## Distributions

**Samples** propose trois familles :

- **Per-frame thread cost** : GameThread, RenderThread ou GPU, avec total observé, travail hors attentes reconnues ou attentes reconnues. Les mesures viennent des diagnostics par frame, qui réunissent les intervalles imbriqués sans compter deux fois leur recouvrement. Les ticks reconstruits restent nommés « Tick candidate ». Le GPU absent reste indisponible.
- **Native scope durations** : choix de chaque piste native et de son identité, avec scopes de profondeur zéro ou toutes les profondeurs. Les événements bruts sont lus depuis l'index sur disque ; les agrégats de rendu de la timeline ne servent pas d'échantillons. Les durées sont inclusives : les scopes imbriqués sont des échantillons distincts, pas une somme d'utilisation du thread.
- **Native task durations** : exécution, délai `Scheduled → Started` dans la file, ou délai `Launched → Scheduled` avant programmation ; filtrage par thread d'exécution. Charger les données dans **Tasks** au préalable. Les deux délais sont calculés à partir des phases enregistrées ; ils ne prouvent pas à eux seuls une cause critique ou une dépendance particulière.

**Whole capture** analyse toute la capture et désactive le suivi ; **Use selected interval** copie la sélection de Timing, ou sa fenêtre visible en l'absence de sélection, et réactive le suivi. **Analyze** prépare un résultat pour ces paramètres. **Follow timeline selection**, activé par défaut, actualise les analyses ouvertes après une sélection sur la règle (Shift + glissement gauche), au relâchement et après 120 ms. Une navigation ou un zoom seuls ne changent pas la population. Un changement des autres paramètres signale qu'il faut recalculer. Le clic droit sur un scope ouvre sa distribution par nom exact ; le double-clic filtre ce nom dans Timing. Voir [les interactions et optimisations](TimelineFunctionAnalysis.md).

Le graphique contient 64 tranches, une courbe cumulative (CDF) et des axes de durée (**Log durations**) et de fréquence (**Log counts**) linéaires ou logarithmiques. L'axe des durées utilise `log(1 + durée / 1 µs)` afin de conserver les zéros ; il est activé par défaut pour distinguer les valeurs courantes malgré les pics extrêmes. Minimum, moyenne, maximum, P50/P90/P95/P99 et nombre d'échantillons indisponibles sont affichés. Les percentiles par frame sont exacts, selon le rang supérieur. Ceux des scopes et tâches sont estimés depuis l'histogramme, marqués `~` ; le survol expose les bornes contenant le quantile. Avec une valeur extrême sur un axe linéaire, ces bornes peuvent être larges.

Toutes les valeurs valides sont comptées, y compris les zéros réellement mesurés. Une durée sans fin enregistrée, une observation contradictoire de tâche ou un domaine absent ne devient pas zéro. Pour les tâches, une phase initiale absente ne permet pas d'attribuer l'échantillon à un intervalle et ne participe pas à cette distribution.

L'échantillon doit commencer dans `[début, fin)` de l'analyse ; sa durée complète est conservée, même s'il dépasse la sélection. Le tableau initial donne les 128 valeurs les plus longues. Un clic dans une tranche permet de parcourir **toutes** ses valeurs, par pages de 128. Cette taille concerne uniquement l'affichage : il n'y a pas de limite au nombre d'événements acceptés. Un double-clic cadre la durée choisie dans Timing.

## Tasks

La lecture démarre à l'ouverture du panneau. Le suivi de sélection est partagé avec Distributions ; les recherches ouvertes se relancent au relâchement, sans interrompre le geste. Quand le canal Unreal `task` a été enregistré, le tableau contient les identifiants natifs 64 bits, noms disponibles, threads d'exécution, durées et délais. Rechercher par nom ou identifiant, choisir le thread et l'intervalle, puis **Find tasks**. Les tâches dont l'exécution recouvre l'intervalle apparaissent ; les tâches encore en attente utilisent leur contexte de vie observé. Les lignes sont paginées sans limiter le total.

Une sélection affiche les sept phases disponibles : **Created, Launched, Scheduled, Started, Finished, Completed, Destroyed**. **Show execution in Timing** ou un double-clic cadre l'exécution sur l'axe temporel commun ; le thread natif est rendu visible s'il est connu. **Load recorded links** lit les événements `SubsequentAdded`, avec leurs directions et timestamps natifs. Le type de prérequis, les listes d'attente `WaitingStarted/Finished`, les relations historiques `NestedAdded` et le chemin critique ne sont pas encore décodés. Les sources `.utrace` sont conservées intégralement.

La vue globale inclut aussi les observations de tâches antérieures ou postérieures au domaine des scopes CPU. Un intervalle d'exécution sans scopes CPU correspondants peut être cadré, mais la timeline n'invente pas ces scopes. Cette version présente le cycle de vie dans le panneau Tasks ; elle ne dessine pas encore une piste dédiée à chaque tâche native.

Un enregistrement commencé en cours de jeu peut manquer les premières phases. La taille, le statut awaitable et les durées nécessitant ces phases restent indisponibles. Des observations contradictoires sont signalées. Les identifiants sont locaux à la capture et ne constituent pas une correspondance d'objets entre deux sessions.

Sans événements natifs TaskTrace, le panneau propose **CPU task scopes (candidates)** : scopes mesurés dont le nom indique `Execute…Task` ou `ParallelFor`, avec exclusion des attentes reconnues. Ce sont des candidats heuristiques, clairement séparés des identifiants et cycles de vie natifs. Ni délais de file ni liens ne sont inventés. Une ancienne trace ne permet pas de retrouver un canal jamais enregistré.

## Collecte et dépendances

Les canaux par défaut du plugin incluent désormais `task` : `default,cpu,gpu,frame,bookmark,log,counters,task`. Un réglage explicite de canaux conserve sa priorité. Le canal est disponible dans les installations UE 5.5–5.8 examinées ; le plugin conserve ses restrictions Editor/Debug/DebugGame/Development.

L'application utilise un petit adaptateur de sa propre source dans `NativeTasks/5.x/CyNativeTasks.exe`, correspondant à l'installation choisie via **Session > Select UnrealInsights.exe**. Il lit la trace avec les bibliothèques **déjà installées** d'Epic. Les DLLs, headers et bibliothèques Unreal ne sont pas redistribués, et aucune source du moteur n'est recompilée ou modifiée. Cette dépendance nécessite une installation Unreal licenciée et ses prérequis Windows. L'absence du lecteur ou des DLLs donne une erreur visible et laisse l'inspection des candidats CPU possible.

Le journal de phases est écrit progressivement puis indexé sur disque. Le cache de travail de 4096 tâches se vide et se relit ; il ne limite pas les tâches retenues. L'index d'identifiants et les catalogues de noms restent en mémoire. Un export incomplet n'est jamais publié comme jeu de données terminé. L'annulation termine uniquement le processus de lecture lancé par l'application.

## Validation

- **452 vérifications / 12 suites**, aucune en échec : distribution complète, quantiles, axes logarithmiques, zéros/indisponibles, tranches et pagination, annulation, identifiants dépassant la précision d'un double, phases absentes, conflits, liens, 6000 tâches au-delà du cache de travail, observations hors domaine CPU, lecture asynchrone et conservation des sources.
- Les quatre adaptateurs inchangés de la release 9.4, compilés contre les installations locales UE 5.5–5.8 sans reconstruire le moteur, lisent une capture UE 5.6 terminée normalement. Leurs exports sont identiques octet pour octet : **64 406 événements, 12 716 tâches, 84 liens**. Parmi ces tâches, 6749 ont une durée d'exécution complète et 6712 un délai de file complet ; les autres observations restent conservées avec les phases manquantes. Cela vérifie ce protocole de trace, pas tous les futurs schémas Unreal possibles.
- La trace utilisateur `20261008_142920.utrace` ne contient pas de tâches TaskTrace natives ; l'application conserve ses **14 192 325 événements de timing** et propose les candidats CPU.

Preuves : `build/release-20261009.5-audit`, manifest de release et [instructions de test](TestingRelease.md). Les autres providers Insights ne sont pas tous implémentés ; ces panneaux ne constituent pas encore une parité complète avec Unreal Insights.
