# CyInsightsAI

CyToolsCore companion for the existing native CyInsights application.
See ../README.md for tested setup, MCP connection, operations and limitations.
The SDK runtime/scheduler is installed from the pinned MIT wheel, not copied.
Run tool.py describe for the full contract; tool.py mcp for official MCP stdio.
Set CYINSIGHTS_APP_PID from the native AI Inspector window.
For additional trace tabs, also set CYINSIGHTS_DOCUMENT_ID from that tab's
copied connection configuration. Switching visible tabs does not retarget jobs.
