---
id: ai
title: AI Inspector, MCP et SDK
description: Outils locaux, clients externes, droits et preuves ciblées.
section: Guides
order: 0
parent: developer-api
sourceRevision: 986e07f8cdc5
---

AI Inspector permet d’exécuter les outils d’analyse natifs et de consulter les constats. Ouvrez **Tools**, choisissez une opération, vérifiez ses paramètres, puis lancez-la. Préparer le formulaire n’exécute pas l’outil. Les outils natifs ne demandent ni modèle externe, ni Python, ni client MCP.

## Connexion externe {#external-connection}
1. Ouvrez la capture à analyser.
2. Ouvrez **AI Inspector > Connection** et activez le pont local MCP/SDK.
3. Copiez la configuration affichée dans votre client externe. Gardez les jetons de connexion privés.
4. Laissez le contrôle de l’application et la capture distante désactivés si vous souhaitez seulement analyser. Ces permissions sont distinctes.
5. Après un redémarrage de l’application ou un changement de document, utilisez la connexion affichée pour ce document.

Le SDK IA autonome contient l’adaptateur Python et ses dépendances. Il ne contient pas de modèle conversationnel : votre client fournit ce modèle. Les [références d’API publique](doc:developer-api) décrivent les contrats des opérations.

## Examiner une analyse {#compact-workflow}
Sélectionnez un court intervalle suspect avant de lancer un outil. Dans **Method**, suivez les étapes proposées et vérifiez le formulaire de paramètres obtenu. Comparez l’intervalle, les unités et la source d’un constat avec les [frames](doc:frames) ou les [données GPU](doc:gpu-desktop). Une cause probable reste une hypothèse.

Un fournisseur peut continuer à charger après l’acceptation d’une demande ; consultez la progression et la disponibilité affichées. Annulez le chargement si nécessaire. Arrêter une analyse n’arrête pas une capture distante : utilisez **Stop remote trace** pour cet enregistrement. Aucun rapport ne récupère les canaux absents de la capture.

## Investigation ciblée {#analysis-flow}

Les résumés orientent l’étude ; les événements individuels étayent un constat.

```diagram
{"type": "flow", "title": "Investigation ciblée", "nodes": [{"id": "n0", "label": "Vérifier sources/unités", "x": 0, "y": 0}, {"id": "n1", "label": "Repérer les frames lentes", "x": 310, "y": 0}, {"id": "n2", "label": "Choisir une courte zone", "x": 0, "y": 100}, {"id": "n3", "label": "Examiner un domaine", "x": 310, "y": 100}, {"id": "n4", "label": "Comparer un bon cas", "x": 0, "y": 200}, {"id": "n5", "label": "Preuves + limites", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
