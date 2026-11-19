---
id: operation-load-counters-load-counters
title: load_counters
description: Charger les échantillons des compteurs en arrière-plan. frameOnly lit les instantanés du plugin par frame sans export externe des compteurs. Suivre get_state.counters.
section: Référence / API développeur
parent: api-counters
order: 100
kind: api
symbol: load_counters
sourceRevision: 108e36e44471
---

Charger les échantillons des compteurs en arrière-plan. frameOnly lit les instantanés du plugin par frame sans export externe des compteurs. Suivre get_state.counters.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

Permissions : `appControl`

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "frameOnly": {
      "type": "boolean",
      "default": false
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
CyInsightsCli invoke load_counters --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `frameOnly?` | `boolean` | false | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[list_counters](doc:operation-list-counters-list-counters), [query_counters](doc:operation-query-counters-query-counters), [query_frame_stats](doc:operation-query-frame-stats-query-frame-stats)
