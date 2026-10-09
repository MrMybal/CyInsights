# Comparaison des timings natifs

Le panneau **Capture comparison** compare les scopes de timing Unreal de deux captures. Il utilise la même application C++/Dear ImGui, sans interface web. Les captures `.utrace` et `.cycapture` sont acceptées. Les pistes GPU natives sont incluses lorsqu'elles existent dans la trace ; la comparaison détaillée ReShade reste une fonction distincte.

## Utilisation

1. Ouvrir la capture **A** et attendre la fin de son analyse.
2. Choisir **Analysis > Compare native capture…**, ou **Compare > Choose capture B…**.
3. Sélectionner l'intervalle A sur la règle de Timing. Sans sélection, l'intervalle visible est utilisé.
4. Saisir les bornes temporelles de **B** puis **Apply B interval**. **Full B** utilise toute la capture. **Use B native frame interval** utilise une frame Game mesurée lorsque l’export la fournit. Sinon, **Use B tick interval** utilise un candidat déduit d’un scope `FEngineLoop::Tick`/`EngineLoopTick` du GameThread. Les numéros restent locaux à leur capture et ne constituent pas une correspondance avec A.
5. Filtrer par nom et domaine de pistes CPU/GPU, puis trier les colonnes de coût ou d'écart. Un double-clic ouvre le timer A dans Timing seulement si le groupe A contient un seul timer.

Le tableau affiche les occurrences complètes qui chevauchent l'intervalle, les coûts inclusifs/exclusifs A et B, l'écart **B − A** et son pourcentage relatif à A. Une valeur positive signifie un coût plus élevé dans B. Les métadonnées différentes ou manquantes (projet, moteur, plateforme, API, CPU, GPU) sont signalées ; l'analyseur choisi pour B est affiché.

Trois modes sont disponibles :

| Mode | Calcul |
| --- | --- |
| Totals | Somme des durées limitées à l'intervalle choisi |
| Per second | Cette somme divisée par la durée propre de l'intervalle A ou B |
| Per occurrence | Cette somme divisée par le nombre de scopes complets chevauchant l'intervalle |

Les coûts restent limités à l'intervalle : une moyenne par occurrence ne reconstitue pas la durée entière d'un scope coupé par une borne. Les sommes inclusives peuvent se chevaucher ; elles ne représentent pas le temps mural ou un pourcentage d'occupation GPU. Les coûts exclusifs sont déduits de l’imbrication des enfants immédiats dans les pistes natives.

## Correspondances et disponibilité

Les groupes sont comparés par **nom de timer et domaine de piste**. Le domaine GPU est déduit du nom de piste, comme dans le modèle actuel de timing ; il ne provient pas d'un nouveau provider matériel. Les `TimerId` natifs sont conservés, mais restent locaux à chaque session. Une égalité de nombres entre captures ne prouve pas une identité commune.

- **Name candidate** : un seul identifiant natif connu de chaque côté ; correspondance candidate par nom, sans identité persistante garantie.
- **Ambiguous name group** : plusieurs identifiants du même nom dans une capture, ou identité native manquante. Le tableau compare leurs sommes agrégées, conserve la liste des identifiants dans le survol et ne choisit pas une paire arbitraire.
- **Only A / Only B** : aucun groupe correspondant dans le catalogue de scopes importés de l'autre capture.

Un timer connu mais sans observation complète dans l'intervalle est distinct d'un timer absent du catalogue. Ses coûts et écarts restent **N/A**. Les scopes incomplets sont comptés séparément dans le survol et exclus des durées. Un pourcentage avec une référence A nulle reste indisponible ; un écart absolu connu peut être affiché. Les deux captures conservent leurs propres horloges et leurs bornes indépendantes : aucune synchronisation artificielle n'est appliquée.

## Analyse et limites

