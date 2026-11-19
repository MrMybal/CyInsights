---
id: operation-load-activities-load-activities
title: load_activities
description: Décoder en arrière-plan les sources réseau, stockage, chargement et mémoire enregistrées. Suivre get_state.activities ; les canaux absents ne sont pas recréés.
section: Référence / API développeur
parent: api-activities
order: 100
kind: api
symbol: load_activities
sourceRevision: 108e36e44471
---

Décoder en arrière-plan les sources réseau, stockage, chargement et mémoire enregistrées. Suivre get_state.activities ; les canaux absents ne sont pas recréés.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

Permissions : `appControl`

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {},
  "required": [],
  "additionalProperties": false
}
```

## Sortie de l’opération {#operation-output}

```json
{
  "type": "object",
  "additionalProperties": true,
  "properties": {
    "generation": {
      "type": "string",
      "description": "Génération de trace propre au document ; les annotations périmées sont refusées."
    },
    "range": {
      "type": "object",
      "properties": {
        "start": {
          "type": "number"
        },
        "end": {
          "type": "number"
        },
        "unit": {
          "type": "string",
          "enum": [
            "seconds"
          ]
        }
      }
    },
    "rows": {
      "type": "array",
      "items": {
        "type": "object",
        "additionalProperties": true
      },
      "description": "Nombre de lignes de réponse borné. Les agrégats et matched portent sur toute la population demandée."
    },
    "matched": {
      "type": "string",
      "description": "Nombre uint64 décimal exact, pas une valeur flottante."
    },
    "offset": {
      "type": "string",
      "description": "Décalage de réponse décimal exact."
    },
    "hasMore": {
      "type": "boolean"
    },
    "provenance": {
      "type": "string",
      "description": "Source mesurée, analyse dérivée ou heuristique ; une donnée absente ne vaut jamais zéro."
    },
    "accepted": {
      "type": "boolean",
      "description": "Action acceptée ; cela ne signifie pas qu’elle est terminée."
    },
    "completion": {
      "type": "string",
      "description": "Consulter les champs indiqués de get_state ou capture_status."
    }
  }
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke load_activities --pid PID --params-file parameters.json
```

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[load_tasks](doc:operation-load-tasks-load-tasks), [query_tasks](doc:operation-query-tasks-query-tasks), [query_task_links](doc:operation-query-task-links-query-task-links)
