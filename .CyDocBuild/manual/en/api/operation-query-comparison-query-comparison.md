---
id: operation-query-comparison-query-comparison
title: query_comparison
description: Rank B-minus-A timing changes using native full-population totals. Match by name/domain, never assume common IDs or clocks. Each interval has independent seconds.
section: Reference / Developer API
parent: api-timing
order: 100
kind: api
symbol: query_comparison
revision: 4a047243c834
---

Rank B-minus-A timing changes using native full-population totals. Match by name/domain, never assume common IDs or clocks. Each interval has independent seconds.

## Call contract {#call-contract}

Module: `tool` from the standalone AI SDK (`AI/app`). Submit the operation through the CyToolsCore runtime. The job result contains `outputs.data` and a task-local `outputs.reportFile`; an accepted action can still be running.

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
    "startB": {
      "type": "number"
    },
    "endB": {
      "type": "number"
    },
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "domain": {
      "type": "string",
      "default": "all",
      "enum": [
        "all",
        "cpu",
        "gpu"
      ]
    },
    "metric": {
      "type": "string",
      "default": "exclusive",
      "enum": [
        "inclusive",
        "exclusive"
      ]
    },
    "mode": {
      "type": "string",
      "default": "per_second",
      "enum": [
        "total",
        "per_second",
        "per_occurrence"
      ]
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
      },
      "description": "A/B costs use seconds, seconds per interval second, or seconds per occurrence according to mode. delta=B-A, deltaPercent is a percentage or null for missing/zero baseline. Correspondence is by name and domain."
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
runtime.submit(actor, session_id, "query_comparison", parameters)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Trace-relative seconds, inclusive. |
| `end?` | `number` |  | Trace-relative seconds, exclusive; must exceed start. |
| `offset?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `limit?` | `integer` | 20 | Response rows only; analysis still includes all samples. |
| `startB?` | `number` |  | See the input schema for constraints and allowed values. |
| `endB?` | `number` |  | See the input schema for constraints and allowed values. |
| `search?` | `string` | "" | See the input schema for constraints and allowed values. |
| `domain?` | `string` | "all" | See the input schema for constraints and allowed values. |
| `metric?` | `string` | "exclusive" | See the input schema for constraints and allowed values. |
| `mode?` | `string` | "per_second" | See the input schema for constraints and allowed values. |

## Returns {#returns}

`dict` — Job descriptor containing jobId. runtime.wait(actor, job_id) returns the job state and its outputs; application loading/capture completion is reported by get_state or capture_status.

## References {#references}

[set_range](doc:operation-set-range-set-range), [filter_function](doc:operation-filter-function-filter-function), [list_threads](doc:operation-list-threads-list-threads)
