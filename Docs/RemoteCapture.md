# Capture réseau depuis le PC

1. Activer CyInsights et lancer le jeu en **Development, Debug ou DebugGame** sur le PC cible ou le Quest. Le mode Editor est autorisé. Le service démarre automatiquement ; Shipping, Test et Server sont exclus.
2. Dans **Session** sur le PC d'analyse, saisir l'**IPv4** du jeu/casque, puis **Connect to device** pour vérifier la connexion ou directement **Start remote trace**.
3. Utiliser **Stop remote trace**, puis **Open received capture**. La durée facultative permet l'arrêt automatique ; `0` conserve l'arrêt manuel.

Depuis la correction du 13 octobre, la capture Quest conserve **le jeu déjà
ouvert par défaut**. Si le profiling GPU a été activé dans MQDH avant son
lancement, laisser `Restart the Quest game before capture (optional)` décoché.
Le client vérifie l'état du pilote sans réactiver son mode ni envoyer de
commande d'arrêt/lancement. Un mode Meta indisponible conserve la capture
native et signale la raison ; il ne déclenche pas une relance implicite.

La relance explicite reste disponible : l'activité de lancement est résolue
avant d'arrêter le seul package sélectionné, puis lancée directement avec
`am start`, sans injection d'événements Monkey. L'ancienne connexion est fermée
avant la relance ; le client vérifie la nouvelle identité de processus, le
package et la première frame quand le plugin expose cette information. La
reconnexion a un délai de 120 secondes avec phase, progression et dernière
erreur visibles. L'arrêt interrompt les attentes de démarrage et les commandes
ADB appartenant à l'app ; aucun processus de jeu supplémentaire n'est contrôlé.

Le listener Android active `SO_REUSEADDR` pour rouvrir son port après fermeture
d'une ancienne connexion. `SO_REUSEPORT` n'est pas activé. Les échecs de bind
incluent leur code dans le log Unreal et sont réessayés. Les nouveaux champs
`runtime_ready` et `game_frame` sont facultatifs : les anciens plugins restent
utilisables pour la capture d'un jeu déjà ouvert.

Un fichier `capture_control_*.jsonl` dans le dossier de sortie conserve les
phases et erreurs du client, sans payloads de trace ni export de données à un
service externe. Le SDK expose `phase`, `startupElapsed`, `finishing` et
`diagnosticLog`. Le temps de démarrage est distinct du temps enregistré ;
aucune archive complète n'est publiée lorsqu'aucune trace valide n'a été reçue.

Validation locale : tests TCP + faux ADB couvrant le jeu déjà ouvert sans
réactivation/relance, la nouvelle identité après relance explicite, l'attente
de première frame, l'arrêt pendant un `hello` silencieux, le repli vers la trace
native si la préparation Meta échoue et le journal final. Les 45 contrôles
Meta et 77 contrôles réseau passent, ainsi que les 21 suites C++, huit tests
SDK et onze tests Python. Le plugin modifié compile sur Meta UE 5.5.3 et
UE 5.8.3 Android. Aucun lancement/arrêt de Beyond n'a été effectué par ces tests ;
le succès d'une nouvelle capture matérielle avec ce correctif reste à vérifier.

Le PC et le jeu doivent pouvoir communiquer sur leur réseau local. Le port de commande est **TCP 41855**. Le PC ouvre un port TCP de réception disponible sur l'interface utilisée pour joindre le jeu ; son pare-feu doit autoriser l'application à recevoir ce flux. L'adresse de retour est déduite de la connexion : aucune adresse du PC à saisir dans le casque, aucune application de monitoring à lancer. Cette première version utilise IPv4, sans découverte automatique ni transport Internet authentifié/chiffré.

Les captures sont enregistrées sur le PC sous `Captures/remote_<identifiant>.cycapture`, à côté de l'application. Le répertoire de préparation conserve la trace et le manifeste. Sur le jeu, `Saved/CyInsights/<session>` conserve les métadonnées/ancrages locaux ; en mode réseau, le fichier natif est écrit sur le PC.

## Données et limites

Les octets du flux Unreal sont enregistrés sans transformation dans `unreal.utrace`. Les pistes CPU/GPU natives dépendent des canaux et de la plateforme. Sur Quest avec le backend OpenXR compatible, les métriques disponibles du collecteur Meta sont déjà des événements de cette même trace et suivent le flux automatiquement. Les événements `CyInsights.Sync` embarquent aussi l'identité de session, les cycles CPU et la frame de jeu observée.

Le manifeste final vient du jeu et conserve sa version du moteur, son projet, sa plateforme, son API graphique, son matériel et les sources disponibles. L'heure d'arrivée sur le PC n'est pas utilisée comme temps d'exécution CPU/GPU. Les correspondances GPU et les limites du runtime Meta conservent leur provenance/disponibilité. Les compteurs natifs et Meta sont consultables dans le panneau Counters de l’application, via l’export de l’analyseur Epic installé. Leurs graphiques utilisent l’axe temporel continu et leur disponibilité par type ; voir [CounterAnalysis.md](CounterAnalysis.md). Le décodeur dédié aux événements bruts Quest reste à implémenter.

La réception est active pendant le jeu. L'ouverture et l'analyse dans l'application se font **après l'arrêt et la fermeture du flux**. L'analyse continue des providers pendant la réception reste à implémenter. Les fichiers détaillés ReShade (shaders, ressources, commandes) ne sont pas transférés par ce flux natif ; leur assemblage reste séparé.

