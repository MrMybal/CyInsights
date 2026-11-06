---
id: operation-query-counters-query-counters
title: query_counters
description: Summaries over every observation in range, optional bounded time bins and sample pages. Integers remain exact strings; means are doubles. No carry-forward for missing samples.
section: Reference / Developer API
parent: api-counters
order: 100
kind: api
symbol: query_counters
revision: 4a047243c834
---

Summaries over every observation in range, optional bounded time bins and sample pages. Integers remain exact strings; means are doubles. No carry-forward for missing samples.

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
    },
    "bins": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 128,
      "description": "Optional time buckets; 0 omits the curve."
    },
    "includeSamples": {
      "type": "boolean",
      "default": false
    },
    "sampleOffset": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 9007199254740991
    },
    "counterRevision": {
      "type": "string",
      "pattern": "^[0-9]+$",
      "description": "Optional revision returned by list_counters; rejects stale indices after provider reload."
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
      "description": "Metric units and type are returned per row. Int64 values are exact decimal strings, doubles are JSON numbers, unavailable values are null. Mean is a double approximation. Selected frames retain source observation/execution identity."
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
runtime.submit(actor, session_id, "query_counters", parameters)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `start?` | `number` |  | Trace-relative seconds, inclusive. |
| `end?` | `number` |  | Trace-relative seconds, exclusive; must exceed start. |
| `offset?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `limit?` | `integer` | 20 | Response rows only; analysis still includes all samples. |
| `search?` | `string` | "" | See the input schema for constraints and allowed values. |
| `group?` | `string` | "" | See the input schema for constraints and allowed values. |
| `metaOnly?` | `boolean` | false | See the input schema for constraints and allowed values. |
| `counter?` | `integer` | -1 | See the input schema for constraints and allowed values. |
| `bins?` | `integer` | 0 | Optional time buckets; 0 omits the curve. |
| `includeSamples?` | `boolean` | false | See the input schema for constraints and allowed values. |
| `sampleOffset?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `counterRevision?` | `string` |  | Optional revision returned by list_counters; rejects stale indices after provider reload. |

## Returns {#returns}

`dict` — Job descriptor containing jobId. runtime.wait(actor, job_id) returns the job state and its outputs; application loading/capture completion is reported by get_state or capture_status.

## References {#references}

[list_counters](doc:operation-list-counters-list-counters), [load_counters](doc:operation-load-counters-load-counters), [query_frame_stats](doc:operation-query-frame-stats-query-frame-stats)
