---
id: operation-capture-gpu-capture-gpu
title: capture_gpu
description: Request a bounded GPU snapshot during this app-owned remote recording. Requires a connected Windows collector recorder. No GPU payload transfer to the PC archive is implemented; poll capture_status for acceptance/refusal.
section: Reference / Developer API
parent: api-capture
order: 100
kind: api
symbol: capture_gpu
revision: 108e36e44471
---

Request a bounded GPU snapshot during this app-owned remote recording. Requires a connected Windows collector recorder. No GPU payload transfer to the PC archive is implemented; poll capture_status for acceptance/refusal.

## Call contract {#call-contract}

Call CyInsightsCli invoke OPERATION --pid PID --json JSON, the shared assistant registry, or native MCP. Results are structured application data. Use job_start/job_result for durable results; accepted actions may still be running.

Permissions: `remoteCapture`

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {
    "capture_options": {
      "type": "object",
      "properties": {
        "cpu": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "gpu": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "frame_stats": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "object_counts": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "memory_stats": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "stat_groups_enabled": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "tasks": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "network": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "file_io": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "loadtime": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "logs": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "memtags": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "quest_metrics": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "quest_gpu_completion": {
          "type": "boolean",
          "description": "Basic profile defaults true. Quest Vulkan only: completion-stage timestamps and supplemental render-pass boundary queries during capture. Two queries per Vulkan pass; no pass splitting or resource readbacks. May affect GPU scheduling; tile subpass costs can remain unresolved. CPU alignment is approximate. Requires a target plugin advertising quest_gpu_completion; supplemental passes require the updated plugin. Explicit native console/launch stage overrides are retained."
        },
        "gpu_passes": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "gpu_details": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "gpu_bindings": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "gpu_barriers": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "gpu_per_command": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "gpu_readbacks": {
          "type": "boolean",
          "description": "Explicit session override; absent fields use the client basic profile."
        },
        "version": {
          "type": "integer",
          "enum": [
            1
          ]
        },
        "stat_groups": {
          "type": "string",
          "maxLength": 512
        },
        "quest_hz": {
          "type": "integer",
          "minimum": 0,
          "maximum": 120,
          "description": "0 observes Meta values every game frame; positive rates throttle. Native GPU scopes are independently recorded per render frame."
        },
        "gpu_frames": {
          "type": "integer",
          "minimum": 1,
          "maximum": 8
        }
      },
      "required": [
        "version"
      ],
      "additionalProperties": false,
      "description": "Optional capture policy v1. Omit the entire object to use plugin project defaults. Image/texture readbacks are opt-in. Disabled/compiled-out stats cannot be recreated."
    }
  },
  "required": [
    "capture_options"
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
CyInsightsCli invoke capture_gpu --pid PID --params-file parameters.json
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `capture_options` | `object` |  | Optional capture policy v1. Omit the entire object to use plugin project defaults. Image/texture readbacks are opt-in. Disabled/compiled-out stats cannot be recreated. |

## Returns {#returns}

`JSON` — Structured native result. Accepted actions may still be loading; poll get_state/capture_status. job_result provides retained output for explicit background jobs.

## References {#references}

[capture_probe](doc:operation-capture-probe-capture-probe), [capture_start](doc:operation-capture-start-capture-start), [capture_status](doc:operation-capture-status-capture-status)
