# Compatibilité du plugin Unreal

Les sources du plugin `CyInsights`, incluant ses modules Windows internes, ciblent
UE **4.27 et 5.0–5.8**, sans modification ni recompilation du moteur. Les modules
restent limités à l'éditeur et aux clients Debug, DebugGame et Development.
Shipping, Test et Server sont exclus. Une installation Unreal complète et
licenciée, avec les outils C++/SDK requis, est nécessaire pour compiler le plugin.

## Vérifications effectuées le 10 octobre 2026

| Installation | Vérification | Résultat |
| --- | --- | --- |
| 4.27.2 Windows | Compilation C++ Runtime/Trace, chargement et capture locale dans un hôte minimal avec NullRHI | Réussi ; validation complète Editor/GPU bloquée par les fichiers SDK manquants de l'installation |
| 5.0–5.2 | Adaptations et détection des fonctionnalités par les en-têtes | Non compilées : installations absentes |
| 5.3.2 Windows | Six modules, Editor Development | Compilation réussie |
| 5.4.4 Windows | Six modules, Editor Development | Compilation réussie |
| 5.5.4 Windows | Six modules, Editor Development et DebugGame ; trois modules sans collecteur GPU | Compilation réussie |
| 5.6.1 Windows | Six modules, Editor Development | Compilation réussie |
| 5.7.4 Windows | Six modules, Editor Development | Compilation réussie |
| 5.8.3 Windows | Six modules, Editor Development | Compilation réussie |
| 5.5.4 Android | Runtime, Trace et Quest, Game Development | Compilation réussie ; packaging et capture sur casque à valider |

Les builds du 10 octobre utilisaient encore le plugin Windows séparé. Depuis le
11 octobre, il est remplacé par les modules internes `CyInsightsGpuLoader` et
`CyInsightsGpuWindows`, dans le seul plugin `CyInsights`. Leurs options de chargement
sont désactivées par défaut ; aucun autre logiciel de capture n'est lancé.
Les résultats de la matrice historique ci-dessus ne valident pas automatiquement
les nouveaux modules. Ces builds
vérifient la compilation des plugins, sans constituer un test des collecteurs sur
tous les matériels ni un test de jeu packagé dans chaque configuration.

L'installation 4.27 locale ne contient pas les en-têtes de réflexion générés et
les bibliothèques d'importation nécessaires au build Editor complet. Le test
Runtime a utilisé ses DLL existantes comme entrées d'importation dans le dossier
`build` du projet uniquement. Une trace native de 616 710 octets a été produite.
Ces entrées locales, les fichiers de capture et les binaires Epic ne sont pas
publiés. Ce test ne valide ni la barre d'outils 4.27 ni les marqueurs GPU RHI.
L'analyseur Unreal Insights installé (5.5) a décodé cette trace et exporté
165 209 événements CPU, sans erreur d'analyse.
Le même Runtime 4.27 a passé les 28 vérifications de `RemotePluginValidation.py`
sur trois captures réseau : arrêt manuel, captures répétées, déconnexion du
propriétaire, arrêt automatique, refus d'une mauvaise session et fermeture du
flux natif. Les 14 suites de composants passent 622 vérifications ; les quatre
tests de descripteurs vérifient les configurations et plateformes autorisées.

## Adaptations

- Le module MIT `CyInsightsTrace` centralise les API de capture, le ticker et
  l'identité native. Les capacités Start/Stop, GUID et options de tail sont
  détectées dans les en-têtes de l'installation, pour tenir compte des forks.
- Sous UE4, les callbacks de rendu utilisent `FRHICommandListImmediate` ; sous
  UE5 ils utilisent `FRDGBuilder`. Les accès aux render targets et les menus
  Editor suivent aussi leurs API respectives.
- Les descripteurs utilisent uniquement les clés historiques `Whitelist*` et
  `Blacklist*`, lues par UE4 et encore acceptées par UE5.8. Les deux variantes
  historiques et nouvelles ne doivent pas être inscrites simultanément.
