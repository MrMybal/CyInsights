---
id: operation-filter-function-filter-function
title: filter_function
description: Mettre en évidence toutes les occurrences d’un nom complet exact, même si leurs identités natives diffèrent. Une chaîne vide retire le filtre.
section: Référence / API développeur
parent: api-timing
order: 100
kind: api
symbol: filter_function
sourceRevision: 108e36e44471
---

Mettre en évidence toutes les occurrences d’un nom complet exact, même si leurs identités natives diffèrent. Une chaîne vide retire le filtre.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

Permissions : `appControl`

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "function": {
      "type": "string",
      "maxLength": 4096
    }
  },
  "required": [
    "function"
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
    }
  }
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke filter_function --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `function` | `string` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[set_range](doc:operation-set-range-set-range), [list_threads](doc:operation-list-threads-list-threads), [list_functions](doc:operation-list-functions-list-functions)
