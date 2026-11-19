---
id: operation-pin-counter-pin-counter
title: pin_counter
description: Afficher ou masquer la courbe d’un compteur dans la timeline native partagée.
section: Référence / API développeur
parent: api-counters
order: 100
kind: api
symbol: pin_counter
sourceRevision: 108e36e44471
---

Afficher ou masquer la courbe d’un compteur dans la timeline native partagée.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

Permissions : `appControl`

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "counter": {
      "type": "integer",
      "minimum": 0,
      "maximum": 2147483647
    },
    "pinned": {
      "type": "boolean",
      "default": true
    },
    "counterRevision": {
      "type": "string",
      "pattern": "^[0-9]+$",
      "description": "Révision optionnelle fournie par list_counters ; elle permet de refuser un indice périmé après rechargement du fournisseur."
    }
  },
  "required": [
    "counter"
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
CyInsightsCli invoke pin_counter --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `counter` | `integer` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `pinned?` | `boolean` | true | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `counterRevision?` | `string` |  | Révision optionnelle fournie par list_counters ; elle permet de refuser un indice périmé après rechargement du fournisseur. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[list_counters](doc:operation-list-counters-list-counters), [load_counters](doc:operation-load-counters-load-counters), [query_counters](doc:operation-query-counters-query-counters)
