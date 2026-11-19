---
id: operation-filter-function-filter-function
title: filter_function
description: Highlight every occurrence of an exact full name, across native identities; empty string clears.
section: Reference / Developer API
parent: api-timing
order: 100
kind: api
symbol: filter_function
revision: 108e36e44471
---

Highlight every occurrence of an exact full name, across native identities; empty string clears.

## Call contract {#call-contract}

Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.

Permissions: `appControl`

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "function": {
      "type": "string",
      "maxLength": 4096
    }
  },
  "required": [
    "function"
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
CyInsightsCli invoke filter_function --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `function` | `string` |  | See the input schema for constraints and allowed values. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[set_range](doc:operation-set-range-set-range), [list_threads](doc:operation-list-threads-list-threads), [list_functions](doc:operation-list-functions-list-functions)
