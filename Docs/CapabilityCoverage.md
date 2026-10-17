# Couverture de l’intégration IA

Révision locale du 17 octobre 2026 : **39 opérations** dans
[CyTool.json](../AI/app/CyTool.json), SDK CyToolsCore 0.10.1 / MCP officiel 1.30.0.
Les mêmes handlers alimentent MCP, CLI/JSONL/HTTP et AI Inspector > Tools.
Le catalogue et la méthode sont embarqués dans l’exécutable : les analyses et
le parcours guidé natifs fonctionnent sans CyAiCodex, sans Python et sans MCP.
Les lecteurs `.utrace` gardent leurs prérequis Unreal habituels.

| Capacité native | Outils IA | Portée et preuve |
|---|---|---|
| État, métadonnées, disponibilité | `get_state`, `get_metadata`, `get_capabilities` | Génération/document, état de chaque lecteur, métadonnées enregistrées ; MCP natif testé |
| Méthode pour les IA | `get_analysis_guide` | JSON versionné, onglet AI method, [guide](AiAnalysisMethod.md) ; découverte MCP testée |
| Ouverture et sélection | `open_trace`, `set_range`, `filter_function`, `focus_view` | Capture liée au document ; permissions natives séparées ; lecteurs locaux Unreal requis |
| Catalogues CPU/GPU | `list_threads`, `list_functions` | Identités natives et réponses paginées |
| Coûts et occurrences | `hotspots`, `histogram`, `show_histogram`, `query_scopes` | Agrégats de toute la population, détails bornés, plage sélectionnée/visible |
| Frames et diagnostics | `detect_issues`, `query_frames` | Avec/sans attentes reconnues ; causes probables, GPU absent explicite |
| Appelants/appelés | `query_relations` | Parents directs de l’index natif, coûts découpés aux bornes ; rejet si imbrication ambiguë ; fixture testée |
| Tâches et dépendances | `load_tasks`, `query_tasks`, `query_task_links` | Phases, IDs 64 bits et liens enregistrés ; candidats CPU uniquement sur demande |
| Compteurs et Meta | `list_counters`, `load_counters`, `query_counters`, `pin_counter` | Types/unités, agrégats, courbes/échantillons facultatifs ; compteur manquant nul ; SDK et lecteur natif testés |
| Statistiques par frame | `query_frame_stats`, `load_counters.frameOnly` | Observations/source game/stats frame, retards et inconnues conservés ; fixture native testée |
| Réseau | `load_activities`, `list_activity_lanes`, `query_activities.kind=network` | Événements enregistrés, volume, intervalles et positions de bits distincts ; MCP natif testé |
| Stockage/chargement/mémoire | `query_activities.kind=storage/loading/memory` | Durées, détails et courbes des providers natifs ; jauges non additionnées comme trafic ; fixtures testées |
| Comparaison A/B | `open_comparison`, `query_comparison` | Intervalles et horloges indépendants, normalisation, delta B−A, correspondance déduite par nom/domaine ; MCP natif testé |
| Sous-passes Quest | `query_vulkan_passes` | Arguments interceptés, pas coûts GPU individuels ni identité Unreal inventée ; nouvelle capture nécessaire |
| Commandes GPU Windows optionnelles | `query_gpu` | Commandes, bindings/état/barrières facultatifs, pipelines, shaders, ressources, passes/dépendances déduites ; fixtures testées ; aucune image/lecture de buffer renvoyée |
| Capture réseau | `capture_probe/start/status/stop`, `capture_gpu` | Profils/options temporaires, autorisation distincte ; capture matérielle Quest non revalidée par cette révision |
| Annulation des lecteurs | `cancel_loading` | Compteurs/activités/tâches/comparaison ; préserve la trace ; ne remplace pas `capture_stop` |
| Observations et rapports | `publish_finding`, jobs SDK | Génération obligatoire, hypothèses explicites, rapports privés ; sources en lecture seule |

## Exclusions du moteur et de la plateforme

| Capacité | État et raison |
|---|---|
| Analyse de la trace entrante pendant l’enregistrement | Non implémentée dans le moteur natif ; le MCP annonce `nativeLiveAnalysis=false`. Analyse après arrêt/assemblage. |
| Logs natifs | Pas de lecteur/provider natif dans cette révision ; aucun faux outil Logs. Les messages de l’application ne sont pas les logs du jeu. |
| Pont objets Unreal/CyEE | Pas d’identité unique générale disponible ; instancing/Nanite restent ambigus. |
| Coût matériel de chaque sous-passe Quest | Les arguments de draw ne sont pas des timestamps GPU ; valeur indisponible. |
| Payload GPU Windows détaillé par le récepteur réseau | Demande de snapshot possible, transfert du payload non implémenté. |
| Console/autres OS | Aucun collecteur console ni transport natif validé ici ; l’adaptateur courant annonce Windows x64. |
| Gestion complète des fenêtres/documents par IA | La connexion reste liée à un document explicite ; les autres documents/fenêtres restent gérés par l’UI et gardent leurs propres autorisations. |
| Export de textures/buffers vers le modèle | Non exposé : les outils d’analyse retournent des métadonnées. Les readbacks de capture restent un opt-in séparé. |

## Contrat et vérification

Résumés avant détails, 20 lignes par défaut/128 maximum, bins facultatifs.
Une page d’événements d’activité contient au maximum 128 événements par piste.
Les réponses int64 sont des chaînes exactes ; les moyennes/bins en double peuvent
être approximatifs. Les timestamps sont des secondes dans leur session.
Les compteurs ont une révision de catalogue pour détecter les indices périmés.

Les requêtes lisent des snapshots/index partagés dans des workers ; elles ne
copient pas les captures dans les jobs SDK. Le chargement prépare le nouveau
modèle de compteurs hors du thread UI. Annuler un job n’annule pas une action UI
acceptée et n’arrête pas un enregistrement.

Validation : 23 suites CTest, dont 43 vérifications de la nouvelle analyse native ;
11 tests SDK Python ; 20 contrôles via client MCP officiel relié à une application
native sur fixtures réseau et statistiques UE 5.8. Les traces sources restent
inchangées. Ces contrôles ne sont pas une nouvelle mesure matérielle Quest.

Standalone : 52 contrôles de ressources embarquées et sept appels natifs réels dans un dossier isolé sans compagnon Python ni pont MCP, avec vérification visuelle du parcours en français.
