# Paquet Windows et sources des plugins

Le ZIP Windows contient `CyInsightsApp.exe`, `CyCaptureTool.exe`, les notices,
la documentation, les sources des lecteurs natifs et le compagnon IA avec ses
wheels verrouillées. Extraire tout le ZIP avant de lancer l'application. Les
paramètres et captures existants ne sont pas inclus dans ce paquet.

Un Unreal Insights installé séparément est nécessaire pour analyser un `.utrace`.
Le sélectionner dans **Session > Select UnrealInsights.exe**. Le pont CSV fonctionne
sans compiler les lecteurs. Pour activer la lecture directe et les lecteurs de
tâches/activités, avec les outils C++ Visual Studio installés :

```powershell
powershell -File Tools/InstallNativeReaders.ps1 -Engine "C:/Example/UE_5.8/Engine"
```

Donner le dossier `Engine` de l'installation licenciée correspondante, UE 5.5–5.8.
UE 5.8 nécessite le toolset MSVC 14.44. Le script compile uniquement nos trois
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

Construire l'application depuis le commit voulu, puis :

```powershell
python Tools/package_release.py --binaries bin/Release `
  --output build/GitHubRelease --version v0.1.0-preview.20261010
```

Le script sélectionne les sources suivies par Git et les deux exécutables de
l'application, vérifie les archives et produit `SHA256SUMS.txt`. Il n'inclut ni
les PDB, traces, paramètres locaux, runtimes virtuels, ni les adaptateurs Unreal
compilés. Les binaires gardent un nom de PDB relatif à la place du chemin du poste
de compilation. Le manifest du ZIP indique le commit et les empreintes des fichiers.
Les binaires de cette preview ne sont pas signés.
