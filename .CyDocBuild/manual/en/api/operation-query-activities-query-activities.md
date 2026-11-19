---
id: operation-query-activities-query-activities
title: query_activities
description: Per-lane summaries and optional 128-event pages/time bins. Durations can overlap; gauges are not summed as traffic. Use kind to isolate network, storage, loading or memory.
section: Reference / Developer API
parent: api-activities
order: 100
kind: api
symbol: query_activities
revision: 108e36e44471
---

Per-lane summaries and optional 128-event pages/time bins. Durations can overlap; gauges are not summed as traffic. Use kind to isolate network, storage, loading or memory.

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
    "kind": {
      "type": "string",
      "default": "all",
      "enum": [
        "all",
        "network",
        "storage",
        "loading",
        "memory"
      ]
    },
    "lane": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 2147483647
    },
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "includeEvents": {
      "type": "boolean",
      "default": false
    },
    "page": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 9007199254740991
    },
    "bins": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 128,
      "description": "Optional time buckets; 0 omits the curve."
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
  },
  "description": "Rows are lanes; optional events use 128-row pages per lane, optional bins have uniform time widths. Event IDs, counts and bytes are decimal strings. Gauge/content-bit values must not be interpreted as traffic sums."
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke query_activities --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Trace-relative seconds, inclusive. |
| `end?` | `number` |  | Trace-relative seconds, exclusive; must exceed start. |
| `offset?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `limit?` | `integer` | 20 | Response rows only; analysis still includes all samples. |
| `kind?` | `string` | "all" | See the input schema for constraints and allowed values. |
| `lane?` | `integer` | -1 | See the input schema for constraints and allowed values. |
| `search?` | `string` | "" | See the input schema for constraints and allowed values. |
| `includeEvents?` | `boolean` | false | See the input schema for constraints and allowed values. |
| `page?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `bins?` | `integer` | 0 | Optional time buckets; 0 omits the curve. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[load_tasks](doc:operation-load-tasks-load-tasks), [query_tasks](doc:operation-query-tasks-query-tasks), [query_task_links](doc:operation-query-task-links-query-task-links)
