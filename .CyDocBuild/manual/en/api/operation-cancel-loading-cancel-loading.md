---
id: operation-cancel-loading-cancel-loading
title: cancel_loading
description: Cancel a provider load, preserving the original capture. Does not stop recording or undo UI actions; poll get_state.
section: Reference / Developer API
parent: api-session
order: 100
kind: api
symbol: cancel_loading
revision: 4a047243c834
---

Cancel a provider load, preserving the original capture. Does not stop recording or undo UI actions; poll get_state.

## Call contract {#call-contract}

Module: `tool` from the standalone AI SDK (`AI/app`). Submit the operation through the CyToolsCore runtime. The job result contains `outputs.data` and a task-local `outputs.reportFile`; an accepted action can still be running.

Permissions: `appControl`

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "provider": {
      "type": "string",
      "enum": [
        "counters",
        "activities",
        "tasks",
        "comparison"
      ]
    }
  },
  "required": [
    "provider"
  ],
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
runtime.submit(actor, session_id, "cancel_loading", parameters)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `provider` | `string` |  | See the input schema for constraints and allowed values. |

## Returns {#returns}

`dict` — Job descriptor containing jobId. runtime.wait(actor, job_id) returns the job state and its outputs; application loading/capture completion is reported by get_state or capture_status.

## References {#references}

[get_state](doc:operation-get-state-get-state), [open_trace](doc:operation-open-trace-open-trace), [get_capabilities](doc:operation-get-capabilities-get-capabilities)
