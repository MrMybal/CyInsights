# Collecteur Windows interne et facultatif

CyInsights lit les pistes CPU/GPU d'une `.utrace` sans le collecteur supplémentaire.
Le plugin Unreal CyInsights coordonne les captures ; il reste facultatif pour ouvrir
et analyser les fichiers existants dans l'application.

Le code de collecte repris de CyGPUInspector est intégré aux modules Windows
`CyInsightsGpuLoader` (avant le RHI) et `CyInsightsGpuWindows` (captures et marqueurs
de viewport) du **seul plugin CyInsights**. Le backend ReShade produit
`CyInsightsGpu.addon64`. L'application CyInsights contient déjà le receiver et
l'analyse des commandes ; aucun plugin ou processus CyGPUInspector n'est utilisé.
Les noms de session, pipes et mappings sont propres à CyInsights, et le backend
ne présente pas l'overlay de l'autre application. Les noms d'origine dans les sources
et licences conservent la provenance du code réutilisé.

Le chargement supplémentaire est désactivé par défaut. Dans les paramètres du projet,
« CyInsights - Windows GPU collector », choisir le chargement Editor et/ou Game,
indiquer le runtime ReShade compatible add-ons, puis redémarrer. Le backend est installé
dans `Plugins/CyInsights/Binaries/ThirdParty/CyInsightsGpu/Win64`. Seuls Editor,
Debug, DebugGame et Development Win64 le chargent ; Android, Shipping, Test,
Server et commandlets restent exclus. Un backend compilé est installé localement
par `Tools/WindowsCollector` ; les DLL Unreal ne sont pas redistribuées sur GitHub.

Dans Session > Windows collector de l'application CyInsights, connecter le processus
local puis demander une capture détaillée. Les options de bindings, barrières,
timestamps par commande et readbacks sont explicites. Textures/buffers restent
désactivés par défaut, y compris pour le bouton local de capture. Les timestamps
reçus après plusieurs frames restent associés à leur frame d'exécution.

La capture native fonctionne sans ces modules activés. L'option ajoutée ne fournit
pas une nouvelle calibration CPU/GPU matérielle : les associations approximatives
restent signalées. Les payloads détaillés ne sont pas encore transportés par le flux
Unreal réseau ; une demande distante nécessite un receiver local déjà connecté.
Cette intégration Windows ne remplace pas les groupes GPU natifs Quest.

## Validation du 11 octobre 2026

Les modules Runtime/Editor, le chargeur et les marqueurs compilent sous UE 5.5.4
et 5.8.3 Win64. Le plugin Android ARM64 UE 5.5.4 compile avec les modules Windows
exclus. Le backend ReShade a été compilé à partir des sources de ce dépôt ; ses
exports et noms IPC propres à CyInsights ont été vérifiés. Dix-huit suites de
composants passent, plus les vérifications des descripteurs et du packaging.

Un hôte Unreal 5.8 rendu avec le backend optionnel activé a accepté une demande
GPU via le protocole réseau CyInsights. Son receiver local a enregistré **345
commandes, 235 draws et 33 dispatches**, avec **4,793 ms** de timestamps GPU pour
la frame d'exécution **70** ; les résultats reçus restent associés à cette même
frame. Aucun readback de texture/buffer n'a été demandé ou reçu. Le flux natif
et les fichiers GPU ont été assemblés dans une `.cycapture` vérifiée par SHA-256.
Il s'agit d'un test Windows précis, pas d'une validation du collecteur ReShade sur
Quest ou de toutes les versions/APIs graphiques.
