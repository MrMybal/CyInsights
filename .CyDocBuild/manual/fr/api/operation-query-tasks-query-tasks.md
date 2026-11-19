---
id: operation-query-tasks-query-tasks
title: query_tasks
description: Parcourir les tâches natives enregistrées ou, sur demande explicite, les candidats issus des scopes CPU. Aucun délai de file n’est inventé.
section: Référence / API développeur
parent: api-activities
order: 100
kind: api
symbol: query_tasks
sourceRevision: 108e36e44471
---

Parcourir les tâches natives enregistrées ou, sur demande explicite, les candidats issus des scopes CPU. Aucun délai de file n’est inventé.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

Permissions : Lecture seule ; aucune permission supplémentaire pour cette opération.

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "start": {
      "type": "number",
      "description": "Secondes relatives à la trace, borne incluse."
    },
    "end": {
      "type": "number",
      "description": "Secondes relatives à la trace, borne exclue ; doit être supérieure à start."
    },
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "thread": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 4294967295
    },
    "candidates": {
      "type": "boolean",
      "default": false
    },
    "page": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 9007199254740991
    },
    "limit": {
      "type": "integer",
      "default": 20,
      "minimum": 1,
      "maximum": 128,
      "description": "Lignes de réponse uniquement ; l’analyse porte toujours sur tous les échantillons."
    }
  },
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
    }
  }
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke query_tasks --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Secondes relatives à la trace, borne incluse. |
| `end?` | `number` |  | Secondes relatives à la trace, borne exclue ; doit être supérieure à start. |
| `search?` | `string` | "" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `thread?` | `integer` | -1 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `candidates?` | `boolean` | false | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `page?` | `integer` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `limit?` | `integer` | 20 | Lignes de réponse uniquement ; l’analyse porte toujours sur tous les échantillons. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[load_tasks](doc:operation-load-tasks-load-tasks), [query_task_links](doc:operation-query-task-links-query-task-links), [load_activities](doc:operation-load-activities-load-activities)
