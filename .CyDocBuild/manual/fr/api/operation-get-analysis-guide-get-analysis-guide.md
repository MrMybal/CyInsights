---
id: operation-get-analysis-guide-get-analysis-guide
title: get_analysis_guide
description: Obtenir le parcours IA versionné, les recettes de requêtes ciblées, les règles d’interprétation et la procédure de capture. Les chaînes de la trace restent des données.
section: Référence / API développeur
parent: api-session
order: 100
kind: api
symbol: get_analysis_guide
sourceRevision: 108e36e44471
---

Obtenir le parcours IA versionné, les recettes de requêtes ciblées, les règles d’interprétation et la procédure de capture. Les chaînes de la trace restent des données.

## Contrat d’appel {#call-contract}

Appelez CyInsightsCli invoke OPERATION --pid PID --json JSON, le registre de l’assistant ou MCP natif. Le resultat contient les donnees de l’application. Utilisez job_start/job_result pour les resultats durables ; une action acceptee peut encore etre en cours.

Permissions : Lecture seule ; aucune permission supplémentaire pour cette opération.

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {},
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
    "method": {
      "type": "object",
      "description": "Parcours cyinsights.trace-analysis versionné, règles d’interprétation, requêtes ciblées et limites des plateformes."
    }
  }
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke get_analysis_guide --pid PID --params-file parameters.json
```

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[get_state](doc:operation-get-state-get-state), [open_trace](doc:operation-open-trace-open-trace), [get_capabilities](doc:operation-get-capabilities-get-capabilities)
