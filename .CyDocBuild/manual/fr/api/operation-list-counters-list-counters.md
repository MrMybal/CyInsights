---
id: operation-list-counters-list-counters
title: list_counters
description: Lister les compteurs natifs, les métriques Meta et les statistiques par frame, avec leurs types, unités et motifs d’indisponibilité.
section: Référence / API développeur
parent: api-counters
order: 100
kind: api
symbol: list_counters
sourceRevision: 108e36e44471
---

Lister les compteurs natifs, les métriques Meta et les statistiques par frame, avec leurs types, unités et motifs d’indisponibilité.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

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
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "group": {
      "type": "string",
      "default": "",
      "maxLength": 512
    },
    "metaOnly": {
      "type": "boolean",
      "default": false
    },
    "counter": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 2147483647
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
CyInsightsCli invoke list_counters --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `offset?` | `integer` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `limit?` | `integer` | 20 | Lignes de réponse uniquement ; l’analyse porte toujours sur tous les échantillons. |
| `search?` | `string` | "" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `group?` | `string` | "" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `metaOnly?` | `boolean` | false | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `counter?` | `integer` | -1 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[load_counters](doc:operation-load-counters-load-counters), [query_counters](doc:operation-query-counters-query-counters), [query_frame_stats](doc:operation-query-frame-stats-query-frame-stats)
