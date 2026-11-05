# AI Inspector, MCP and SDK {#title}

AI Inspector includes a guided Method and native Tools catalog. These run locally without CyAiCodex, Python or an MCP client. Preparing parameters does not execute a tool. An external conversational model runs in your chosen client; no model provider is embedded.

## External connection {#external-connection}
In AI Inspector > Connection, explicitly enable the local MCP/SDK bridge for the open document. Read-only access suffices for analysis. Application-control and remote-capture permissions are separate and initially off. Use the generated endpoint for this process/document; a new process can need new configuration. Keep bridge tokens private.

The standalone AI/ package supplies MCP/SDK companion, offline Python dependencies and analysis method. Discover runtime schemas/providers instead of assuming readiness. The present catalog has 39 shared operations; capabilities can evolve.

## Compact workflow {#compact-workflow}
Discover generation/providers, find a few slow frames, select a short range, rank exclusive hotspots, inspect focused GPU/histogram evidence and publish findings with uncertainty. Start with summaries and limits of 5–20. Request individual events/bindings/bins only when useful. UI and MCP share native analytical handlers.

Poll long jobs until Completed/Failed/Cancelled. Export needed results before finishing an ephemeral task. Cancelling analysis does not stop an owned remote recording: explicitly stop it. Finishing an AI task never deletes a persistent trace reference. Queries cannot recover missing channels.

A report names capture generation, interval, units, measured facts, derived results, suspected cause and a concrete next check. See [Glossary](doc:glossary) and [Comparison](doc:comparison).
