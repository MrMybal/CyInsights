---
id: developer-api
title: SDK and public API
description: Connect an external client and use the supported operation contracts.
section: Reference / Developer API
order: 50
revision: 0360810dc94d
---

For connection settings and the native tool interface, read [AI Inspector and external clients](doc:ai). The references below describe public Python adapter operations, their parameters, results and permissions. They do not expose implementation listings. A declared capability can still require a recorded source or an available provider.

## Python entry point {#python-entry}
```python
from pathlib import Path
from tool import create_runtime

with create_runtime(Path("data/jobs")) as runtime:
    client = runtime.create_client("reader")
    actor = runtime.authenticate(client["token"])
    session = runtime.create_session(actor, temporary=True)
    job = runtime.submit(actor, session["sessionId"], "get_state", {})
    result = runtime.wait(actor, job["jobId"])
```

Use the separately distributed AI SDK and its installed dependencies. `tool` is the adapter module in `AI/app`; it is not an import from the CyDoc library. Connect it to the document’s enabled bridge before invoking operations. See [state](doc:operation-get-state-get-state) and [capabilities](doc:operation-get-capabilities-get-capabilities).
