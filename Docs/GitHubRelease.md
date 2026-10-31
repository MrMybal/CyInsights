# Paquet Windows et sources des plugins

Le ZIP Windows contient `CyInsightsApp.exe`, `CyCaptureTool.exe`,
`CyInsightsUpdater.exe`, les notices,
la documentation, les sources des lecteurs natifs et le compagnon IA avec ses
wheels verrouillées. Extraire tout le ZIP avant de lancer l'application. Les
paramètres et captures existants ne sont pas inclus dans ce paquet.

Un Unreal Insights installé séparément est nécessaire pour analyser un `.utrace`.
Dans **Session > Native analyzer**, coller le dossier racine d'un moteur Unreal
puis cliquer **Add engine**, ou utiliser **Browse engine folder**. Le sous-dossier
`Engine` est aussi accepté. L'application vérifie la version et retrouve
`UnrealInsights.exe`, mémorise les moteurs ajoutés et la sélection. La liste peut
contenir plusieurs moteurs ; **Remove selected engine** retire seulement une
entrée, sans supprimer le moteur. Elle ne consulte plus le registre ni l'inventaire
du Launcher et ne recherche pas d'autres installations voisines. Après une mise
à jour depuis la détection automatique, ajouter son moteur une fois manuellement.
Après cet ajout, elle prépare automatiquement les lecteurs de timings,
tâches et activités en arrière-plan, avec progression, journal et annulation.
Une capture ouverte pendant cette étape attend sa fin puis reprend automatiquement.
Il n'est plus nécessaire de lancer un script manuellement.

La préparation exige une installation Unreal licenciée UE 5.5–5.8 et les outils
C++/CMake Visual Studio ; UE 5.8 nécessite le toolset MSVC 14.44. Ces dépendances
ne sont pas installées automatiquement. Si elles manquent ou si la préparation
est annulée, le pont CSV reste disponible. Les lecteurs prêts sont conservés
dans `%LOCALAPPDATA%/CyInsights/NativeReaders`, par installation/version du moteur.
Une mise à jour des sources ou du moteur déclenche une nouvelle préparation.
Dans **Session > Registered Unreal engines**, on peut changer de moteur ou
relancer la préparation. Au démarrage, seule la liste locale enregistrée est
chargée et le moteur sélectionné est réutilisé. Les métadonnées d'une capture
peuvent choisir une version correspondante uniquement dans cette liste.

Pour un diagnostic ou une installation manuelle facultative :

```powershell
powershell -File Tools/InstallNativeReaders.ps1 -Engine "C:/Example/UE_5.8/Engine"
```

Donner le dossier `Engine` de l'installation licenciée correspondante. Le script compile uniquement nos trois
adaptateurs ; il ne recompile pas Unreal et ne copie pas ses DLL. Les sources
sont fournies dans la release ; les exécutables liés aux outils Unreal se
construisent localement selon [le périmètre des licences](Licensing.md).

Le compagnon IA est facultatif. Il nécessite Python 3.11 x64, puis :

```powershell
powershell -File AI/SetupRuntime.ps1
```

L'installation utilise les wheels fournies, sans téléchargement de dépendances.
Le runtime local, ses paramètres et ses journaux ne font pas partie de la release.

Le second ZIP contient les plugins Unreal en sources. Copier `Plugins/CyInsights`
dans le dossier `Plugins` du projet puis le compiler avec l'engine licencié.
Voir [compatibilité Unreal](UnrealCompatibility.md) et les licences des composants.
L'archive des sources correspondant au tag fournit le reste du projet et ses
instructions de compilation, y compris le collecteur Windows facultatif.

## Préparer une release

Définir la version dans `VERSION.json`, puis construire l'application depuis le
commit voulu. Le tag doit correspondre à cette version et aux binaires compilés :

```powershell
$Version = 'v' + (Get-Content VERSION.json -Raw | ConvertFrom-Json).version
python Tools/package_release.py --binaries bin/Release `
  --output build/GitHubRelease --version $Version
```

Le script sélectionne les sources suivies par Git et les trois exécutables de
l'application, vérifie les archives et produit `SHA256SUMS.txt`. Il n'inclut ni
les PDB, traces, paramètres locaux, runtimes virtuels, ni les adaptateurs Unreal
compilés. Les binaires gardent un nom de PDB relatif à la place du chemin du poste
de compilation. Le manifest du ZIP indique le commit et les empreintes des fichiers.
Les binaires de cette preview ne sont pas signés. La préparation automatique
ne rend pas l'analyseur indépendant d'une installation Unreal.

Les mises à jour depuis **Help > Updates** sur Windows et **Updates** sur Quest
acceptent uniquement les versions stables plus récentes. Les releases alpha
restent marquées comme prereleases et ne sont pas proposées. Voir
[la préparation des paquets et le fonctionnement des mises à jour](Updates.md).

## GitHub release descriptions

Write release descriptions in English. Read Markdown explicitly as UTF-8,
for example `Path(notes_path).read_text(encoding="utf-8")`, and send JSON as
UTF-8. Do not rely on the Windows default code page. After updating a release,
read it back through the GitHub API and verify that its body matches the
expected text exactly.
