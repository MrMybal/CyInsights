---
id: operation-load-counters-load-counters
title: load_counters
description: Load counter samples asynchronously. frameOnly reads plugin per-frame snapshots without the external counter export. Poll get_state.counters.
section: Reference / Developer API
parent: api-counters
order: 100
kind: api
symbol: load_counters
revision: 4a047243c834
---

Load counter samples asynchronously. frameOnly reads plugin per-frame snapshots without the external counter export. Poll get_state.counters.

## Call contract {#call-contract}

Module: `tool` from the standalone AI SDK (`AI/app`). Submit the operation through the CyToolsCore runtime. The job result contains `outputs.data` and a task-local `outputs.reportFile`; an accepted action can still be running.

Permissions: `appControl`

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "frameOnly": {
      "type": "boolean",
      "default": false
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
        },
        "accepted": {
          "type": "boolean",
          "description": "Action accepted, not completion."
        },
        "completion": {
          "type": "string",
          "description": "Poll the indicated get_state/capture_status fields."
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
runtime.submit(actor, session_id, "load_counters", parameters)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `frameOnly?` | `boolean` | false | See the input schema for constraints and allowed values. |

## Returns {#returns}

`dict` — Job descriptor containing jobId. runtime.wait(actor, job_id) returns the job state and its outputs; application loading/capture completion is reported by get_state or capture_status.

## References {#references}

[list_counters](doc:operation-list-counters-list-counters), [query_counters](doc:operation-query-counters-query-counters), [query_frame_stats](doc:operation-query-frame-stats-query-frame-stats)
