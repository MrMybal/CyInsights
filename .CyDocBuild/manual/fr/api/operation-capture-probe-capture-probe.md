---
id: operation-capture-probe-capture-probe
title: capture_probe
description: Interroger par IPv4 le plugin d’un jeu de développement ou d’un Quest, sans démarrer de capture.
section: Référence / API développeur
parent: api-capture
order: 100
kind: api
symbol: capture_probe
sourceRevision: 4a047243c834
---

Interroger par IPv4 le plugin d’un jeu de développement ou d’un Quest, sans démarrer de capture.

## Contrat d’appel {#call-contract}

Module : `tool` du SDK IA autonome (`AI/app`). Soumettre l’opération avec le runtime CyToolsCore. Le résultat de tâche contient `outputs.data` et un fichier `outputs.reportFile` propre à la tâche ; une action acceptée peut encore être en cours.

Permissions : `remoteCapture`

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "ipv4": {
      "type": "string",
      "minLength": 7,
      "maxLength": 15,
      "pattern": "^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$"
    },
    "port": {
      "type": "integer",
      "default": 41855,
      "minimum": 1,
      "maximum": 65535
    },
    "seconds": {
      "type": "number",
      "default": 0,
      "minimum": 0,
      "maximum": 3600
    }
  },
  "required": [
    "ipv4"
  ],
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
runtime.submit(actor, session_id, "capture_probe", parameters)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `ipv4` | `string` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `port?` | `integer` | 41855 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `seconds?` | `number` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |

## Retour {#returns}

`dict` — Descripteur de tâche contenant jobId. runtime.wait(actor, job_id) renvoie l’état et les sorties ; la fin du chargement ou de la capture dans l’application est indiquée par get_state ou capture_status.

## Références {#references}

[capture_start](doc:operation-capture-start-capture-start), [capture_gpu](doc:operation-capture-gpu-capture-gpu), [capture_status](doc:operation-capture-status-capture-status)
