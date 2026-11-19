# Notes de version de CyUI

[English](VERSION.md) · [README](README.fr.md) · [Publications](https://github.com/MrMybal/CyUI/releases)

## Versions et statut

| Composant | Version | Disponibilité |
| --- | --- | --- |
| Bibliothèque C++ et paquet Python | 1.2.0 | Version actuelle des sources dans CMake et `pyproject.toml`. |
| Démo autonome de la galerie | 1.2.0 | Build portable Windows x64 / DirectX 11 publié. |
| Lecteur CyDoc embarqué | 0.1.0 | Dépendance de documentation des builds actuels, avec les modules image et gallery. |

**CyUI est en développement actif.** Le projet ne déclare pas de statut produit
Alpha, Bêta ou Stable. Le numéro de version de la bibliothèque et le marquage de
la publication de la démo n’établissent ni une API stable ni une validation des plateformes.

## Démo 1.2.0 publiée

La [publication v1.2.0](https://github.com/MrMybal/CyUI/releases/tag/v1.2.0) fournit
la galerie Windows et sa somme de contrôle SHA-256. Son contenu documenté inclut :

- Widgets natifs, dispositions, cinq thèmes et sélection anglais/français à chaud.
- Vues audio, graphiques, courbes éditables et chronologies.
- Graphes à nœuds avec aperçus synthétiques, navigateur de contenu et inspecteur de propriétés.
- Édition de code et modes syntaxiques, vues Diff, commandes et filtres de menus/panneaux.
- Licences des dépendances et informations sur la révision des sources.

[Téléchargez le ZIP Windows x64](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip)
et [sa somme de contrôle](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip.sha256).
Extrayez toute l’archive avant de lancer l’exécutable.

## Sources actuelles après la démo publiée

La version des sources reste 1.2.0. Les sources plus récentes incluent le manuel
CyDoc anglais/français complet, les références indexées des en-têtes publics et
les points d’entrée de l’aide locale. Les builds actuels de la démo, les wheels
Python et les paquets Unreal incluent le lecteur et ses ressources. La vitrine
statique actualisée l’inclut également.

Ces ajouts des sources ne font pas partie de l’ancien ZIP 1.2.0 publié.
Compilez les sources actuelles pour utiliser cette intégration de l’aide ;
la lecture du manuel exporté ne nécessite ni outils de compilation, ni serveur,
ni connexion.

## Limites documentées

- **Plateformes :** l’exécutable publié est uniquement pour Windows x64. Les
  sources de la bibliothèque et la configuration CI existent pour Linux/macOS ;
  les vérifications locales Windows ne valident pas l’exécution sur ces systèmes.
  Le déploiement CEF nécessite une validation par cible.
- **Responsabilités de l’hôte :** graphes et vues affichent les données fournies
  par l’application. Décodage audio, FFT, DSP, exécution des shaders, opérations
  sur les fichiers, accès aux dépôts et services de langage restent dans l’application.
- **Éditeur de code natif :** CodeEditor demande C++20. Les modes syntaxiques
  fournissent une coloration lexicale, sans compilateur, débogueur ni serveur
  de langage. L’éditeur Python utilise une saisie multiligne avec aperçu
  syntaxique et n’expose pas tout le backend TextEditor natif.
- **Intégrations optionnelles :** la démo ordinaire n’embarque aucun runtime CEF
  ou Unreal. Le connecteur Unreal vise Editor C++/Slate, pas runtime/UMG.
  SDK externes, polices et notices des dépendances restent à la charge de l’hôte.
- **Documentation :** conservez tout le dossier `Documentation/`. Le lecteur
  utilise JavaScript ; préférences et annotations personnelles dépendent des
  autorisations de stockage du navigateur. Références externes et téléchargements
  nécessitent une connexion.

Consultez le [guide des plateformes](Documentation/Documentation.html#/fr/overview/platforms),
le [guide Web](Documentation/Documentation.html#/fr/web) et le
[guide Unreal](Documentation/Documentation.html#/fr/unreal) pour le périmètre documenté.

## Problèmes et retours

Les notes de publication 1.2.0 ne fournissent pas de liste détaillée des bugs.
Consultez le [suivi des problèmes](https://github.com/MrMybal/CyUI/issues) pour
les signalements et échanges.