- Le collecteur Windows est facultatif. Son absence laisse la capture native
  disponible. Aucun collecteur ReShade n'est activé sur Android.

## Capture UE 4.27

UE4.27 n'expose pas de déconnexion publique du writer natif. Le plugin reçoit
donc son flux sur une socket locale `127.0.0.1`, puis le sauvegarde ou le relaie
vers le PC. L'en-tête `ECRT` et les paquets natifs sont conservés sans réécriture.
Le récepteur PC accepte les en-têtes UE4 et UE5.

À l'arrêt, le récepteur du plugin draine pendant 100 ms puis ferme ses propres
sockets après les derniers paquets complets reçus. Un paquet final incomplet
n'est pas publié. Des événements encore en vol peuvent manquer : il ne s'agit
pas d'un flush garanti du moteur. Le plugin ne ferme ni ne réinitialise le
système global de trace Unreal. Une trace déjà connectée doit être arrêtée par
son propriétaire avant de lancer une capture CyInsights ; un nouvel essai peut
devoir attendre que le writer constate la fermeture du récepteur précédent.

Les GUID absents des anciennes API sont remplacés par une identité d'époque
observée par le plugin. Elle est explicitement marquée `derived`, et ne prétend
pas être un GUID natif. Le manifeste expose `trace_guid_source`,
`capture_transport` et `stop_semantics`. Les ancres de frame utilisent le canal
propre `CyInsightsSync` ; elles restent des observations CPU et ne deviennent
pas des identités exactes de frames GPU.

## Quest et moteurs anciens

Les métriques Meta nécessitent `XR_META_performance_metrics`, les hooks OpenXR
utilisés et la disponibilité de l'extension sur le runtime du casque. Le build
détecte les API requises. Le SDK OpenXR fourni avec l'installation 4.27 testée
ne les expose pas : le module déclare les métriques **indisponibles avec leur
raison**, tout en laissant la capture Unreal fonctionner. Il ne fabrique pas
de temps GPU. Les forks Oculus/Meta peuvent exposer des capacités différentes.

Ce changement ne fournit pas de nouveau collecteur Vulkan, ni de collecteur
console. La compatibilité du plugin ne garantit pas la présence de métriques
Meta sur toutes les versions ni l'identité unique mesh/draw/Nanite.

## Reproduire les compilations

Depuis la racine du dépôt, avec les SDK appropriés :

```powershell
python Tools/test_unreal_compatibility.py --engine C:/Example/UE_5.5/Engine
python Tools/test_unreal_compatibility.py --engine C:/Example/UE_5.5/Engine --configuration DebugGame
python Tools/test_unreal_compatibility.py --engine C:/Example/UE_5.5/Engine --without-gpu
python Tools/test_unreal_compatibility.py --engine C:/Example/UE_5.5/Engine --platform Android --target Game
python Tools/test_unreal_compatibility.py --engine C:/Example/UE_4.27/Engine --compiler 14.29.30133 --no-pch
python -m unittest discover -s Tests -p test_unreal_descriptors.py
```

Le script crée un projet hôte isolé dans `build/UnrealCompatibility`, demande
uniquement les modules du plugin et écrit `validation.json` et les logs. Les
compilations UBT de différentes installations doivent rester séquentielles,
car leur cache utilisateur de règles Marketplace est partagé. Le dernier
exemple 4.27 ne contourne pas les fichiers SDK manquants : son échec complet
reste documenté dans la matrice.

Le lecteur de traces de l'application et ses adaptateurs de tâches natifs ont
leur propre périmètre de validation. Voir [NativeTasks](../Tools/NativeTasks/README.md)
et [analyse des traces](TraceAnalysis.md) ; élargir la compatibilité du plugin
ne signifie pas que toutes les vues d'Unreal Insights sont implémentées.
