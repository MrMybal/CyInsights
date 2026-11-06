---
id: welcome
title: Commencer ici
description: Choisir un parcours et utiliser ce guide hors ligne.
section: Guides
order: 0
sourceRevision: 13bfb8db9fde
---

CyInsights aide à comprendre où passe le temps d’une frame Unreal enregistrée. Utilisez l’application Windows pour les timelines CPU/GPU, les compteurs, les tâches et les activités. CyInsightOVR fournit les statistiques Quest et des surimpressions de développement optionnelles. Le plugin Unreal enregistre les données du jeu.

## Choisir un parcours {#choose-your-workflow}
1. Vous avez une capture ? [Ouvrez-la](doc:opening-traces), puis [parcourez la timeline](doc:timeline).
2. Vous voulez enregistrer un jeu ? [Installez le plugin](doc:unreal-plugin) et [choisissez un profil de capture](doc:profiles).
3. Vous utilisez un Quest ? [Configurez CyInsightOVR](doc:quest-setup), [choisissez les courbes du HUD](doc:quest-hud) ou [enregistrez les étapes GPU détaillées](doc:quest-capture).
4. Vous cherchez la cause d’un ralentissement ? Comparez les [coûts par frame](doc:frames), les [durées des fonctions](doc:functions) et les [données GPU](doc:gpu-desktop).

## Utiliser ce manuel {#use-this-manual}
La recherche en haut retrouve les pages, les exemples et les signatures de l’API publique. **Ctrl+K** ouvre la recherche globale. Dépliez une rubrique pour accéder à ses sous-pages. Survolez un titre, ou sélectionnez-le avec Tab, pour lire son libellé complet et sa description. English / Français change la langue du manuel indépendamment de l’application.

Les liens internes ouvrent la page ou la section concernée. Précédent/Suivant retrouve les pages visitées ; le lecteur conserve les positions de lecture et de la barre latérale. Un lien de section ouvre cette section. Le menu des thèmes propose des présentations claires, sombres et à fort contraste.

Les commandes d’image permettent de zoomer, de déplacer ou d’agrandir une image dans cette fenêtre. Échap la réduit. Les commandes sont disponibles au clavier et sur écran tactile. Les captures d’écran présentent des exemples enregistrés ; les schémas expliquent les parcours et ne mesurent pas votre capture.

Le dossier **Documentation/** complet accompagne l’application. Ouvrez directement **Documentation.html**, ou utilisez **Help > User manual / F1** sous Windows et **Help** sur Quest. La lecture ne demande ni compte, ni serveur, ni connexion. L’anglais et le français proposent le même manuel complet.


## De la capture à l’analyse {#capture-flow}

Schéma de parcours. Seules les sources enregistrées sont analysables.

```diagram
{"type": "flow", "title": "De la capture à l’analyse", "nodes": [{"id": "n0", "label": "Choisir les sources", "x": 0, "y": 0}, {"id": "n1", "label": "Enregistrer la cible", "x": 310, "y": 0}, {"id": "n2", "label": "Arrêter et finaliser", "x": 0, "y": 100}, {"id": "n3", "label": "Ouvrir le conteneur", "x": 310, "y": 100}, {"id": "n4", "label": "Charger les fournisseurs", "x": 0, "y": 200}, {"id": "n5", "label": "Sélectionner et examiner", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
