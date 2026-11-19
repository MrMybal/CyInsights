---
id: operation-histogram-histogram
title: histogram
description: All native scopes matching exact name, or frame thread costs. Compact quantiles and slowest samples; bins opt-in. Unfinished/absent values remain unavailable.
section: Reference / Developer API
parent: api-timing
order: 100
kind: api
symbol: histogram
revision: 108e36e44471
---

All native scopes matching exact name, or frame thread costs. Compact quantiles and slowest samples; bins opt-in. Unfinished/absent values remain unavailable.

## Call contract {#call-contract}

Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.

Permissions: Read-only; no extra operation permission.

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
    "source": {
      "type": "string",
      "enum": [
        "scopes",
        "frames"
      ],
      "default": "scopes"
    },
    "function": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "track": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 2147483647
    },
    "rootsOnly": {
      "type": "boolean",
      "default": false
    },
    "domain": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 2
    },
    "metric": {
      "type": "string",
      "enum": [
        "active",
        "wait",
        "total"
      ],
      "default": "active"
    },
    "logDurations": {
      "type": "boolean",
      "default": true
    },
    "includeBins": {
      "type": "boolean",
      "default": false
    },
    "limit": {
      "type": "integer",
      "default": 20,
      "minimum": 1,
      "maximum": 128,
      "description": "Response rows only; analysis still includes all samples."
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
CyInsightsCli invoke histogram --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Trace-relative seconds, inclusive. |
| `end?` | `number` |  | Trace-relative seconds, exclusive; must exceed start. |
| `source?` | `string` | "scopes" | See the input schema for constraints and allowed values. |
| `function?` | `string` | "" | See the input schema for constraints and allowed values. |
| `track?` | `integer` | -1 | See the input schema for constraints and allowed values. |
| `rootsOnly?` | `boolean` | false | See the input schema for constraints and allowed values. |
| `domain?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `metric?` | `string` | "active" | See the input schema for constraints and allowed values. |
| `logDurations?` | `boolean` | true | See the input schema for constraints and allowed values. |
| `includeBins?` | `boolean` | false | See the input schema for constraints and allowed values. |
| `limit?` | `integer` | 20 | Response rows only; analysis still includes all samples. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[set_range](doc:operation-set-range-set-range), [filter_function](doc:operation-filter-function-filter-function), [list_threads](doc:operation-list-threads-list-threads)
