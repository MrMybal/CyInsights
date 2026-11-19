---
id: operation-set-range-set-range
title: set_range
description: Select a time interval shared by native analysis panels; optionally focus Timing.
section: Reference / Developer API
parent: api-timing
order: 100
kind: api
symbol: set_range
revision: 108e36e44471
---

Select a time interval shared by native analysis panels; optionally focus Timing.

## Call contract {#call-contract}

Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.

Permissions: `appControl`

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "start": {
      "type": "number",
      "description": "Trace-relative seconds, inclusive."
    },
    "end": {
      "type": "number",
      "description": "Trace-relative seconds, exclusive; must exceed start."
    },
    "focus": {
      "type": "boolean",
      "default": true
    }
  },
  "required": [
    "start",
    "end"
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
CyInsightsCli invoke set_range --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `start` | `number` |  | Trace-relative seconds, inclusive. |
| `end` | `number` |  | Trace-relative seconds, exclusive; must exceed start. |
| `focus?` | `boolean` | true | See the input schema for constraints and allowed values. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[filter_function](doc:operation-filter-function-filter-function), [list_threads](doc:operation-list-threads-list-threads), [list_functions](doc:operation-list-functions-list-functions)
