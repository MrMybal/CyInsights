# CyInsights 0.2.2 Alpha 6

**Windows x64 · ALPHA**

## Native assistant, CLI and MCP

CyMCPSDK 0.7.0 replaces the Python/CyToolsCore companion. The Windows application
includes the official ChatUI panel and `CyInsightsCli.exe`; there is no runtime
installation step. Open **AI Inspector > Assistant**, detect providers or local
services, select a model and connect using your existing authentication.

The shared registry exposes 39 business operations and nine job/resource
utilities. The assistant, CLI and MCP use the same document permissions.
Application control can select/focus an interval, highlight a function, open a
histogram, show analysis panels and pin counter curves. It does not provide
arbitrary desktop control or automatic access to every image or file.

Conversations support streaming, cancellation, restoration and compatible
attachments. Durable host jobs preserve results across restarts without replay.
Compact discovery, chunked results, disk spooling, cache/notifications, private
metadata-only performance logs and API context limits are separately configurable.
Explicitly allowed remote HTTP MCP tools can be used in the chat.

Old runtime folders and saved data remain intact. Existing JSON reports can be
imported with `job_import`; the old CyTools wire protocol is replaced.

## Downloads

- **Windows-x64-Setup.exe:** per-user installer with the native assistant/CLI and
  complete English/French offline CyDoc manual.
- **Windows-x64-Portable.zip:** extract the whole archive; preferences stay in `Data/`.
- **Windows-x64-Update.zip:** complete payload for the application updater.
- **AI-SDK.zip:** the native CLI, contracts, guide and notices for external clients;
  the Windows app already includes them.
- **Unreal-Plugins-Source.zip:** the optional Unreal plugin sources.

This is a desktop integration update. Quest APKs are unchanged; use the
[Alpha 5 Quest downloads](https://github.com/MrMybal/CyInsights/releases/tag/v0.2.2-alpha.5).

## Validation and limits

32 local Windows x64 test suites passed, with CLI/MCP calls on real application
instances, allowed mutations and permission denials. A streaming API protocol
fixture verified chat rendering, tool use, cancellation, restoration and remote
tool errors. Actual provider inference/authentication and other desktop platforms
remain to be validated. Existing capture pack/inspect/extract commands were tested.

Windows binaries are unsigned. This experimental alpha is installed manually;
the application updater continues to select stable releases only.

[Native integration guide](AiIntegration.md) ·
[English manual](https://mrmybal.github.io/CyInsights/Documentation.html#/en/ai) ·
[Français](Release-0.2.2-alpha.6.fr.md)
