---
id: tile-recording
title: Record tiles with associated images
description: Consent, timeline replay and observation-time alignment.
section: Guides
order: 17
parent: quest-heatmap
revision: ed36a666a855
---

With a compatible target and CyInsightOVR available, Session / Capture options can Record Quest tile costs and optionally Include associated Quest images. Images are off initially. Choose a bounded snapshot rate: diagnostic samples, not a full per-frame video.

If the image route requires sharing, use Share Quest view in CyInsightOVR, accept Android's dialog and return to the game. Supported workflows can instead use the authorized Meta/ADB capture path during recording. No audio is captured. Consent does not prove non-black game pixels arrived.

Open the finalized .cycapture. Observations appear on the timeline and in Quest GPU heatmap. Select observation, surface, tile and stage; inspect cost, sample age, dimensions and image-time difference. Zoom/pan the image and optionally overlay the approximate tile grid. Companion .cyovr snapshots also open on the PC.

Association uses available clocks/anchors and observation times. Exact execution-frame and eye can be unknown. Black images remain flagged; a saved file alone is not a captured scene. An image helps find a region but does not identify a mesh/shader without identity data. Old-boot or stale evidence is not current data.

Images/detailed tiles add overhead. Use short captures, preserve originals and compare with a [light profile](doc:profiles).


## Associate tiles and images {#recording-flow}

Observation-time association is approximate. A black image does not prove scene capture.

```diagram
{"type": "flow", "title": "Associate tiles and images", "nodes": [{"id": "n0", "label": "Enable tiles", "x": 0, "y": 0}, {"id": "n1", "label": "Opt in to images", "x": 310, "y": 0}, {"id": "n2", "label": "Accept consent", "x": 0, "y": 100}, {"id": "n3", "label": "Record visible target", "x": 310, "y": 100}, {"id": "n4", "label": "Finalize .cycapture", "x": 0, "y": 200}, {"id": "n5", "label": "Inspect time delta", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```


## Illustrative tile costs {#tile-grid}

EXAMPLE ONLY: colors and microseconds are illustrative, not live measurements. No unique mesh/shader/eye attribution.

![EXAMPLE ONLY: colors and microseconds are illustrative, not live measurements. No unique mesh/shader/eye attribution.](assets/tile-grid-en.svg)
