# CyInsights 0.2.2 Alpha 5

**Windows x64 et Meta Quest arm64 · ALPHA**

## Installer le MCP/SDK complet depuis l’application

Dans Windows, ouvrez **AI Inspector > Connection > Install MCP/SDK**. CyInsights
télécharge Python 3.11 x64 privé depuis une release Astral figée, vérifie son
SHA-256 et installe les dépendances CyToolsCore/MCP déjà fournies avec l’application.
Les paquets s’installent hors ligne avec vérification de leurs empreintes.
Aucun Python global ni droit administrateur n’est requis.

Le panneau propose progression, annulation, nouvelle tentative/réparation et
journal copiable. L’environnement n’est prêt qu’après vérification des imports,
versions des dépendances et du contrat complet de 39 opérations. Activez ensuite
le pont local et copiez la configuration MCP vérifiée dans votre client externe.
Le contrôle de l’application et la capture distante restent des droits séparés,
désactivés au départ.

L’application installée utilise ses données locales ; le portable conserve
l’environnement dans `Data/AI/Runtime` à côté de l’application. Une préparation
annulée ou en échec ne remplace pas l’installation vérifiée précédente. Le ZIP
SDK séparé est destiné aux usages avancés et n’est pas requis en plus de l’app
Windows. Aucun modèle IA n’est téléchargé ni inclus.

## Téléchargements

- **Windows-x64-Setup.exe** : installation utilisateur, raccourcis du menu Démarrer,
  désinstalleur et manuel anglais/français hors ligne complet.
- **Windows-x64-Portable.zip** : extraction puis lancement ; les réglages et
  l’environnement SDK privé restent avec le dossier portable.
- **Windows-x64-Update.zip** : paquet complet utilisé par le système de mise à jour,
  construit depuis les mêmes sources que Setup et Portable. Windows reconnaît
  aussi l’ancien nom de paquet de mise à jour. Ce n’est pas un ancien build.
- **APK et paquet Quest Standard/Vulkan** : recompilés avec la version
  0.2.2-alpha.5 et le code Android 6 ; installez une seule variante.
- **Sources du plugin Unreal et sources/dépendances du SDK IA optionnel.**

Tous les fichiers correspondent à ce tag. Consultez `SHA256SUMS.txt` et
`release-manifest.json` pour les empreintes et le commit source correspondant.

## Limites de l’alpha

Cette version reste une alpha expérimentale. Le système de mise à jour automatique
ne propose toujours que les versions stables ; installez cette alpha manuellement.
Les binaires Windows ne sont pas signés. Le premier téléchargement de Python
nécessite Internet ; les préparations suivantes peuvent réutiliser l’archive
vérifiée en cache. Windows `tar.exe` est requis. Le manuel et les outils d’analyse
natifs restent disponibles hors ligne, sans SDK.

Cette release n’ajoute pas de capacité GPU Quest. Compteurs détaillés et overlays
de tuiles gardent leurs limites documentées de firmware et de cible Development.
Les APK mis à jour n’ont pas été retestés sur casque pour ce changement PC.

[Manuel français](https://mrmybal.github.io/CyInsights/Documentation.html#/fr/ai) ·
[English](Release-0.2.2-alpha.5.md) · [README](../README.fr.md)
