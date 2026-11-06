---
id: developer-api
title: SDK et API publique
description: Connecter un client externe et utiliser les contrats des opérations disponibles.
section: Référence / API développeur
order: 50
sourceRevision: 0360810dc94d
---

Pour la connexion et l’interface des outils natifs, lisez [AI Inspector et les clients externes](doc:ai). Les références ci-dessous décrivent les opérations publiques de l’adaptateur Python, leurs paramètres, leurs résultats et leurs permissions. Elles ne donnent pas accès aux implémentations. Une capacité déclarée peut nécessiter une source enregistrée ou un fournisseur disponible.

## Point d’entrée Python {#python-entry}
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

Utilisez le SDK IA distribué séparément et ses dépendances installées. `tool` est le module adaptateur dans `AI/app` ; il ne vient pas de la bibliothèque CyDoc. Connectez-le au pont activé du document avant d’appeler une opération. Voir l’[état](doc:operation-get-state-get-state) et les [capacités](doc:operation-get-capabilities-get-capabilities).
