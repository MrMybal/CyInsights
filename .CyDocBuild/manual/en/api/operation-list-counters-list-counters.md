---
id: operation-list-counters-list-counters
title: list_counters
description: Discover native counters, Meta metrics and frame statistics, types, units and availability reasons.
section: Reference / Developer API
parent: api-counters
order: 100
kind: api
symbol: list_counters
revision: 4a047243c834
---

Discover native counters, Meta metrics and frame statistics, types, units and availability reasons.

## Call contract {#call-contract}

Module: `tool` from the standalone AI SDK (`AI/app`). Submit the operation through the CyToolsCore runtime. The job result contains `outputs.data` and a task-local `outputs.reportFile`; an accepted action can still be running.

Permissions: Read-only; no extra operation permission.

## Operation input {#operation-input}

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
      "description": "Response rows only; analysis still includes all samples."
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
runtime.submit(actor, session_id, "list_counters", parameters)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `offset?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `limit?` | `integer` | 20 | Response rows only; analysis still includes all samples. |
| `search?` | `string` | "" | See the input schema for constraints and allowed values. |
| `group?` | `string` | "" | See the input schema for constraints and allowed values. |
| `metaOnly?` | `boolean` | false | See the input schema for constraints and allowed values. |
| `counter?` | `integer` | -1 | See the input schema for constraints and allowed values. |

## Returns {#returns}

`dict` — Job descriptor containing jobId. runtime.wait(actor, job_id) returns the job state and its outputs; application loading/capture completion is reported by get_state or capture_status.

## References {#references}

[load_counters](doc:operation-load-counters-load-counters), [query_counters](doc:operation-query-counters-query-counters), [query_frame_stats](doc:operation-query-frame-stats-query-frame-stats)
