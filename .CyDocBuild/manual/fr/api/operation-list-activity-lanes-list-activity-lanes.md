---
id: operation-list-activity-lanes-list-activity-lanes
title: list_activity_lanes
description: Lister les pistes enregistrées, leurs unités et leurs nombres d’événements.
section: Référence / API développeur
parent: api-activities
order: 100
kind: api
symbol: list_activity_lanes
sourceRevision: 4a047243c834
---

Lister les pistes enregistrées, leurs unités et leurs nombres d’événements.

## Contrat d’appel {#call-contract}

Module : `tool` du SDK IA autonome (`AI/app`). Soumettre l’opération avec le runtime CyToolsCore. Le résultat de tâche contient `outputs.data` et un fichier `outputs.reportFile` propre à la tâche ; une action acceptée peut encore être en cours.

Permissions : Lecture seule ; aucune permission supplémentaire pour cette opération.

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "offset": {
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
    },
    "kind": {
      "type": "string",
      "default": "all",
      "enum": [
        "all",
        "network",
        "storage",
        "loading",
        "memory"
      ]
    },
    "lane": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 2147483647
    },
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
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
runtime.submit(actor, session_id, "list_activity_lanes", parameters)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `offset?` | `integer` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `limit?` | `integer` | 20 | Lignes de réponse uniquement ; l’analyse porte toujours sur tous les échantillons. |
| `kind?` | `string` | "all" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `lane?` | `integer` | -1 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `search?` | `string` | "" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |

## Retour {#returns}

`dict` — Descripteur de tâche contenant jobId. runtime.wait(actor, job_id) renvoie l’état et les sorties ; la fin du chargement ou de la capture dans l’application est indiquée par get_state ou capture_status.

## Références {#references}

[load_tasks](doc:operation-load-tasks-load-tasks), [query_tasks](doc:operation-query-tasks-query-tasks), [query_task_links](doc:operation-query-task-links-query-task-links)
