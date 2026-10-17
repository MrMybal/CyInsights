# Profils de capture

## Défaut léger

Une nouvelle configuration conserve les scopes CPU/GPU natifs, les frames, les ancrages et les compteurs. Le collecteur de snapshots ajoute sept observations par frame : derniers temps GameThread, RenderThread, RHIThread et GPU, intervalle de frame observé, slots UObject alloués et capacité du tableau. Les lectures indisponibles restent signalées ; les scalaires GPU/RT/RHI ne garantissent pas l'identité de la frame d'exécution.

Sur Quest, le profil de base demande les groupes GPU natifs par frame de rendu et leurs timestamps GPU, ainsi que les groupes RDG imbriqués (`r.RDG.Events=2`). Leur présence dans la trace dépend des timestamps du RHI et des scopes instrumentés compilés. Le plugin prépare les timestamps au lancement sur les casques reconnus. Les observations Meta complémentaires sont désormais prises chaque frame de jeu par défaut (`quest_hz=0`), sans identité GPU exacte fournie par le runtime. Voir [frames et passes GPU Quest](QuestGpuFrames.md).

Par défaut, aucune agrégation des groupes Stats supplémentaires, lecture de mémoire du processus, instrumentation Task/Network/File/LoadTime/Log/LLM, capture ReShade détaillée, timestamp par commande ou copie d'image/texture n'est demandée. Le chargement ReShade et le lancement de son recorder sont également désactivés dans les nouveaux réglages. Les choix explicites déjà enregistrés dans un projet restent respectés.

## Configurer dans Unreal

**Project Settings > Plugins > CyInsights**, ou le menu CyInsights **Capture settings**. Les valeurs sont enregistrées dans `Config/DefaultGame.ini`, section `[/Script/CyInsights.CyInsightsSettings]`, et s'appliquent à la capture suivante. Le service fonctionne uniquement dans les configurations autorisées Editor/Debug/DebugGame/Development.

- Basic : scopes CPU/GPU, snapshots par frame et décomptes UObject.
- Statistics : lectures mémoire du processus, agrégation de groupes Stats et liste de groupes. `Game,UObjects,GPU` est la sélection initiale ; elle n'est utilisée qu'après activation de l'agrégation. `*` choisit tous les groupes actifs.
- Extra native sources : tâches, réseau, fichiers, chargements, logs et tags mémoire.
- GPU natif : groupes de base par défaut, ou noms de passes RDG individuelles en complément pendant toute la capture (`gpu_passes`), selon la couverture du profiler installé.
- Quest : observations Meta par frame (0, défaut), ou polling optionnel de 1 à 120 Hz ; préparation des timestamps GPU au démarrage.
- GPU snapshot : demande au démarrage, 1 à 8 frames, descriptors, barrières, timestamps par commande et copies d'images/textures séparément.

Les groupes absents, désactivés ou retirés à la compilation ne sont pas reconstruits. La collecte n'exécute pas automatiquement les commandes HUD `stat ...`. Le réseau dépend de NetTrace compilé et de son niveau ; les tags mémoire nécessitent LLM. Les allocations détaillées nécessitent toujours les flags d'instrumentation au lancement. Voir [NativeActivities](NativeActivities.md).

Les commandes explicites `CyInsights.RecordStats`, `CyInsights.RecordStatGroups` et `CyInsights.StatsGroups` restent disponibles. Le profil de base n'active pas le master Stats. L'activation optionnelle est équilibrée à l'arrêt ; les canaux gérés sont nettoyés entre captures. Les moteurs fournissant l'énumération publique des canaux permettent aussi de restaurer leur état précédent. UE 4.27 ne fournit pas cette énumération : les canaux gérés sont désactivés après la capture. Une trace déjà active est toujours refusée avant de changer ses canaux.

## Configurer depuis le PC

Dans **Session > Capture options**, **Use plugin project defaults** est coché initialement. **Connect to device** récupère les valeurs du plugin ; **Copy plugin defaults for this capture** permet de partir de celles-ci et de les modifier. Décocher l'option pour utiliser les réglages du PC ; **Basic profile** les réinitialise au profil léger. Les réglages se choisissent avant **Start remote trace**.

