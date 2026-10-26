# CyInsights 0.2.1

## Capture GPU Quest intégrée

La capture réseau depuis CyInsights collecte maintenant les stages matériels
Meta détaillés via `track_event/gpu_renderstage` quand cette source est disponible.
Les timings Unreal, stages GPU, compteurs et métadonnées sont enregistrés dans
une seule archive `.cycapture`, puis ouverts automatiquement dans l'application.
Le jeu en cours reste actif ; son redémarrage est une option explicite.

Les pistes GPU montrent Binning, Render, LoadColor, LoadDepthStencil,
StoreColor et StoreDepthStencil, ainsi que les autres événements disponibles.
La timeline utilise les horloges Perfetto et l'ancre de session Unreal. Les
sources grossières sont signalées comme telles ; un redémarrage du casque
invalide les données de l'ancien démarrage.

Le parcours complet a été testé sur Quest 3 avec le petit projet Development :
deux captures combinées reçues et ouvertes dans CyInsights, dont une capture
finale de cinq secondes contenant 630 034 événements indexés et 360 frames
natives. Les sources et limites sont détaillées dans [les résultats Quest](MetaQuestGpu.md).

## Application et diagnostic

- Attente visible et annulable du service de capture au démarrage du jeu.
- Ouverture automatique des captures terminées, désactivable dans Session.
- Comptage des types de stages réellement reçus et distinction entre détails
  mesurés, événements grossiers et données indisponibles.
- Ouverture facultative des fichiers Meta `.pftrace` pour le diagnostic GPU
  seul ; ce mode ne crée pas de pistes CPU ni de frames Unreal.
- Petit projet Quest utilisable sans manettes ; sa configuration évite de
  recharger les caches Vulkan qui faisaient planter le test sur le pilote testé.

## Installation et limites

Extraire tout le ZIP Windows puis lancer `CyInsightsApp.exe`. Pour lire les
traces Unreal, ajouter la racine d'une installation Unreal licenciée dans
Session ; les lecteurs sont préparés avec les outils C++/CMake locaux.
Unreal n'est pas recompilé. Les sources du plugin sont fournies séparément.

La collecte Meta requiert un Quest autorisé dans ADB et le profiling GPU détaillé
activé avant le lancement du jeu. Les étapes et réglages sont décrits dans
[la documentation Quest](MetaQuestGpu.md). Aucune copie de texture ou de buffer
n'est activée par défaut.

Les phases matérielles ne fournissent pas leur correspondance exacte avec les
passes, shaders, objets ou frames Unreal. Les identités de processus inconnues
et les associations approximatives restent signalées. Cette validation Quest 3
ne constitue pas une validation de chaque casque, moteur ou pilote.

Les binaires Windows restent sans signature Authenticode de confiance. Les
licences, sources des lecteurs et empreintes SHA-256 accompagnent la release.
