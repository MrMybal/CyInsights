---
id: operation-query-comparison-query-comparison
title: query_comparison
description: Classer les écarts B moins A à partir des totaux natifs de toute la population. La correspondance utilise le nom et le domaine, jamais une identité ou une horloge supposée commune. Chaque intervalle utilise ses propres secondes.
section: Référence / API développeur
parent: api-timing
order: 100
kind: api
symbol: query_comparison
sourceRevision: 108e36e44471
---

Classer les écarts B moins A à partir des totaux natifs de toute la population. La correspondance utilise le nom et le domaine, jamais une identité ou une horloge supposée commune. Chaque intervalle utilise ses propres secondes.

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
    "startB": {
      "type": "number"
    },
    "endB": {
      "type": "number"
    },
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "domain": {
      "type": "string",
      "default": "all",
      "enum": [
        "all",
        "cpu",
        "gpu"
      ]
    },
    "metric": {
      "type": "string",
      "default": "exclusive",
      "enum": [
        "inclusive",
        "exclusive"
      ]
    },
    "mode": {
      "type": "string",
      "default": "per_second",
      "enum": [
        "total",
        "per_second",
        "per_occurrence"
      ]
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
  },
  "description": "Selon mode, les coûts A/B sont en secondes, en secondes par seconde d’intervalle ou par occurrence. delta=B-A ; deltaPercent est un pourcentage, ou null si la référence manque ou vaut zéro. La correspondance utilise nom et domaine."
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke query_comparison --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Secondes relatives à la trace, borne incluse. |
| `end?` | `number` |  | Secondes relatives à la trace, borne exclue ; doit être supérieure à start. |
| `offset?` | `integer` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `limit?` | `integer` | 20 | Lignes de réponse uniquement ; l’analyse porte toujours sur tous les échantillons. |
| `startB?` | `number` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `endB?` | `number` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `search?` | `string` | "" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `domain?` | `string` | "all" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `metric?` | `string` | "exclusive" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `mode?` | `string` | "per_second" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[set_range](doc:operation-set-range-set-range), [filter_function](doc:operation-filter-function-filter-function), [list_threads](doc:operation-list-threads-list-threads)
