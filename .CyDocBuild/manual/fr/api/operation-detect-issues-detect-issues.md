---
id: operation-detect-issues-detect-issues
title: detect_issues
description: Repérer les dépassements de budget avec et sans les attentes reconnues, leurs causes probables et les données GPU absentes. Ces heuristiques ne prouvent pas une causalité.
section: Référence / API développeur
parent: api-timing
order: 100
kind: api
symbol: detect_issues
sourceRevision: 4a047243c834
---

Repérer les dépassements de budget avec et sans les attentes reconnues, leurs causes probables et les données GPU absentes. Ces heuristiques ne prouvent pas une causalité.

## Contrat d’appel {#call-contract}

Module : `tool` du SDK IA autonome (`AI/app`). Soumettre l’opération avec le runtime CyToolsCore. Le résultat de tâche contient `outputs.data` et un fichier `outputs.reportFile` propre à la tâche ; une action acceptée peut encore être en cours.

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
    "targetFps": {
      "type": "number",
      "default": 60,
      "minimum": 1,
      "maximum": 1000
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
  "properties": {
    "data": {
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
    },
    "reportFile": {
      "type": "string",
      "x-cy-kind": "File"
    }
  },
  "required": [
    "data",
    "reportFile"
  ],
  "additionalProperties": false
}
```

## Signature {#signature}

```python
runtime.submit(actor, session_id, "detect_issues", parameters)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Secondes relatives à la trace, borne incluse. |
| `end?` | `number` |  | Secondes relatives à la trace, borne exclue ; doit être supérieure à start. |
| `targetFps?` | `number` | 60 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `limit?` | `integer` | 20 | Lignes de réponse uniquement ; l’analyse porte toujours sur tous les échantillons. |

## Retour {#returns}

`dict` — Descripteur de tâche contenant jobId. runtime.wait(actor, job_id) renvoie l’état et les sorties ; la fin du chargement ou de la capture dans l’application est indiquée par get_state ou capture_status.

## Références {#references}

[set_range](doc:operation-set-range-set-range), [filter_function](doc:operation-filter-function-filter-function), [list_threads](doc:operation-list-threads-list-threads)