L'extraction de B, l'export natif, l'import CSV, l'indexation et les changements d'intervalle B se font dans un worker. Les contrôles utilisant B sont suspendus pendant cette opération ; Timing A reste utilisable. Un échec ou une annulation conserve la référence B précédente. L'annulation ne termine que l'analyseur lancé et détenu par cette opération. Le temps maximal d'attente de son export est dix minutes ; l'extraction et l'import/indexation ont leur propre granularité d'annulation.

Le pont utilise toujours un `UnrealInsights.exe` installé. Pour une archive indiquant UE 5.5–5.8, il cherche l'installation correspondante à côté de l'installation sélectionnée, puis conserve l'analyseur sélectionné si elle n'existe pas. Aucun binaire Epic n'est redistribué, modifié ou recompilé. Les plafonds de CSV et d'événements ont été supprimés. A et B utilisent un index sur disque et des statistiques calculées sur les événements complets de l'intervalle ; l'agrégation visuelle par pixel ne réduit pas leurs totaux. Chaque capture conserve ses métadonnées et son cache ; voir [chargement des grandes traces](LargeTraceLoading.md).

La comparaison de compteurs/Meta, de frames détaillées ReShade choisies explicitement en A/B, de shaders/ressources par identité persistante, les différences graphiques entre deux timelines et les correspondances d'objets Unreal restent à développer. Les logs natifs nécessitent aussi un helper d'analyse dédié : les commandes d'export actuelles d'Insights ne les fournissent pas.

## Vérification

Les sept suites de composants passent **232 vérifications**, dont **45** pour les comparaisons : conservation des coûts, coupures d'intervalle, domaines CPU/GPU, identifiants locaux, noms ambigus, scopes incomplets, référence nulle, normalisation par durée/occurrences, import asynchrone, invalidité des intervalles et annulation d'un analyseur de test détenu par l'opération. Les suites d’import et de timing vérifient aussi la reconnaissance des frames, leurs indices complets, leur namespace distinct des timers et leur exclusion des coûts de scopes.

Le parcours réel archive → extraction → export Epic → import → comparaison a été vérifié sur deux captures réseau UE 5.5 : **1 499 153** et **1 657 829** événements. Il retourne **109 groupes observés**, dont **107 correspondances candidates**, **un groupe ambigu** et **un groupe sans correspondant**. L'auto-comparaison de A donne des écarts nuls. Les fichiers originaux sont conservés. Ces captures utilisent NullRHI et ne valident pas un GPU matériel.

L’export 5.8 des mêmes traces brutes comporte **1 505 825** et **1 665 200** lignes de timing, dont **6 671** et **7 370** frames Game complètes reconnues pour l’aperçu, plus des frames incomplètes. Après séparation de ces lignes, les mêmes **109 groupes de scopes observés** restent comparés. Cet export normalise aussi les IDs de timers issus de métadonnées : il produit **108 candidats**, aucun groupe ambigu et un groupe sans correspondant. Les identités et correspondances peuvent donc dépendre de la version du pont ; sélectionner l’analyseur correspondant au moteur reste nécessaire. Les traces extraites et brutes ont une empreinte SHA-256 identique.

Les mises à jour réelles d’intervalle B ont été vérifiées sur un tick candidat 5.5 (`115.113704 .. 115.113929 s`, **93 scopes**) et une frame mesurée exportée par 5.8 (`115.113709 .. 115.113928 s`, **91 scopes**). Les bornes sont différentes, sans correspondance artificielle ni ajout des frames aux coûts. Les deux auto-comparaisons donnent un écart nul. Journaux : `build/comparison-csv-5.5.log`, `build/comparison-csv-5.8.log` et `build/comparison-real-5.5-interval.log`.

L'interface compile en objets et bibliothèques statiques. Aucun nouvel exécutable d'interface n'est lié, lancé ou visuellement vérifié, en raison de l'alerte Kaspersky non résolue sur l'ancien exécutable ; voir [SecurityReview.md](SecurityReview.md). Les diagnostics utilisent des programmes console isolés et l'analyseur Epic sans interface.
