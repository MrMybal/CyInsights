# Réseau, stockage et mémoire dans la timeline

L'onglet natif **Activities** charge les fournisseurs Unreal réseau, fichiers,
chargement d'assets et mémoire. Les pistes épinglées apparaissent dans **Timing**,
sur le même axe en secondes que le CPU/GPU. Zoom et déplacement restent communs.
La sélection avec **Shift + glisser dans la règle** filtre les détails ; sans
sélection, ils suivent l'intervalle visible. Recherche par nom et pages de 128
observations permettent d'inspecter les fichiers et les contenus de paquets.
Les pages limitent l'affichage, jamais le nombre d'événements acceptés.

| Source native | Ce qui est lu | Interprétation |
| --- | --- | --- |
| NetTrace | Paquets envoyés/reçus, connexion/instance, séquence, taille, perte signalée, noms de contenus disponibles | Horodatage d'observation mesuré ; durée de transit indisponible |
| PlatformFile | Open, ReOpen, Close, Read, Write ; thread, offset, taille effective, échec | Intervalle de l'opération Unreal ; activité physique du disque système indisponible |
| LoadTime | Scopes de chargement d'assets, package connu et taille sérialisée disponible | Travail CPU de chargement ; ne pas additionner ses octets aux transferts disque |
| Memory | Échantillons LLM par tracker/tag, minimum/maximum de mémoire allouée et nombres d'allocations/libérations par intervalle natif | LLM mesuré ; résumés d'allocations déduits des événements natifs ; intervalles distincts des durées d'exécution |

Les graphes d'activité sont une **densité déduite** des événements mesurés par
colonne de pixels ; les valeurs mémoire retiennent les extrema des échantillons
présents. Tous les événements correspondants contribuent, même si les détails
occupent plusieurs pages. Les tailles des contenus réseau sont en **bits** et
ne sont pas ajoutées aux octets des paquets. Les contenus n'ont pas de timestamp
indépendant : ils sont rattachés à l'observation du paquet ; leurs offsets de bits
ne deviennent jamais des millisecondes. Les identités affichées sont des
séquences/offsets/identités de package suivant la piste, pas des identités d'actor
garanties.

Les opérations coupées par la sélection gardent leurs bornes originales dans
les détails ; leur durée inclusive agrégée est tronquée à la sélection. Les
octets d'opérations/paquets sont attribués à leur début dans la plage. Des
opérations simultanées peuvent se recouvrir : leur somme n'est pas le temps
occupé du disque. Les valeurs mémoire ne sont jamais additionnées comme un débit
et les opérations incomplètes n'ont pas de durée inventée. Une piste absente ou
une plage sans observation est indiquée comme indisponible.

## Enregistrer

Le plugin demande par défaut `net,file,loadtime,memtag` en plus des canaux de
timing existants. Le manifeste conserve les canaux demandés et la verbosité
réseau native obtenue, sans prétendre que chaque canal a produit des événements.
Cela s'applique à la capture locale et à la capture demandée par IPv4 : les
sources restent dans le même `.utrace`, puis dans le même `.cycapture`.

`CyInsights.NetworkTraceLevel` vaut 1 par défaut. Quand `net` est sélectionné et
que NetTrace est compilé, le plugin active sa verbosité au démarrage de capture,
après connexion de la sortie réseau. Il conserve une verbosité déjà activée par
l'utilisateur. À l'arrêt, il rétablit seulement la valeur qu'il avait activée
lui-même. La valeur 0 désactive cette activation automatique. Cela n'ajoute pas
les sockets externes, HTTP, voix ou tout le trafic réseau du système à NetTrace.
Le Network Profiler historique `.nprof` n'est pas importé : utiliser la trace
native Networking Insights pour conserver la même horloge.

La mémoire détaillée nécessite une instrumentation activée **au lancement**, par
exemple `-trace=memory` sur les versions qui la prennent en charge. Ajouter
`memory` à `-CyInsightsChannels=...` pour garder ces canaux pendant la capture.
Les tags LLM demandent aussi LLM actif, par exemple `-LLM` sur les plateformes
compatibles. Une capture tardive ne reconstruit pas les allocations antérieures.
Les callstacks et l'explorateur d'allocations individuelles ne sont pas encore
exposés par ce lecteur. Une sortie native déjà connectée est toujours refusée
par le coordinateur pour préserver la capture existante.

La disponibilité dépend du moteur, de la configuration et de la plateforme.
Quest reste analysé sur le PC ; aucun nouvel outil externe n'est nécessaire sur
le casque. Cette version ne valide pas une capture matérielle Quest ni console.

## Lecture et performance

Construire l'adaptateur facultatif [NativeActivities](../Tools/NativeActivities/README.md)
pour l'installation Unreal Insights sélectionnée. Les DLL Epic restent dans
l'installation sous licence. L'analyseur charge seulement les modules Unreal
réseau, chargement et mémoire, puis écrit un journal de streaming. L'application
construit un index disque par blocs de 4096 observations et par piste. Les
requêtes et le décodage sont asynchrones et annulables ; les blocs hors plage
sont ignorés. Le lecteur Unreal peut lui-même consommer de la mémoire pendant
son analyse ; ce travail ne bloque pas le rendu ImGui.

Le cache natif est créé dans le dossier d'analyse, jamais à côté de la trace
source. Le journal doit avoir sa ligne de fin et le bon nombre d'événements
avant publication de l'index. Les données sources sont ouvertes en lecture seule.
Les valeurs mémoire 64 bits, y compris les valeurs LLM signées, sont conservées.
Le dernier point LLM est interrogé isolément pour éviter une ancienne valeur
retournée par certains fournisseurs dans leur callback final de plage.

Validation locale : 23 vérifications du stockage/requêtes/annulation/échec du lecteur, 15 suites
de composants, interface compilée, lecteurs 5.5–5.8 compilés et exécutés sur une
fixture native synthétique réseau/fichiers/LLM. Une trace réelle existante a
également fourni une opération de fichier. Les modules plugin natifs Editor
Development 5.5 et 5.8 ont compilé sans reconstruire de module moteur. Les
autres versions restent soumises à [la matrice](UnrealCompatibility.md).

Le lecteur enregistre les modules installés avant de fixer leurs chemins DLL.
Cette initialisation corrige l'accès mémoire invalide de la première version.
Un échec du processus est signalé dans Activities avec son code Windows, sans
publier d'index partiel. Le panneau s'ouvre à côté de Timing afin de garder les
pistes visibles ; il démarre également l'analyse si son ancien onglet était masqué.

Références Epic : [Networking Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-insights-in-unreal-engine),
[canaux de trace](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-reference-in-unreal-engine-5).
