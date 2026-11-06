---
id: operation-query-gpu-query-gpu
title: query_gpu
description: Inspect the currently loaded optional Windows collector frame: commands, pipelines, shaders, resources. No texture/buffer bytes are returned; no fabricated Unreal identity or GPU duration. Quest uses native scopes and query_vulkan_passes.
section: Reference / Developer API
parent: api-gpu
order: 100
kind: api
symbol: query_gpu
revision: 4a047243c834
---

Inspect the currently loaded optional Windows collector frame: commands, pipelines, shaders, resources. No texture/buffer bytes are returned; no fabricated Unreal identity or GPU duration. Quest uses native scopes and query_vulkan_passes.

## Call contract {#call-contract}

Module: `tool` from the standalone AI SDK (`AI/app`). Submit the operation through the CyToolsCore runtime. The job result contains `outputs.data` and a task-local `outputs.reportFile`; an accepted action can still be running.

Permissions: Read-only; no extra operation permission.

## Operation input {#operation-input}

```json
{
  "type": "object",
  "properties": {
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
    "section": {
      "type": "string",
      "default": "summary",
      "enum": [
        "summary",
        "commands",
        "pipelines",
        "shaders",
        "resources",
        "passes",
        "dependencies"
      ]
    },
    "search": {
      "type": "string",
      "default": "",
      "maxLength": 4096
    },
    "pipeline": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 4294967295,
      "description": "Commands/pipelines only."
    },
    "event": {
      "type": "integer",
      "default": -1,
      "minimum": -1,
      "maximum": 4294967295,
      "description": "Commands only; -1 selects every recorded command."
    },
    "includeBindings": {
      "type": "boolean",
      "default": false,
      "description": "Commands only; bounded binding metadata, no image/buffer contents."
    },
    "includeDetails": {
      "type": "boolean",
      "default": false,
      "description": "Commands only; fixed draw state and bounded recorded barriers."
    },
    "detailOffset": {
      "type": "integer",
      "default": 0,
      "minimum": 0,
      "maximum": 9007199254740991,
      "description": "Commands only; page offset for both recorded bindings and barriers, with the usual limit."
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
      "description": "Capture-local command/pipeline/shader/resource IDs are decimal strings; gpuSeconds is measured only when delayed timings match the original frame. Pass names/dependencies are derived, not Unreal object identities."
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
runtime.submit(actor, session_id, "query_gpu", parameters)
```

## Parameters {#parameters}

| Name | Type | Default | Description |
| --- | --- | --- | --- |
| `offset?` | `integer` | 0 | See the input schema for constraints and allowed values. |
| `limit?` | `integer` | 20 | Response rows only; analysis still includes all samples. |
| `section?` | `string` | "summary" | See the input schema for constraints and allowed values. |
| `search?` | `string` | "" | See the input schema for constraints and allowed values. |
| `pipeline?` | `integer` | -1 | Commands/pipelines only. |
| `event?` | `integer` | -1 | Commands only; -1 selects every recorded command. |
| `includeBindings?` | `boolean` | false | Commands only; bounded binding metadata, no image/buffer contents. |
| `includeDetails?` | `boolean` | false | Commands only; fixed draw state and bounded recorded barriers. |
| `detailOffset?` | `integer` | 0 | Commands only; page offset for both recorded bindings and barriers, with the usual limit. |

## Returns {#returns}

`dict` — Job descriptor containing jobId. runtime.wait(actor, job_id) returns the job state and its outputs; application loading/capture completion is reported by get_state or capture_status.

## References {#references}

[query_vulkan_passes](doc:operation-query-vulkan-passes-query-vulkan-passes)
