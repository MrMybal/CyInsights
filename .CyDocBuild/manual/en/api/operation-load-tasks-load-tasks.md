---
id: operation-load-tasks-load-tasks
title: load_tasks
description: Start the installed UE 5.5-5.8 native TaskTrace reader; poll get_state.
section: Reference / Developer API
parent: api-activities
order: 100
kind: api
symbol: load_tasks
revision: 108e36e44471
---

Start the installed UE 5.5-5.8 native TaskTrace reader; poll get_state.

## Call contract {#call-contract}

Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.

Permissions: `appControl`

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {},
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
CyInsightsCli invoke load_tasks --pid PID --params-file parameters.json
```

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[query_tasks](doc:operation-query-tasks-query-tasks), [query_task_links](doc:operation-query-task-links-query-task-links), [load_activities](doc:operation-load-activities-load-activities)