Une surcharge réseau ne modifie ni le fichier INI du projet, ni ses CVars, ni le profil de la capture suivante. Le manifeste `.cycapture` conserve les options réellement appliquées dans `capture_options`. Un ancien plugin continue à recevoir ses commandes habituelles ; une surcharge explicite lui est refusée s'il n'annonce pas le protocole d'options v1.

Pendant l'enregistrement, **Request extra GPU snapshot** offre une demande ponctuelle au recorder Windows connecté. Ses copies d'images/textures restent décochées initialement. Le refus d'une demande GPU conserve la capture native en cours et affiche sa raison.

**Limite actuelle :** cette commande nécessite un collecteur Windows chargé et un recorder connecté. Les fichiers détaillés du recorder ne sont pas encore transférés dans le `.cycapture` par le récepteur réseau natif (`gpu_payload_transfer=false`). Le workflow d'export du collecteur reste séparé. L’interception détaillée des commandes/pipelines/ressources Vulkan Quest et les backends console restent indisponibles ; les scopes/passes GPU natifs sont une source distincte. Sur Quest, les options de métriques Meta sont déjà appliquées à la trace native.

`gpu_readbacks` / l'ancien réglage `bSaveBuffers` autorisent les copies d'images/textures prises en charge, dont l'image finale. Les octets de buffers GPU génériques ne disposent pas encore d'un backend de readback. Aucun contenu GPU n'est fabriqué pour compléter une capture.

## IA et protocole

Les commandes `hello/status` annoncent `capture_defaults` et `capture_capabilities`. `start` accepte un objet `capture_options` v1 ; les champs de cet objet sont strictement typés, les valeurs inconnues sont refusées avant de démarrer. Le client PC envoie son profil complet lorsqu'une surcharge est sélectionnée. La commande `gpu` requiert l'identifiant de capture détenu par cette connexion.

`capture_start` expose les mêmes réglages dans le SDK/MCP. Omettre l'objet utilise les valeurs du plugin ; fournir un objet partiel via le SDK part du profil de base du client. `capture_gpu` demande un snapshot borné pendant une capture détenue par l'application ; son acceptation signifie que la demande est en file, pas que des données GPU ont été reçues. `capture_status` expose valeurs par défaut, capacités, options appliquées et raison d'acceptation/refus GPU.

## Validation du 11 octobre 2026

Quatre captures réseau successives d'un hôte UE 5.8/NullRHI appartenant au test : léger **18 269 observations / 7 métriques actives**, groupes Game/UObjects/GPU et mémoire **76 936 / 83**, retour au léger **19 557 / 7**, snapshots désactivés **0 / 0**. Le décodage natif confirme exactement les comptes du manifeste. Unreal peut rejouer les déclarations importantes des métriques précédentes ; une déclaration sans échantillon n'est pas une collecte de valeurs. Les demandes mal typées, champs inconnus, mauvais identifiant et demande GPU sans recorder sont refusés. Les valeurs par défaut du projet restent identiques entre captures.

Les 16 suites CTest passent, dont 68 contrôles réseau/profils ; les tests du compagnon valident le contrat et le passage des options. Le plugin Runtime/Editor est compilé et lié sous Windows UE 5.5/5.8 ; les modules Android ARM64 5.8 compilent sans jeu lié ni validation matérielle. L'application et le collecteur Windows compilent. Ces contrôles fonctionnels ne mesurent pas le surcoût CPU/GPU sur un jeu réel ou un Quest.

Les nouveaux réglages sont analysés par UHT 4.27, mais la compilation Runtime/Editor complète de cette révision reste bloquée par l'installation locale : `DeveloperSettings.generated.h`, `ToolMenuOwner.generated.h` et bibliothèques d'import Core/Sockets manquants. Le module Trace compile sans PCH. Aucun fichier manquant n'est généré artificiellement et le moteur n'est pas recompilé.
