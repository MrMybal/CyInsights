---
id: operation-open-trace-open-trace
title: open_trace
description: Ouvrir en lecture seule un document existant par son chemin absolu. Le résultat confirme l’acceptation, pas la fin de l’analyse ; consulter get_state. La trace n’est pas dupliquée.
section: Référence / API développeur
parent: api-session
order: 100
kind: api
symbol: open_trace
sourceRevision: 108e36e44471
---

Ouvrir en lecture seule un document existant par son chemin absolu. Le résultat confirme l’acceptation, pas la fin de l’analyse ; consulter get_state. La trace n’est pas dupliquée.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

Permissions : `appControl`

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "path": {
      "type": "string",
      "minLength": 1,
      "maxLength": 32760,
      "description": "Référence persistante d’un document local de l’application (.utrace ou .cycapture), pas une entrée temporaire de tâche."
    }
  },
  "required": [
    "path"
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
CyInsightsCli invoke open_trace --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `path` | `string` |  | Référence persistante d’un document local de l’application (.utrace ou .cycapture), pas une entrée temporaire de tâche. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[get_state](doc:operation-get-state-get-state), [get_capabilities](doc:operation-get-capabilities-get-capabilities), [get_metadata](doc:operation-get-metadata-get-metadata)
