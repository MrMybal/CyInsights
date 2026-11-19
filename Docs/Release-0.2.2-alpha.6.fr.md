# CyInsights 0.2.2 Alpha 6

**Windows x64 · ALPHA**

## Assistant, CLI et MCP natifs

CyMCPSDK 0.7.0 remplace le compagnon Python/CyToolsCore. L’application Windows
intègre le panneau ChatUI officiel et `CyInsightsCli.exe`, sans étape d’installation
d’un environnement. Ouvrez **AI Inspector > Assistant**, détectez les fournisseurs
ou services locaux, choisissez un modèle et utilisez votre authentification existante.

Le registre partagé expose 39 opérations métier et neuf utilitaires de tâches et
ressources. L’assistant, la CLI et MCP appliquent les mêmes droits par document.
Le contrôle de l’application permet de sélectionner et cadrer un intervalle,
mettre une fonction en évidence, ouvrir son histogramme, afficher des panneaux
et épingler des courbes. Il ne donne pas un contrôle arbitraire du bureau ni
un accès automatique à chaque image ou fichier.

Les conversations prennent en charge les réponses progressives, l’annulation,
la restauration et les pièces jointes compatibles. Les tâches durables conservent
leurs résultats après redémarrage sans rejouer les actions. Découverte compacte,
résultats par morceaux, stockage temporaire sur disque, cache et notifications,
mesures privées de métadonnées et fenêtre de contexte API se règlent séparément.
Le chat accepte les outils MCP HTTP distants explicitement autorisés.

Les anciens environnements et données sauvegardées restent conservés. `job_import`
importe les rapports JSON existants ; l’ancien protocole CyTools est remplacé.

## Téléchargements

- **Windows-x64-Setup.exe** : installation utilisateur avec assistant et CLI natifs,
  ainsi que le manuel CyDoc anglais/français hors ligne.
- **Windows-x64-Portable.zip** : extraire toute l’archive ; préférences dans `Data/`.
- **Windows-x64-Update.zip** : paquet complet du système de mise à jour.
- **AI-SDK.zip** : CLI native, contrats, guide et notices pour les clients externes ;
  l’application Windows les contient déjà.
- **Unreal-Plugins-Source.zip** : sources du plugin Unreal optionnel.

Cette mise à jour concerne l’application de bureau. Les APK Quest restent inchangés :
utilisez les [téléchargements Quest Alpha 5](https://github.com/MrMybal/CyInsights/releases/tag/v0.2.2-alpha.5).

## Vérifications et limites

32 suites de tests locales réussies sous Windows x64, avec appels CLI/MCP sur de
vraies instances, modifications autorisées et refus d’accès. Un serveur de test
du protocole API a vérifié le rendu progressif du chat, les outils, l’annulation,
la restauration et les erreurs distantes. L’inférence et l’authentification d’un
vrai fournisseur ainsi que les autres plateformes restent à valider. Les commandes
existantes de création, inspection et extraction de capture ont été testées.

Les binaires Windows ne sont pas signés. Cette alpha expérimentale s’installe
manuellement ; la mise à jour automatique sélectionne toujours les versions stables.

[Guide d’intégration native](AiIntegration.md) ·
[Manuel français](https://mrmybal.github.io/CyInsights/Documentation.html#/fr/ai) ·
[English](Release-0.2.2-alpha.6.md)
