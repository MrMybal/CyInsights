# IA, MCP et SDK dans CyInsights

Révision locale du 17 octobre 2026 : **39 opérations** communes au SDK,
au MCP et à l’onglet Tools natif. Les nouveaux outils couvrent compteurs et
statistiques par frame, activités réseau/stockage/chargement/mémoire,
comparaison A/B, scopes individuels, relations appelants/appelés, dépendances
natives de tâches et détails du collecteur GPU optionnel.

Lire la [méthode d’analyse](AiAnalysisMethod.md), la [couverture actuelle](CapabilityCoverage.md)
et le [contrat exécutable](../AI/app/CyTool.json). `get_analysis_guide` fournit la méthode
aux IA ; **AI Inspector > AI method** permet de la lire/copier dans l’application.
`get_capabilities` annonce les providers chargés et les limites natives.
Les nouvelles requêtes suivent par défaut la plage sélectionnée/visible ;
les int64 et identités gardent leur précision, les absences restent `null`.

Le détail ci-dessous documente également l’intégration initiale ; ses anciens
comptages de tests et d’opérations ne décrivent pas la révision actuelle.

La release `bin/Release-20261009.6` intègre **CyToolsCore 0.10.1**, son ordonnanceur de jobs et le SDK MCP officiel **1.30.0**. La fenêtre **AI Inspector**, les calculs et la timeline restent en C++/Dear ImGui. Un compagnon Python expose les outils ; il ne dépend pas du checkout de CyToolsCore à l'exécution.

## Utilisation

1. Lancer `CyInsightsApp.exe` et ouvrir **AI Inspector** dans la barre supérieure ou le menu View.
2. **Findings** analyse automatiquement la plage sélectionnée, ou la fenêtre visible sans sélection. Après un glissement, l'analyse attend le relâchement. Régler le budget FPS et cliquer **Show in Timing** pour examiner une frame suspecte.
3. Dans **Connection**, activer **Enable local bridge**, puis **Copy MCP configuration**. Coller le JSON dans les paramètres MCP du client IA choisi. Le PID identifie cette instance : recopier la configuration après un redémarrage.
4. Activer **Allow AI application control** pour ouvrir une trace, changer la sélection ou publier une observation. Activer séparément **Allow AI remote capture** pour démarrer/arrêter une capture réseau.

Les deux autorisations sont désactivées au démarrage normal. `--ai-enable`, `--ai-control` et `--ai-capture` sont des options explicites de lancement ; `--ai-view` affiche la fenêtre. Aucune configuration de client IA existante n'est modifiée automatiquement.

Avec plusieurs captures, copier la connexion depuis le document concerné :
`CYINSIGHTS_DOCUMENT_ID` identifie son endpoint indépendamment de l'onglet
visible. Les permissions sont propres à ce document. Voir
[Plusieurs captures](MultipleCaptures.md). `--ai-control-all` est une option
explicite de lancement pour autoriser le contrôle de tous les documents passés
en arguments ; les documents ouverts ultérieurement restent désactivés.

**Tools** présente les opérations et leurs contrats JSON, permet de les lancer localement et de copier le résultat. Save/Load parameters conserve les paramètres versionnés ; charger ne lance pas l'opération. **Options** propose anglais/français, l'analyse automatique, le budget FPS et les notices. Le modèle conversationnel s'exécute dans le client MCP choisi ; aucun fournisseur ou modèle n'est embarqué.

## Opérations de l’intégration initiale

Les entrées exactes, valeurs par défaut et bornes proviennent de [CyTool.json](../AI/app/CyTool.json). Les paramètres inconnus sont refusés.

