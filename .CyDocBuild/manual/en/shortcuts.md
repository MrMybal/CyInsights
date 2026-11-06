---
id: shortcuts
title: Controls and shortcuts
description: Timeline, images, keyboard and Quest scrolling.
section: Guides
order: 22
parent: reference
revision: 749fce63a1f0
---

| Context | Control | Result |
| --- | --- | --- |
| Desktop | F1 / Help > User manual | Open this guide |
| Desktop | Ctrl+O | Open recordings |
| Timing | Right drag / wheel | Pan tracks/time / zoom at pointer |
| Fine measurement ruler | Left drag | Select time range |
| Timing | F / Home | Fit selection / entire recording |
| Timing | Left / Right | Previous / next frame |
| Timing | Escape | Clear the time range |
| Scope | Click / double click | Select / exact-name filter |
| Empty timing space | Double click | Clear view filter |
| Scope | Right click | Histogram/context actions |
| Capture tab | Context menu | Detach or close document |
| Image viewport | Wheel / middle drag / double click | Zoom / pan / fit |
| Manual | Back / Forward | Visited-topic history |
| Manual | Ctrl+K | Global search |
| Manual image | Escape | Reduce the enlarged image |

Shortcuts apply to the appropriate focused view; text entry should not trigger document navigation. Toolbar actions remain accessible without a keyboard.

Quest configuration accepts controller/pointer selection and drag scrolling without accidentally toggling a checkbox. Keyboard and wheel/generic scrolling are also supported where available. Help opens CyDoc's local touch-scrollable reader, with a collapsible contents menu on narrow windows and in-window image enlargement. Escape reduces enlarged media.

Statistics placement belongs to [HUD](doc:quest-hud); tile position/opacity belongs to [Heatmap overlay](doc:quest-heatmap).


## Navigation and measurement {#timeline-navigation}

Schematic controls, not a trace. Both rulers share one time axis.

```diagram
{"type": "flow", "title": "Navigation and measurement", "nodes": [{"id": "n0", "label": "Right drag: pan", "x": 0, "y": 0}, {"id": "n1", "label": "Wheel: zoom", "x": 310, "y": 0}, {"id": "n2", "label": "Fine ruler: A to B", "x": 0, "y": 100}, {"id": "n3", "label": "Read duration", "x": 310, "y": 100}, {"id": "n4", "label": "Range filters queries", "x": 0, "y": 200}, {"id": "n5", "label": "F: fit range", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
