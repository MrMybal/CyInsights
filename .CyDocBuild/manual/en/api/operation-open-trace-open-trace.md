---
id: operation-open-trace-open-trace
title: open_trace
description: Open an existing application document by absolute path, read-only. Returns accepted, not analysis completion; poll get_state. No large trace duplication.
section: Reference / Developer API
parent: api-session
order: 100
kind: api
symbol: open_trace
revision: 108e36e44471
---

Open an existing application document by absolute path, read-only. Returns accepted, not analysis completion; poll get_state. No large trace duplication.

## Call contract {#call-contract}

Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.

Permissions: `appControl`

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "path": {
      "type": "string",
      "minLength": 1,
      "maxLength": 32760,
      "description": "Persistent local application document reference (.utrace or .cycapture), not a temporary task input."
    }
  },
  "required": [
    "path"
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
}
```

## Signature {#signature}

```cpp
CyInsightsCli invoke open_trace --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `path` | `string` |  | Persistent local application document reference (.utrace or .cycapture), not a temporary task input. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[get_state](doc:operation-get-state-get-state), [get_capabilities](doc:operation-get-capabilities-get-capabilities), [get_metadata](doc:operation-get-metadata-get-metadata)
