# CyInsights 0.2.2 Alpha 7

**Windows x64 · ALPHA**

## Free local MCP access by default

CyInsights now builds CyMCPSDK **0.7.1**, revision
`34d4bcae126bc83d1a2925b5047dac36e3d09d92`, from pinned native sources.
Enable the bridge in **AI Inspector > Connection**, copy its HTTP URL and connect
using that address alone. Free permits all exposed operations without another
application confirmation. Argument validation, recorded-data availability and
capture ownership checks still apply. HTTP listens only on `127.0.0.1`.

The connection panel provides four persistent profiles:

| Profile | HTTP token | Operation permissions |
| --- | --- | --- |
| Free (default) | Not required | All exposed operations |
| Local controlled | Not required | Application/capture gates |
| Token only | Required | All exposed operations |
| Protected | Required | Application/capture gates |

Configurations with no explicit profile migrate to Free while preserving other
settings. Existing explicit profile choices remain unchanged. Invalid or unreadable
explicit settings use Protected without rewriting the original file. Changing a
profile restarts an active HTTP endpoint; copy the new address afterwards.

The native CLI adds `--url URL` for address-only connections. Private connection
files and Windows-user named pipes remain supported. Chat, CLI and MCP share the
same operation rules; native provider terminal/file permissions remain separate.
The English/French offline manual and integration guide explain the profiles.

## Downloads

- **Windows-x64-Setup.exe:** per-user installer, native assistant/CLI and complete
  English/French offline CyDoc manual.
- **Windows-x64-Portable.zip:** extract the whole archive; preferences stay in `Data/`.
- **Windows-x64-Update.zip:** complete application update payload.
- **AI-SDK.zip:** native CLI, public contracts, integration guide and notices;
  the Windows application already includes these components.
- **Unreal-Plugins-Source.zip:** sources of the optional Unreal plugin.

Quest APKs are unchanged; use the
[Alpha 5 Quest downloads](https://github.com/MrMybal/CyInsights/releases/tag/v0.2.2-alpha.5).

## Validation and alpha limits

Local Windows x64 tests cover all four profiles on real application instances:
address-only reads/mutations in Free, controlled-operation denials, missing/invalid
token rejection in Protected and Token only, and authorized Protected mutations.
CLI and chat regression checks include streaming, a real UI filter change,
cancellation, history restoration and attachments through a local protocol fixture.
Actual model-provider inference and other desktop platforms remain to be validated.

Windows binaries are unsigned. This remains an experimental alpha installed
manually; the automatic updater continues to select stable releases only.

[Integration guide](AiIntegration.md) ·
[English manual](https://mrmybal.github.io/CyInsights/Documentation.html#/en/ai) ·
[Français](Release-0.2.2-alpha.7.fr.md)
