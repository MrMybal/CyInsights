---
id: operation-capture-start-capture-start
title: capture_start
description: Démarrer une capture de développement distante et la transmettre au PC. Cette action persiste dans l’application ; annuler la tâche SDK n’arrête pas l’enregistrement.
section: Référence / API développeur
parent: api-capture
order: 100
kind: api
symbol: capture_start
sourceRevision: 4a047243c834
---

Démarrer une capture de développement distante et la transmettre au PC. Cette action persiste dans l’application ; annuler la tâche SDK n’arrête pas l’enregistrement.

## Contrat d’appel {#call-contract}

Module : `tool` du SDK IA autonome (`AI/app`). Soumettre l’opération avec le runtime CyToolsCore. Le résultat de tâche contient `outputs.data` et un fichier `outputs.reportFile` propre à la tâche ; une action acceptée peut encore être en cours.

Permissions : `remoteCapture`

## Entrée de l’opération {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "ipv4": {
      "type": "string",
      "minLength": 7,
      "maxLength": 15,
      "pattern": "^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$"
    },
    "port": {
      "type": "integer",
      "default": 41855,
      "minimum": 1,
      "maximum": 65535
    },
    "seconds": {
      "type": "number",
      "default": 0,
      "minimum": 0,
      "maximum": 3600
    },
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
    },
    "quest_tiles": {
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "enabled": {
          "type": "boolean",
          "default": false,
          "description": "Enregistrer les instantanés de tuiles CyInsightOVR dans la même archive cycapture ; une connexion ADB autorisée et le pont CyInsightOVR actif sont nécessaires."
        },
        "images": {
          "type": "boolean",
          "default": false,
          "description": "Enregistrer explicitement les images Quest optionnelles via le pont ADB autorisé lorsque le système le permet, sinon via le partage accepté par le système. Association approximative des observations ; aucun audio ni attribution exacte de frame GPU, d’œil ou de maillage."
        },
        "hz": {
          "type": "number",
          "default": 1,
          "minimum": 0.2,
          "maximum": 5,
          "description": "Nombre d’instantanés de tuiles demandé par seconde, indépendant des scopes GPU natifs par frame. La cadence des images dépend de la source et du temps de capture."
        }
      }
    },
    "meta_gpu": {
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "enabled": {
          "type": "boolean",
          "default": true,
          "description": "Étapes et compteurs matériels Quest via ADB autorisé. Cette option est activée par défaut."
        },
        "restart_game": {
          "type": "boolean",
          "default": false,
          "description": "Redémarrage explicite optionnel du jeu de développement après activation du profilage GPU. Par défaut, le jeu et sa configuration de profilage restent en cours."
        },
        "adb_path": {
          "type": "string",
          "default": "",
          "maxLength": 4096
        },
        "device": {
          "type": "string",
          "default": "",
          "maxLength": 255
        },
        "package": {
          "type": "string",
          "default": "",
          "maxLength": 255
        }
      }
    }
  },
  "required": [
    "ipv4"
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
runtime.submit(actor, session_id, "capture_start", parameters)
```

## Paramètres {#parameters}

| Nom | Type | Valeur par défaut | Description |
| --- | --- | --- | --- |
| `ipv4` | `string` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `port?` | `integer` | 41855 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `seconds?` | `number` | 0 | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `capture_options?` | `object` |  | Politique de capture v1 optionnelle. Omettre l’objet entier conserve les réglages du plugin dans le projet. Les lectures d’images et de textures sont explicites. Les statistiques désactivées ou absentes du build ne sont pas recréées. |
| `quest_tiles?` | `object` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |
| `meta_gpu?` | `object` |  | Voir le schéma d’entrée pour les contraintes et valeurs autorisées. |

## Retour {#returns}

`dict` — Descripteur de tâche contenant jobId. runtime.wait(actor, job_id) renvoie l’état et les sorties ; la fin du chargement ou de la capture dans l’application est indiquée par get_state ou capture_status.

## Références {#references}

[capture_probe](doc:operation-capture-probe-capture-probe), [capture_gpu](doc:operation-capture-gpu-capture-gpu), [capture_status](doc:operation-capture-status-capture-status)
