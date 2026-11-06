---
id: operation-capture-gpu-capture-gpu
title: capture_gpu
description: Demander un instantané GPU borné pendant une capture distante détenue par cette application. Un enregistreur Windows connecté est nécessaire. Le transfert de cette charge GPU dans l’archive PC n’est pas implémenté ; consulter capture_status pour connaître l’acceptation ou le refus.
section: Référence / API développeur
parent: api-capture
order: 100
kind: api
symbol: capture_gpu
sourceRevision: 4a047243c834
---

Demander un instantané GPU borné pendant une capture distante détenue par cette application. Un enregistreur Windows connecté est nécessaire. Le transfert de cette charge GPU dans l’archive PC n’est pas implémenté ; consulter capture_status pour connaître l’acceptation ou le refus.

## Contrat d’appel {#call-contract}

Module : `tool` du SDK IA autonome (`AI/app`). Soumettre l’opération avec le runtime CyToolsCore. Le résultat de tâche contient `outputs.data` et un fichier `outputs.reportFile` propre à la tâche ; une action acceptée peut encore être en cours.

Permissions : `remoteCapture`

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "capture_options": {
      "type": "object",
      "properties": {
        "cpu": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "gpu": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "frame_stats": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "object_counts": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "memory_stats": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "stat_groups_enabled": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "tasks": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "network": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "file_io": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "loadtime": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "logs": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "memtags": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "quest_metrics": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "quest_gpu_completion": {
          "type": "boolean",
          "description": "Le profil de base active cette option. Sur Quest Vulkan : horodatages de fin et requêtes supplémentaires aux limites des passes, deux par passe, sans séparation des passes ni lecture des ressources. Cela peut modifier l’ordonnancement GPU ; les coûts des sous-passes par tuile peuvent rester inconnus. L’alignement CPU est approximatif. Le plugin doit annoncer quest_gpu_completion et être à jour pour les passes supplémentaires. Les réglages explicites de console ou de lancement sont conservés."
        },
        "gpu_passes": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "gpu_details": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "gpu_bindings": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "gpu_barriers": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "gpu_per_command": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "gpu_readbacks": {
          "type": "boolean",
          "description": "Remplacement explicite pour cette session ; les champs absents utilisent le profil de base du client."
        },
        "version": {
          "type": "integer",
          "enum": [
            1
          ]
        },
        "stat_groups": {
          "type": "string",
          "maxLength": 512
        },
        "quest_hz": {
          "type": "integer",
          "minimum": 0,
          "maximum": 120,
          "description": "0 observe les valeurs Meta à chaque frame de jeu ; une valeur positive limite la cadence. Les scopes GPU natifs sont enregistrés séparément à chaque frame de rendu."
        },
        "gpu_frames": {
          "type": "integer",
          "minimum": 1,
          "maximum": 8
        }
      },
      "required": [
        "version"
      ],
      "additionalProperties": false,
      "description": "Politique de capture v1 optionnelle. Omettre l’objet entier conserve les réglages du plugin dans le projet. Les lectures d’images et de textures sont explicites. Les statistiques désactivées ou absentes du build ne sont pas recréées."
    }
  },
  "required": [
    "capture_options"
  ],
  "additionalProperties": false
}
```

## Sortie de l’opération {#operation-output}

```json
{
  "type": "object",
  "properties": {
    "data": {
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
    },
    "reportFile": {
      "type": "string",
      "x-cy-kind": "File"
    }
  },
  "required": [
    "data",
    "reportFile"
  ],
  "additionalProperties": false
}
```

## Signature {#signature}

```python
runtime.submit(actor, session_id, "capture_gpu", parameters)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `capture_options` | `object` |  | Politique de capture v1 optionnelle. Omettre l’objet entier conserve les réglages du plugin dans le projet. Les lectures d’images et de textures sont explicites. Les statistiques désactivées ou absentes du build ne sont pas recréées. |

## Retour {#returns}

`dict` — Descripteur de tâche contenant jobId. runtime.wait(actor, job_id) renvoie l’état et les sorties ; la fin du chargement ou de la capture dans l’application est indiquée par get_state ou capture_status.

## Références {#references}

[capture_probe](doc:operation-capture-probe-capture-probe), [capture_start](doc:operation-capture-start-capture-start), [capture_status](doc:operation-capture-status-capture-status)
