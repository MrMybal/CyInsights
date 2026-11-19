---
id: operation-query-gpu-query-gpu
title: query_gpu
description: Examiner la frame chargée du collecteur Windows optionnel : commandes, pipelines, shaders et ressources. Aucun contenu d’image ou de tampon n’est renvoyé ; les identités Unreal et les durées GPU ne sont pas inventées. Sur Quest, utiliser les scopes natifs et query_vulkan_passes.
section: Référence / API développeur
parent: api-gpu
order: 100
kind: api
symbol: query_gpu
sourceRevision: 108e36e44471
---

Examiner la frame chargée du collecteur Windows optionnel : commandes, pipelines, shaders et ressources. Aucun contenu d’image ou de tampon n’est renvoyé ; les identités Unreal et les durées GPU ne sont pas inventées. Sur Quest, utiliser les scopes natifs et query_vulkan_passes.

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
    "section": {
      "type": "string",
      "default": "summary",
      "enum": [
        "summary",
        "commands",
        "pipelines",
        "shaders",
        "resources",
        "passes",
        "dependencies"
      ]
    },
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "pipeline": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 4294967295,
      "description": "Commandes et pipelines uniquement."
    },
    "event": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 4294967295,
      "description": "Commandes uniquement ; -1 sélectionne toutes les commandes enregistrées."
    },
    "includeBindings": {
      "type": "boolean",
      "default": false,
      "description": "Commandes uniquement ; métadonnées de liaisons bornées, sans contenu d’image ou de tampon."
    },
    "includeDetails": {
      "type": "boolean",
      "default": false,
      "description": "Commandes uniquement ; état de dessin fixe et barrières enregistrées bornées."
    },
    "detailOffset": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 9007199254740991,
      "description": "Commandes uniquement ; décalage de page pour les liaisons et barrières enregistrées, avec la limite habituelle."
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
  "description": "Les identifiants locaux de commandes, pipelines, shaders et ressources sont des chaînes décimales. gpuSeconds est mesuré seulement si les résultats retardés correspondent à la frame d’origine. Les noms de passes et dépendances sont déduits, pas des identités d’objets Unreal."
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke query_gpu --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `offset?` | `integer` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `limit?` | `integer` | 20 | Lignes de réponse uniquement ; l’analyse porte toujours sur tous les échantillons. |
| `section?` | `string` | "summary" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `search?` | `string` | "" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `pipeline?` | `integer` | -1 | Commandes et pipelines uniquement. |
| `event?` | `integer` | -1 | Commandes uniquement ; -1 sélectionne toutes les commandes enregistrées. |
| `includeBindings?` | `boolean` | false | Commandes uniquement ; métadonnées de liaisons bornées, sans contenu d’image ou de tampon. |
| `includeDetails?` | `boolean` | false | Commandes uniquement ; état de dessin fixe et barrières enregistrées bornées. |
| `detailOffset?` | `integer` | 0 | Commandes uniquement ; décalage de page pour les liaisons et barrières enregistrées, avec la limite habituelle. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[query_vulkan_passes](doc:operation-query-vulkan-passes-query-vulkan-passes)