## Protocole et coordination

Le service accepte uniquement `hello`, `status`, `start`, `gpu` et `stop` en JSON version 1, délimité par des nouvelles lignes. Requêtes limitées à 4096 octets, réponses à 65536 octets, une connexion active et expiration après 30 secondes sans activité. Le PC interroge le statut chaque seconde. Aucune commande console distante n'est exécutée. `hello/status` exposent les valeurs par défaut et capacités ; `start` accepte les options typées v1, `gpu` une demande ponctuelle appartenant à la session. Voir [CaptureProfiles](CaptureProfiles.md) pour le profil léger, les surcharges et la limitation de transfert GPU détaillé.

`start` fournit `receiver_port` (1..65535) et `seconds` (0..3600). Le jeu utilise exclusivement l'adresse IP du client de commande comme destination. Une connexion détient sa capture et son `session_id` ; un arrêt avec un autre identifiant est refusé. Une trace native déjà active empêche un nouveau démarrage. La fermeture/expiration de la connexion arrête seulement la capture réseau qu'elle possède.

Le module `CyInsightsTrace` prépare les canaux puis connecte le writer natif avec le port de réception. La connexion est lancée dans une tâche, sans attente de socket sur le thread du jeu. Les API sont adaptées à UE 4.27 et 5.0–5.8, sans modification/recompilation du moteur ; les versions effectivement testées sont précisées dans [la matrice de compatibilité](UnrealCompatibility.md). UE4 utilise un récepteur local appartenant au plugin pour gérer la fin de capture, avec une limite explicite sur les événements encore en vol. Les refus transitoires du writer pendant un démarrage/arrêt sont réessayés.

Le PC confirme l'en-tête natif et suit la session. Après l'arrêt confirmé, il attend l'EOF TCP du writer Unreal, vérifie le manifeste et les payloads, puis crée et valide le `.cycapture`. Une erreur réseau, un reset, un en-tête invalide ou une incohérence de session conserve les données partielles et ne publie pas de capture complète. Réception et packaging s'effectuent hors du thread de l'interface.

Le bilan final des sources est pris avant l'arrêt, pendant que l'identité native de la trace et les métriques Quest sont encore disponibles. Le plugin conserve ce manifeste en mémoire pour le client même si l'enregistrement local sur le jeu échoue. Les builds désactivant Counters signalent les ancrages embarqués indisponibles ; les ancrages locaux JSON restent séparés.

Options : `CyInsights.RemoteCapture 0` désactive le service ; `CyInsights.RemotePort` modifie son port. `-CyInsightsRemotePort=<port>` le fixe pour une instance ; adapter alors le port dans les options du PC. Les canaux restent configurables avec `-CyInsightsChannels=...` ; compteurs et ancrages embarqués sont ajoutés par le coordinateur.

## Validation

42 vérifications réseau isolées couvrent les en-têtes fragmentés/tronqués, les limites, le refus d'une trace active, les réponses invalides, l'identité de session, les interruptions, les arrêts manuels/automatiques, les dernières données après l'acquittement d'arrêt et le round-trip de l'archive. Avec les suites existantes et les 45 vérifications de compteurs, **180 vérifications de composants** passent ; voir [CounterAnalysis.md](CounterAnalysis.md).

Un processus de test Unreal 5.5.4 sous Win64/NullRHI a produit une capture réseau réelle de deux secondes, via le backend du client, avec **5 654 519 octets natifs**. Le `.cycapture` a été validé et Epic Insights a exporté **1 499 153 événements de timing**. L'import et l'index de l'application ont traité cet export : 11 pistes et 6 780 identités de timer. NullRHI ne valide pas le GPU matériel. `Tests/RemotePluginValidation.py` a passé **28 vérifications** : captures répétées, arrêt manuel/automatique, arrêt refusé pour une autre session, commandes inconnues/version incorrecte et arrêt à la déconnexion du propriétaire.

Les sources finales des modules réseau/Quest ont compilé pour Android ARM64 Development avec UE 5.5–5.8 : objets de modules, pas un jeu lié/empaqueté. Les DLL Runtime/Editor Win64 5.5 ont été compilées et liées. UBT a rencontré une recherche incorrecte de `mt.exe` ; les DLL ont été liées avec ses response files dans l'environnement MSVC/SDK correct (`build/link-remote-validation.cmd`, lancé depuis `Engine/Source`). Aucun binaire du moteur n'a été reconstruit. Logs : `build/remote-android-5.5.log` à `5.8.log`, `build/remote-game-validation.log`, `build/remote-real-validation/analysis/analysis.log`.

Le test réseau sur casque reste à effectuer avec un jeu empaqueté intégrant ce plugin. L'interface a été compilée, mais aucun nouvel exécutable de l'application n'a été lié/lancé : l'alerte Kaspersky précédente reste non résolue, voir [SecurityReview.md](SecurityReview.md).

Les canaux réseau/fichiers/chargement/tags mémoire sont maintenant demandés dans la même trace. `CyInsights.NetworkTraceLevel` règle l’activation NetTrace par le plugin ; les allocations détaillées demandent toujours un démarrage instrumenté. Voir [NativeActivities](NativeActivities.md) pour les conditions et l’analyse dans Timing.
