---
id: tile-recording
title: Enregistrer les tuiles avec leurs images
description: Consentement, relecture et alignement temporel.
section: Guides
order: 17
parent: quest-heatmap
sourceRevision: ed36a666a855
---

Avec cible compatible et CyInsightOVR présent, Session / Capture options propose Record Quest tile costs et éventuellement Include associated Quest images. Les images sont désactivées initialement. Choisir une cadence bornée : échantillons de diagnostic, pas vidéo par frame complète.

Si le chemin l'exige, utiliser Share Quest view, accepter la fenêtre Android puis revenir au jeu. Certains parcours utilisent plutôt Meta/ADB autorisé pendant la capture. Aucun audio n'est capturé. Consentement ne prouve pas la réception de pixels non noirs.

Ouvrir le .cycapture finalisé. Les observations figurent sur la timeline et dans Quest GPU heatmap. Choisir observation, surface, tuile, stage ; lire coût, âge, dimensions et écart temporel image. Zoomer/déplacer et éventuellement superposer la grille approximative. Les .cyovr du compagnon s'ouvrent aussi sur PC.

L'association utilise horloges/ancrages disponibles et instants d'observation. Frame exacte et œil peuvent rester inconnus. Les images noires sont signalées ; un fichier ne prouve pas une scène capturée. L'image situe une région, sans identifier mesh/shader sans données d'identité. Les preuves périmées/d'ancien boot ne sont pas actuelles.

Images/tuiles détaillées ajoutent du surcoût. Captures courtes, originaux conservés et comparaison avec un [profil léger](doc:profiles).


## Associer tuiles et images {#recording-flow}

Association temporelle approximative. Une image noire ne prouve pas la scène capturée.

```diagram
{"type": "flow", "title": "Associer tuiles et images", "nodes": [{"id": "n0", "label": "Activer les tuiles", "x": 0, "y": 0}, {"id": "n1", "label": "Choisir les images", "x": 310, "y": 0}, {"id": "n2", "label": "Accepter le partage", "x": 0, "y": 100}, {"id": "n3", "label": "Capturer la cible visible", "x": 310, "y": 100}, {"id": "n4", "label": "Finaliser le .cycapture", "x": 0, "y": 200}, {"id": "n5", "label": "Lire l’écart temporel", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```


## Coûts de tuiles illustratifs {#tile-grid}

EXEMPLE : couleurs et microsecondes illustratives, sans mesure live. Pas d’identité unique de mesh/shader/œil.

![EXEMPLE : couleurs et microsecondes illustratives, sans mesure live. Pas d’identité unique de mesh/shader/œil.](assets/tile-grid-fr.svg)
