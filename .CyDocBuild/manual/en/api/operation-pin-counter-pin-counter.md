---
id: operation-pin-counter-pin-counter
title: pin_counter
description: Show or hide one counter curve on the shared native timeline.
section: Reference / Developer API
parent: api-counters
order: 100
kind: api
symbol: pin_counter
revision: 108e36e44471
---

Show or hide one counter curve on the shared native timeline.

## Call contract {#call-contract}

Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.

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
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke pin_counter --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `counter` | `integer` |  | See the input schema for constraints and allowed values. |
| `pinned?` | `boolean` | true | See the input schema for constraints and allowed values. |
| `counterRevision?` | `string` |  | Optional revision returned by list_counters; rejects stale indices after provider reload. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[list_counters](doc:operation-list-counters-list-counters), [load_counters](doc:operation-load-counters-load-counters), [query_counters](doc:operation-query-counters-query-counters)
