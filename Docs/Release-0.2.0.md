# CyInsights 0.2.0 preview

## Interface et analyse

- Intégration de CyUI : cinq thèmes, densités compacte et confortable,
  écran de capture adaptatif et réglages avancés repliables.
- Commandes d'analyse regroupées, recherche avec effacement, menu des panneaux
  et barre d'état rappelant les interactions.
- Plusieurs captures indépendantes, avec onglets et fenêtres natives détachables.
- Déplacement continu pendant les chargements, frames synchronisées, hauteur
  des pistes adaptée au contenu visible, règle de mesure et sélection d'intervalle.
- Filtre exact au double clic et histogramme de fonction depuis le menu
  contextuel, limité à l'intervalle sélectionné.
- Réduction des commandes de rendu des labels de timeline ; préparation et
  requêtes bornées en arrière-plan pour les index natifs sur disque.
- Outils IA/MCP, SDK optionnel et méthode d'analyse disponibles depuis le produit
  autonome ; l'inspecteur natif fonctionne sans Python.

## Capture et GPU

- Un seul plugin CyInsights avec modules Windows internes facultatifs,
  configurations de capture et commandes réseau. Les copies de textures/buffers
  restent désactivées par défaut.
- Sources natives réseau, fichiers, chargement et mémoire consultables avec la
  timeline ; leur disponibilité dépend des canaux et lecteurs installés.
- Import des stages matériels Meta sur des pistes GPU distinctes, en préservant
  les horloges, l'identité inconnue et les limites d'association aux groupes Unreal.
- Correction du script ADB de récupération et du contrôle des dispatchs Vulkan
  pour UE 5.5 standard, qui n'exporte pas EndRenderPass2 comme le fork Meta.
- Petit projet Quest reproductible et outils de diagnostic des groupes réellement
  enregistrés, sans modification du moteur.

## Limites et installation

Cette version reste une preview : elle n'offre pas encore tous les fournisseurs
d'Unreal Insights. Sur le Quest testé, des événements Workload, Dispatch, Blit,
Preempt et surfaces ont été reçus ; Binning, Render et les chargements/écritures
séparés n'ont pas été obtenus. Des crashs dans la couche Meta/Adreno ont interrompu
les essais suivants. Le flux détaillé complet dans l'APK reste à valider.
Voir [les résultats Quest](MetaQuestGpu.md).

Extraire l'archive Windows et lancer `CyInsightsApp.exe`. Enregistrer la racine
de son installation Unreal dans Session si nécessaire. Les lecteurs natifs sont
préparés automatiquement avec cette installation et les outils C++ requis ;
aucun script manuel ni rebuild du moteur n'est demandé. Ils sont distribués sous
forme de sources, sans bibliothèques Epic.

Les sources du plugin ciblent UE 4.27–5.8, avec des vérifications et limites
détaillées dans [la matrice de compatibilité](UnrealCompatibility.md). Les modules
de capture restent réservés à Editor, Debug, DebugGame et Development ; Shipping,
Test et Server sont exclus. La matrice ne signifie pas que chaque combinaison
matérielle a été validée.

Les exécutables Windows ne disposent pas d'une signature Authenticode de confiance.
Les archives comportent leurs empreintes et l'état réel des signatures ; cela ne
constitue pas un verdict antivirus. Les licences des composants sont conservées.
