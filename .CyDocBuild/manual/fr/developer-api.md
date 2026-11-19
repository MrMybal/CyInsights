---
id: developer-api
title: SDK et API publique
description: Connecter un client externe et utiliser les contrats des opérations disponibles.
section: Référence / API développeur
order: 50
sourceRevision: 5df6c949853b
---

Les 39 contrats metier ci-dessous appartiennent au registre CyMCPSDK natif. Les memes handlers servent l’assistant integre, le terminal et MCP externe. La disponibilite des sources et les autorisations independantes de controle/capture du document restent applicables. Lire [AI Inspector](doc:ai).

## Point d’entree CLI natif {#python-entry}

```powershell
CyInsightsCli list
CyInsightsCli describe histogram
CyInsightsCli invoke get_state --pid 1234
CyInsightsCli invoke histogram --trace timing.csv --json '{"function":"Work"}'
```

Utilisez le PID et le document affiches dans **AI Inspector > Connection** pour l’instance ouverte. Aucune installation Python n’est necessaire. `mcp --pid PID` lance le serveur MCP stdio natif ; ajoutez `--full` pour la decouverte complete. Les resultats types sont renvoyes directement.

## Taches durables et ressources {#durable-jobs}

Neuf utilitaires completent les contrats metier : `job_start`, `job_status`, `job_result`, `job_list`, `job_cancel`, `job_delete`, `job_import`, `read_resource`, `grep_resources`. Consultez describe pour le schema JSON exact. Les resultats durables survivent aux redemarrages ; aucune action interrompue n’est rejouee. Les anciens rapports JSON peuvent etre importes explicitement avec le controle active. Les anciens environnements Python et bases restent intacts.
