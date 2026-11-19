---
id: operation-capture-probe-capture-probe
title: capture_probe
description: Interroger par IPv4 le plugin d’un jeu de développement ou d’un Quest, sans démarrer de capture.
section: Référence / API développeur
parent: api-capture
order: 100
kind: api
symbol: capture_probe
sourceRevision: 108e36e44471
---

Interroger par IPv4 le plugin d’un jeu de développement ou d’un Quest, sans démarrer de capture.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

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
CyInsightsCli invoke capture_probe --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `ipv4` | `string` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `port?` | `integer` | 41855 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `seconds?` | `number` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[capture_start](doc:operation-capture-start-capture-start), [capture_gpu](doc:operation-capture-gpu-capture-gpu), [capture_status](doc:operation-capture-status-capture-status)
