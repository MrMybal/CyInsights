---
id: operation-query-activities-query-activities
title: query_activities
description: Lire des résumés par piste et, en option, des pages de 128 événements ou des classes temporelles. Les durées peuvent se chevaucher ; les jauges ne s’additionnent pas comme du trafic. kind isole réseau, stockage, chargement ou mémoire.
section: Référence / API développeur
parent: api-activities
order: 100
kind: api
symbol: query_activities
sourceRevision: 108e36e44471
---

Lire des résumés par piste et, en option, des pages de 128 événements ou des classes temporelles. Les durées peuvent se chevaucher ; les jauges ne s’additionnent pas comme du trafic. kind isole réseau, stockage, chargement ou mémoire.

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
    "kind": {
      "type": "string",
      "default": "all",
      "enum": [
        "all",
        "network",
        "storage",
        "loading",
        "memory"
      ]
    },
    "lane": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 2147483647
    },
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "includeEvents": {
      "type": "boolean",
      "default": false
    },
    "page": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 9007199254740991
    },
    "bins": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 128,
      "description": "Classes temporelles optionnelles ; 0 omet la courbe."
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
  "description": "Les lignes représentent des pistes. Les événements optionnels utilisent des pages de 128 lignes par piste et les classes temporelles ont une largeur uniforme. Identifiants, nombres et octets sont des chaînes décimales. Les jauges et bits de contenu ne sont pas des sommes de trafic."
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke query_activities --pid PID --params-file parameters.json
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Secondes relatives à la trace, borne incluse. |
| `end?` | `number` |  | Secondes relatives à la trace, borne exclue ; doit être supérieure à start. |
| `offset?` | `integer` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `limit?` | `integer` | 20 | Lignes de réponse uniquement ; l’analyse porte toujours sur tous les échantillons. |
| `kind?` | `string` | "all" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `lane?` | `integer` | -1 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `search?` | `string` | "" | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `includeEvents?` | `boolean` | false | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `page?` | `integer` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `bins?` | `integer` | 0 | Classes temporelles optionnelles ; 0 omet la courbe. |

## Retour {#returns}

`JSON` — Resultat natif structure. Les actions acceptees peuvent encore charger ; consulter get_state/capture_status. job_result fournit les sorties conservees des taches explicites.

## Références {#references}

[load_tasks](doc:operation-load-tasks-load-tasks), [query_tasks](doc:operation-query-tasks-query-tasks), [query_task_links](doc:operation-query-task-links-query-task-links)
