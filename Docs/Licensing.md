# Licences et périmètre de distribution

Révision du 10 octobre 2026, effectuée à la demande du propriétaire Cyberalien
(MrMybal). Cette adaptation accorde MIT aux composants dont il détient les
droits, dans la copie CyInsights. Elle ne modifie pas le dépôt CyGPUInspector,
ne révoque pas les autorisations AGPL déjà accordées et ne couvre aucun code tiers.

| Composant | Licence de notre code | Texte |
| --- | --- | --- |
| Application `App/`, `Core/`, tests et outils généraux | AGPL-3.0-or-later | `LICENSE` |
| Analyse/session reprises de CyGPUInspector | AGPL-3.0-or-later | `ThirdParty/CyGPUInspector/LICENSE` |
| Plugin `Plugins/CyInsights` (Runtime, Editor, Quest) | MIT | `Plugins/CyInsights/LICENSE` |
| Plugin chargeur `Plugins/CyGPUInspector` | MIT | `Plugins/CyGPUInspector/LICENSE` |
| SDK C, IPC et formats `ThirdParty/CyGPUInspector/CyGPUInspectorCore` | MIT | `CyGPUInspectorCore/LICENSE` |
| Collecteur ReShade `ThirdParty/CyGPUInspector/CyGPUInspectorRS` | MIT | `CyGPUInspectorRS/LICENSE` |
| Adaptateurs facultatifs `Tools/NativeTasks`, `Tools/NativeActivities`, `Tools/NativeTiming` et build du collecteur `Tools/WindowsCollector` | MIT | `LICENSE` dans chaque dossier |
| Dear ImGui, API ReShade et dépendances Python | Licences tierces inchangées | `LICENCES/`, voir `THIRD-PARTY-CyGPUInspector.md` |

La licence racine AGPL s'applique par défaut à notre code sans licence plus
spécifique. Les sous-dossiers MIT sont des composants séparés ; ils n'importent
aucun fichier de l'analyse AGPL ou de l'application dans le jeu. Un lien IPC
échange des données. L'application reste AGPL même si certaines bibliothèques
qu'elle utilise sont MIT. Les notices tierces gardent priorité pour leur code.
Les assets de marque de notre application accompagnent le code AGPL ; aucune
licence de marque ou affiliation Epic, Meta ou ReShade n'est accordée.

## Unreal Engine

MIT couvre nos intégrations, pas Unreal Engine. Les en-têtes, bibliothèques
d'importation, DLL et contenus Epic ne sont pas inclus dans ce dépôt. Il faut
une installation Unreal sous licence pour compiler les plugins et l'adaptateur.
La préparation des dépendances n'importe plus de DLL ni de fichiers Epic.
Le projet Unreal local de test et son contenu sont exclus de Git.

L'[EULA Epic](https://www.unrealengine.com/eula/unreal), sections 5 et 6(c),
régit la redistribution d'Engine Code et les licences incompatibles. La
séparation MIT évite d'imposer le copyleft de notre code à Unreal ; elle ne
remplace pas cette EULA. Les fichiers compilés avec Unreal, notamment le
décodeur `NativeTasks` utilisant `Developer/TraceAnalysis`, doivent être traités
comme potentiellement **Engine Tools** : leur distribution publique reste
soumise aux canaux Epic prévus par les sections 5(a) et 6(d). Ne pas les joindre
automatiquement à une release GitHub ordinaire. Le caractère privé du dépôt
ne dispense pas les destinataires des conditions Epic applicables.

Cette livraison GitHub porte sur les sources de nos composants et leurs
dépendances permissives, sans binaire Unreal. La publication publique éventuelle
des intégrations doit confirmer leur qualification au regard de l'EULA et le
canal adéquat. L'application principale ne lie aucune bibliothèque Epic ; elle
appelle l'outil Unreal Insights installé par l'utilisateur.

## Collecteur Windows et anciennes versions

Recompiler le collecteur avec `Tools/WindowsCollector` produit une version issue
des sources MIT ici présentes. Les anciens `.addon64`, plugins et adaptateurs
déjà compilés ne changent pas de licence par modification de ces fichiers.
Ils sont exclus de Git. Une future release doit être reconstruite et accompagnée
de ses notices et sources correspondantes ; la release de test locale
`Release-20261010.1` reste un artefact historique avec ses notices d'origine.
La DLL ReShade elle-même est un composant séparé, non inclus ici, soumis à ses
propres notices et conditions de distribution.

## Sources correspondantes et mises à jour

`ThirdParty/CyGPUInspector` contient le snapshot réellement utilisé : les
modifications du writer de capture sont conservées. `DEPENDENCIES.lock.json`
enregistre les empreintes actuelles ; `UPSTREAM.lock.json` conserve les
empreintes originales des composants précédemment consommés. Le commit amont
est une référence de provenance, pas une affirmation que sa version Git seule
reproduit le working tree du propriétaire.

Une distribution du programme AGPL doit respecter les obligations de cette
licence, notamment fournir les sources correspondantes et scripts nécessaires.
Pour les packages Python, les roues exactes, leurs empreintes et leurs textes de
licence sont conservés ; `AI/runtime` est généré localement et exclu de Git.
Ne pas réintroduire un composant AGPL/GPL dans le collecteur chargé dans Unreal.
Un contributeur ne doit fournir que du code qu'il est autorisé à placer sous
la licence du composant concerné.
