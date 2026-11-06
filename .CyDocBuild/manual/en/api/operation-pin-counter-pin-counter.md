---
id: operation-pin-counter-pin-counter
title: pin_counter
description: Show or hide one counter curve on the shared native timeline.
section: Reference / Developer API
parent: api-counters
order: 100
kind: api
symbol: pin_counter
revision: 4a047243c834
---

Show or hide one counter curve on the shared native timeline.

## Call contract {#call-contract}

Module: `tool` from the standalone AI SDK (`AI/app`). Submit the operation through the CyToolsCore runtime. The job result contains `outputs.data` and a task-local `outputs.reportFile`; an accepted action can still be running.

Permissions: `appControl`

## Operation input {#operation-input}

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
      "description": "Optional revision returned by list_counters; rejects stale indices after provider reload."
    }
  },
  "required": [
    "counter"
  ],
  "additionalProperties": false
}
```

## Operation output {#operation-output}

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
          "description": "Document-local trace generation; reject stale annotations."
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
          "description": "Bounded response rows. Aggregates/matched counts include the entire requested population."
        },
        "matched": {
          "type": "string",
          "description": "Exact uint64 decimal count, not a floating-point value."
        },
        "offset": {
          "type": "string",
          "description": "Exact decimal response offset."
        },
        "hasMore": {
          "type": "boolean"
        },
        "provenance": {
          "type": "string",
          "description": "Measured source, derived analysis or heuristic; never treat missing evidence as zero."
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
runtime.submit(actor, session_id, "pin_counter", parameters)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `counter` | `integer` |  | See the input schema for constraints and allowed values. |
| `pinned?` | `boolean` | true | See the input schema for constraints and allowed values. |
| `counterRevision?` | `string` |  | Optional revision returned by list_counters; rejects stale indices after provider reload. |

## Returns {#returns}

`dict` — Job descriptor containing jobId. runtime.wait(actor, job_id) returns the job state and its outputs; application loading/capture completion is reported by get_state or capture_status.

## References {#references}

[list_counters](doc:operation-list-counters-list-counters), [load_counters](doc:operation-load-counters-load-counters), [query_counters](doc:operation-query-counters-query-counters)