| Opération | Usage | Autorisation native externe |
|---|---|---|
| `get_state` | Trace courante, disponibilité, génération, sélection et état de capture | Lecture |
| `open_trace` | Ouvrir un chemin absolu `.utrace` ou `.cycapture` existant | Application |
| `set_range` | Sélectionner `[start, end)` ; cadrer avec `focus` | Application |
| `filter_function` | Filtrer le nom exact sur tous les threads ; chaîne vide pour effacer | Application |
| `list_threads` | Catalogue paginé des pistes, recherche | Lecture |
| `list_functions` | Catalogue paginé des identités de timers, recherche | Lecture |
| `hotspots` | Coûts inclusifs/exclusifs agrégés dans un intervalle | Lecture |
| `histogram` | Distribution par fonction ou par frame/thread, avec/sans attentes reconnues | Lecture |
| `detect_issues` | Frames dépassant le budget, couverture et causes probables | Lecture |
| `load_tasks` | Demander le chargement du provider natif de tâches | Application |
| `query_tasks` | Phases des tâches enregistrées ou scopes CPU candidats explicitement demandés | Lecture |
| `show_histogram` | Ouvrir la distribution d'une fonction dans l'interface | Application |
| `publish_finding` | Ajouter une hypothèse IA liée à une génération et à un intervalle | Annotation |
| `capture_probe` | Vérifier l'endpoint du plugin à une IPv4/port | Capture réseau |
| `capture_start` | Démarrer réception/capture ; durée 0 = arrêt explicite, maximum 3600 s | Capture réseau |
| `capture_status` | État, octets reçus et archive disponible | Lecture |
| `capture_stop` | Arrêter uniquement l'enregistrement détenu par cette application | Capture réseau |
| `capture_gpu` | Demander un snapshot GPU optionnel au recorder Windows connecté ; aucun transfert du payload GPU par le récepteur natif | Capture réseau |

Les permissions SDK `appControl`, `annotations` et `remoteCapture` s'ajoutent aux contrôles natifs correspondants. Les annotations ont leur permission SDK propre ; le contrôle Application natif autorise leur publication.

Un appel MCP utilise `op_<operation>` avec `{"parameters":{...}}`. Il retourne un job. Interroger `cy_get_job` jusqu'à `Completed`, `Failed` ou `Cancelled` ; lire `outputs.data`. `outputs.reportFile` est le rapport JSON privé de ce job. Exporter les rapports nécessaires avant `cy_finish_task`.

Une ouverture ou une commande de capture retourne **accepted** ; cela ne signifie pas que l'indexation ou la réception est terminée. Interroger `get_state` ou `capture_status`. Annuler un job analytique interrompt le calcul natif. Annuler un job après une action UI acceptée ne l'annule pas rétroactivement et n'arrête pas une capture : appeler `capture_stop`.

Séquence conseillée : `get_state` → `detect_issues` → `set_range` → `hotspots` → `histogram` → `publish_finding`. Pour le réseau : `capture_probe` → `capture_start` → `capture_status` → `capture_stop`, attendre l'archive, puis `open_trace`.

## Résultats compacts et interprétation

- Les réponses renvoient par défaut 20 lignes, au maximum 128. Les décomptes et agrégats portent sur toute la population demandée. Les bins d'histogramme sont optionnels ; les valeurs lentes sont bornées par `limit`.
- Les temps sont en secondes. Identités natives 64 bits et décomptes sont des chaînes JSON pour conserver leur précision. Une mesure absente est `null` ou explicitement indisponible, jamais zéro inventé.
- Les scopes d'histogramme sont sélectionnés par début dans `[start, end)` et gardent leur durée complète. Les coûts de hotspots sont découpés aux limites de la plage. Des scopes inclusifs imbriqués se recouvrent : leur somme n'est pas un pourcentage d'utilisation du thread.
- Les quantiles des scopes sont estimés et leurs bornes sont fournies ; ceux des valeurs par frame sont exacts pour la population calculée. Les durées non terminées sont indisponibles.
- Les coûts actifs/sans attentes sont déduits des scopes et des attentes reconnues. Une cause probable de dépassement n'est pas une preuve causale. Une identité de frame native se distingue d'un candidat dérivé de Tick ; les coûts Render/GPU sont associés par recouvrement temporel.
- Sans TaskTrace, `query_tasks` ne produit des candidats CPU que si `candidates=true`. Leur ID natif et délai de queue restent `null`. Les pistes candidates sont remappées vers les identités natives de thread.
- Les pages de tâches contiennent au maximum 128 entrées. Avec `limit=20`, seule une partie de la page est retournée ; demander `limit=128` pour parcourir une page complète.
- Chaque résultat analytique porte une génération de trace. `publish_finding` refuse une génération périmée. Les observations externes sont marquées `external_ai_hypothesis` et affichées comme texte.

