# CyInsights 0.2.2 Alpha 4

[Retour au README](../README.fr.md) · [English](Release-0.2.2-alpha.4.md)

**Version ordinateur et Quest : 0.2.2-alpha.4 · Statut : ALPHA**<br>
**Versions disponibles : Windows x64 et Android/Quest arm64**<br>
**Descripteur du plugin Unreal livré : 0.2.1 (sources)**

Une préversion expérimentale destinée aux essais et aux retours. La version du
descripteur du plugin est indépendante de celle de l’application et du paquet.

## Documentation anglaise et française intégrée

L’application Windows et CyInsightOVR incluent désormais le lecteur CyDoc actuel.
Ouvrez **Help > User manual** ou **F1** sur Windows, ou **Help** sur Quest.
Le manuel contient des guides complets en anglais et en français, une navigation
par tâches, une recherche dans l’en-tête global, des infobulles de titre et de
description, des captures et des schémas explicatifs. Les images s’agrandissent
dans la fenêtre du lecteur ; Échap les réduit.

Le manuel s’ouvre directement depuis `Documentation/Documentation.html`.
Distribuez tout le dossier `Documentation`. Scripts, styles, médias et notices
de licence sont inclus dans `DocData` ; la lecture ne nécessite ni serveur ni
connexion. Les instructions pour les assistants restent dans un `DocAI.md`
séparé. Les références des API publiques décrivent les 39 opérations prises en
charge, leurs paramètres, résultats et droits sans exposer les implémentations.
Les historiques de développement restent en dehors du lecteur.

Le même lecteur est disponible dans la [documentation CyInsights](https://mrmybal.github.io/CyInsights/Documentation.html#/fr/welcome).

## Commandes du HUD Quest

Les graphiques du HUD, ses valeurs numériques et les graphiques de la fenêtre
de configuration ont des sélections indépendantes. Les compteurs de fréquence
des draw calls et triangles peuvent être sélectionnés sans effacer les autres
métriques. Les valeurs Meta natives et les métriques personnalisées des jeux de
développement conservent leurs disponibilités et prérequis distincts. Les valeurs
non prises en charge, périmées ou manquantes restent indisponibles au lieu de devenir zéro.

Les compteurs Vulkan personnalisés et l’overlay de tuiles nécessitent l’APK
Vulkan, une cible débogable compatible et sa couche préparée. Une fermeture et
réouverture complète de la cible peut être nécessaire après avoir changé cette
couche. Cette alpha ne fournit pas un overlay personnalisé universel pour les
jeux commerciaux non modifiés.

## Téléchargements

- Archive ZIP de l’application Windows x64.
- APK Quest autonomes Standard et Vulkan ; installez une seule variante.
- Paquet Quest avec les deux APK et l’installateur optionnel d’autorisation développeur.
- Sources du plugin Unreal et paquet MCP/SDK IA autonome.

Cette version reste une **préversion alpha**. Le système de mise à jour de
l’application ne propose que les nouvelles versions stables ; installez cette
alpha manuellement. Les lecteurs natifs `.utrace` se préparent localement avec
une installation Unreal compatible et licenciée ainsi que les outils C++.
Aucun binaire Unreal Engine n’est inclus.

[Téléchargements et notes publiées](https://github.com/MrMybal/CyInsights/releases/tag/v0.2.2-alpha.4) · [Empreintes SHA-256](https://github.com/MrMybal/CyInsights/releases/download/v0.2.2-alpha.4/SHA256SUMS.txt)

## Problèmes connus

Les notes publiées ne fournissent pas de liste détaillée de bugs confirmés.
Consultez le [suivi des problèmes](https://github.com/MrMybal/CyInsights/issues)
pour les signalements et mises à jour. Les limites documentées ci-dessous
n’impliquent pas l’absence d’autres bugs.

## Limites documentées

- Le paquet de bureau cible Windows x64 ; aucune application Linux/macOS n’est
  fournie. La préparation native `.utrace` nécessite un UE 5.5–5.8 compatible et
  licencié ainsi que les outils C++/CMake. Elle compile les adaptateurs localement,
  sans recompiler Unreal.
- Les sources du plugin Unreal ciblent UE 4.27 et 5.0–5.8, avec des validations
  manquantes selon les versions. Shipping, Test et Server sont exclus. Consultez
  la [matrice de compatibilité](UnrealCompatibility.md).
- Les traces entrantes sont analysées après l’arrêt de l’enregistrement. La
  lecture native des logs du jeu, l’exploration des piles d’allocations mémoire et
  plusieurs lecteurs d’Unreal Insights restent incomplets.
- Les métriques système Quest sont des observations, sans identité garantie
  d’une frame d’exécution GPU ou d’un œil précis. Les sources détaillées dépendent
  du firmware et de la préparation de la cible ; les valeurs manquantes ou périmées
  restent indisponibles.
- La heatmap Vulkan et les compteurs personnalisés de draw calls/triangles
  nécessitent une cible débogable compatible et préparée. Ce ne sont pas des
  overlays personnalisés universels pour tous les APK commerciaux. L’association
  des tuiles et images peut être approximative ; consultez les
  [limites de l’enregistrement des tuiles](QuestTileRecording.md).
- Le détail des commandes/ressources Windows nécessite le collecteur optionnel
  et un runtime ReShade configuré séparément. Une demande distante de snapshot
  GPU détaillé ne transfère pas actuellement ce contenu Windows au récepteur.
  Les lectures d’images/buffers restent une option distincte.
- Les outils IA exposent les lecteurs et droits implémentés ; ils n’ajoutent ni
  analyse native en cours de capture ni timestamps GPU manquants. Consultez la
  [couverture des opérations](CapabilityCoverage.md).

## Références

- [Manuel](https://mrmybal.github.io/CyInsights/Documentation.html#/fr/welcome) · [Présentation](https://mrmybal.github.io/CyInsights/?lang=fr).
- [Toutes les releases](https://github.com/MrMybal/CyInsights/releases) · [Signaler un problème](https://github.com/MrMybal/CyInsights/issues/new).
