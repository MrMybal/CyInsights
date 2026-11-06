---
id: opening-traces
title: Ouvrir et gérer les captures
description: Formats, onglets, fenêtres détachées et chargement.
section: Guides
order: 3
parent: desktop
sourceRevision: 1229dff198de
---

Utiliser File > Open capture ou Ctrl+O. Plusieurs captures s'ouvrent dans des onglets indépendants. Le menu d'un onglet permet une fenêtre native détachée. Chaque document conserve vue, filtre, sélection et analyse. Fermer un document ne supprime pas sa source.

## Formats {#formats}
- .utrace : trace Unreal originale, lue par les lecteurs natifs/fournisseurs d'export disponibles.
- .cycapture : conteneur versionné réunissant timings, événements Meta optionnels, compteurs et enregistrements tuiles/images disponibles.
- .pftrace : source Meta/Perfetto prise en charge, pas automatiquement une session Unreal complète.
- .cyovr : snapshot de tuiles du compagnon, pas une session de timings complète à lui seul.

La préparation travaille en arrière-plan. Utiliser progression/annulation plutôt que lancer en double. Les index persistants accélèrent les lectures suivantes mais le premier parcours prend du temps. Un fournisseur annulé/incomplet est indisponible, pas de coût nul. Fournisseurs natifs et compteurs ordinaires peuvent suivre des voies différentes.

Les originaux restent en lecture seule. Index, exports et conteneurs sont distincts. Les données .utrace entrantes sont finalisées avant l'analyse native ; des octets reçus ne prouvent pas une analyse des frames en direct.

Suite : [Timeline](doc:timeline), [Compteurs](doc:counters), [Comparaison](doc:comparison).


## Vraie timeline CPU/GPU {#timeline}

Vue réelle : frames, scopes et stages GPU disponibles partagent un axe. Trace d’exemple, pas votre résultat actuel.

```image
{"id": "timeline", "src": "assets/cyinsights-timeline.png", "alt": "Vraie timeline CPU/GPU", "controls": "minimal"}
```