Sur la trace de validation, une réponse d'histogramme sans bins et avec trois valeurs lentes occupe **1085 octets JSON**, une réponse avec quatre frames suspectes **2925 octets**. Ce sont des tailles mesurées, pas un nombre de tokens garanti. Le fichier contient 14 192 325 événements ; aucun export intégral de ces événements n'est transmis au modèle par les outils.

## Installation et isolation

La release de test contient un environnement privé déjà installé, fondé sur le Python **3.11 x64** de cette machine. Pour recréer l'environnement : `AI/SetupRuntime.ps1`, avec Python 3.11 x64 installé. Les 38 wheels et leurs versions/empreintes sont conservés dans `AI/wheels` et [dependency-lock.json](../AI/dependency-lock.json) ; l'installation peut se faire hors ligne. Ce n'est pas une distribution Python portable. Le profiler peut fonctionner sans ce compagnon.

Le transport natif est un named pipe limité au compte Windows de l'application et refuse les clients distants. Quatre workers transportent les demandes ; le thread UI traite les actions et des workers calculent les analyses sur des snapshots immuables. L'ordonnanceur, les sessions, les tokens, les jobs, les rapports et les budgets de ressources appartiennent à CyToolsCore.

Les rapports appartiennent aux espaces privés des jobs. En revanche, la trace ouverte, la sélection et le récepteur réseau sont des états partagés de cette instance de l'application. Une source de trace est une **référence persistante en lecture seule** : elle n'est volontairement pas importée, copiée ou purgée comme fichier temporaire SDK, afin de préserver les captures de plusieurs Go et l'analyse native ultérieure. Ce choix ne transforme pas une trace en fichier privé par utilisateur SDK. L'adaptateur n'envoie pas de trace à un fournisseur IA ; le client externe choisit ce qu'il transmet.

Le compagnon stdio crée un client SDK local avec les permissions nécessaires ; les contrôles de la fenêtre restent déterminants. Un service HTTP partagé nécessite des tokens explicites selon CyToolsCore. La configuration copiée par l'interface utilise le MCP stdio local.

## Portée actuelle et validation

L'aide réactive fonctionne sur la trace chargée et l'intervalle visible/sélectionné. **Le décodage de la trace entrante pendant l'enregistrement n'est pas implémenté.** Les outils savent lancer, recevoir, arrêter et assembler la capture ; l'analyse native suit l'arrêt. Les données GPU manquantes ne sont pas fabriquées. Les compteurs, comparaison A/B, détails de commandes GPU, providers Logs/Memory/Network et liens d'objets Unreal ne disposent pas tous d'une opération IA : voir [CapabilityCoverage.md](CapabilityCoverage.md).

La révision passe 483 vérifications natives sur 13 suites et cinq tests Python du compagnon. Les tests MCP officiels couvrent lecture seule, ouverture réelle, histogramme/recherche/diagnostic, sélection/filtre, rejet de génération, annulation, tâches candidates, isolation de rapports et commandes réseau. La source `.utrace` reste inchangée. La capture MCP réseau de cette révision utilise un endpoint de test local et un payload synthétique ; aucune nouvelle capture matérielle Quest n'a été validée.

CyToolsCore provient du dépôt [MrMybal/CyToolsCore](https://github.com/MrMybal/CyToolsCore), commit `a8d200f2d0baaade1633e0683663b4506c595f74`, wheel officielle 0.10.1 MIT. Les notices intégrales Core/MCP/dépendances accompagnent les licences existantes dans `LICENCES`. Le moteur et ses DLLs ne sont pas redistribués. Les preuves et empreintes de la release sont dans `release-manifest.json` et `build/release-20261009.6-audit`.


`query_vulkan_passes` reads captured subpass draw arguments in the selected
range (`start`, `end`, optional capture-local `passId`, bounded `limit`). It
uses the immutable read-only detail cache in a worker, never copies a trace
into an SDK job, and returns null individual GPU costs/Unreal names. Requires
new `CyInsightsVulkan.Contents` events. See [subpass contents](QuestSubpassContents.md).
