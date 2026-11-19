---
id: operation-query-frame-stats-query-frame-stats
title: query_frame_stats
description: Values observed inside each frame for one counter. Missing values are null; delayed GPU/stat results keep their recorded frame provenance.
section: Reference / Developer API
parent: api-counters
order: 100
kind: api
symbol: query_frame_stats
revision: 108e36e44471
---

Values observed inside each frame for one counter. Missing values are null; delayed GPU/stat results keep their recorded frame provenance.

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
    "counter": {
      "type": "integer",
      "minimum": 0,
      "maximum": 2147483647
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
  },
  "description": "Metric units and type are returned per row. Int64 values are exact decimal strings, doubles are JSON numbers, unavailable values are null. Mean is a double approximation. Selected frames retain source observation/execution identity."
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke query_frame_stats --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Trace-relative seconds, inclusive. |
| `end?` | `number` |  | Trace-relative seconds, exclusive; must exceed start. |
| `offset?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `limit?` | `integer` | 20 | Response rows only; analysis still includes all samples. |
| `counter` | `integer` |  | See the input schema for constraints and allowed values. |
| `counterRevision?` | `string` |  | Optional revision returned by list_counters; rejects stale indices after provider reload. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[list_counters](doc:operation-list-counters-list-counters), [load_counters](doc:operation-load-counters-load-counters), [query_counters](doc:operation-query-counters-query-counters)
