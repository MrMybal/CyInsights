# Couverture de l'intégration IA

État de la release `Release-20261009.6`. Le contrat exécutable est [AI/app/CyTool.json](../AI/app/CyTool.json). Les mêmes outils natifs alimentent l'onglet Tools et le compagnon CyToolsCore/MCP.

| Fonction | Interface native | Outils IA | Limite |
|---|---|---|---|
| Ouverture `.utrace`/`.cycapture` | Open capture | `open_trace`, `get_state` | Analyseur Epic installé séparément ; progression asynchrone |
| Timeline continue CPU/GPU | Timing, Frames | `set_range`, `filter_function`, `get_state` | Les outils donnent des agrégats et intervalles, pas une image ou tous les événements |
| Identités de threads/fonctions | Pistes, Timers | `list_threads`, `list_functions` | Catalogues paginés, IDs natifs |
| Coûts par fonction | Timers | `hotspots` | Inclusion imbriquée ; bornes de plage appliquées |
| Distributions | Distributions, histogramme contextuel | `histogram`, `show_histogram` | Bins optionnels, quantiles scopes estimés |
| Dépassements par frame | Couleurs, AI Inspector/Findings | `detect_issues` | Actif/attentes déduits ; cause probable |
| Tâches | Tasks | `load_tasks`, `query_tasks` | TaskTrace nécessaire pour IDs/phases natifs ; candidats explicites sinon |
| Observations externes | AI Inspector/Findings | `publish_finding` | Hypothèse, génération obligatoire, lien vers la plage/fonction |
| Capture réseau IPv4 | Capture remote | `capture_probe/start/status/stop` | Plugin actif en Editor/Debug/Development, autorisation distincte |
| Aide sur plage chargée | Findings automatique | Agrégats et publication de l'IA externe | Pas de modèle conversationnel embarqué |
| Analyse de trace entrante | Pas encore | `nativeLiveAnalysis=false` | Analyse après arrêt/assemblage |
| Compteurs/métriques Meta | Counters lorsqu'enregistrés et chargés | Pas encore d'opération dédiée | La lecture des timings GPU n'implique pas des compteurs matériels |
| Comparaison A/B | Compare | Pas encore d'opération dédiée | Aucun score de régression IA automatique |
| Détails GPU supplémentaires | GPU frame selon collecteur | Pas encore d'opération dédiée | ReShade Windows, collecte Quest différente ; availability explicite |
| Logs, mémoire, réseau, assets | Couverture Insights encore partielle | Pas encore | Ne pas annoncer une parité complète avec Unreal Insights |
| Objets Unreal/CyEE | Correspondances selon données disponibles | Pas encore | Instancing/Nanite et identités approximatives restent distincts |
| Jobs/rapports/permissions | État résultat dans Tools | SDK CyToolsCore, MCP officiel | Rapports privés ; document UI partagé par instance |
| Paramètres/schémas | Tools : contrat, Save/Load | Manifest SDK | Recharger des paramètres ne les exécute pas |

Une trace source est une référence persistante en lecture seule ; les rapports générés sont des fichiers privés de jobs. La validation MCP réseau de cette release utilise un endpoint synthétique local. Les lecteurs UE 5.5–5.8 sont conservés ; aucune nouvelle validation matérielle Quest/console n'est annoncée.
