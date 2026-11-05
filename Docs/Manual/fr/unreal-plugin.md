# Plugin Unreal et connexion {#title}

Copier les sources du plugin CyInsights dans Plugins du projet, l'activer et recompiler pour le moteur/la plateforme. Le plugin est CyInsights ; son collecteur Windows interne optionnel ne nécessite pas l'application CyGPUInspector séparée.

## Connecter {#connect}
1. Ouvrir Project Settings > Plugins > CyInsights, ou Capture settings dans le plugin.
2. Lancer un build autorisé Editor, Debug, DebugGame ou Development. Ne pas supposer les services présents en Shipping.
3. Dans Session PC, saisir l'IPv4 locale de la cible puis Connect to device. Le port habituel est 41855.
4. Vérifier défauts/capacités annoncés, puis Start remote trace. Stop remote trace finalise la capture ; l'analyse commence après l'arrêt.

La plage visée 4.27–5.8 ne prouve pas l'exécution de chaque combinaison plateforme/version. Des compilations Windows sont documentées pour 5.5–5.8 ; la validation Editor 4.27 complète a des fichiers SDK manquants. Les essais Android/Quest portent sur des builds précis. Recompiler pour la version mineure réellement utilisée.

En cas d'échec, vérifier jeu lancé, service autorisé dans ce build, IP et réseau/pare-feu. GPU Profiling vert dans Meta ne prouve pas que le service Unreal répond. Voir [Dépannage](doc:troubleshooting).
