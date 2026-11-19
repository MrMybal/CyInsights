---
id: operation-capture-probe-capture-probe
title: capture_probe
description: Probe a development game/Quest plugin by IPv4; no capture starts.
section: Reference / Developer API
parent: api-capture
order: 100
kind: api
symbol: capture_probe
revision: 108e36e44471
---

Probe a development game/Quest plugin by IPv4; no capture starts.

## Call contract {#call-contract}

Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.

Permissions: `remoteCapture`

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "ipv4": {
      "type": "string",
      "minLength": 7,
      "maxLength": 15,
      "pattern": "^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$"
    },
    "port": {
      "type": "integer",
      "default": 41855,
      "minimum": 1,
      "maximum": 65535
    },
    "seconds": {
      "type": "number",
      "default": 0,
      "minimum": 0,
      "maximum": 3600
    }
  },
  "required": [
    "ipv4"
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
CyInsightsCli invoke capture_probe --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `ipv4` | `string` |  | See the input schema for constraints and allowed values. |
| `port?` | `integer` | 41855 | See the input schema for constraints and allowed values. |
| `seconds?` | `number` | 0 | See the input schema for constraints and allowed values. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[capture_start](doc:operation-capture-start-capture-start), [capture_gpu](doc:operation-capture-gpu-capture-gpu), [capture_status](doc:operation-capture-status-capture-status)
