# Windows installer and portable distribution

The NSIS 3.12 compiler is a build-only tool. The installer is Unicode,
English/French, per-user and needs no elevation. It installs the complete offline
application under LocalAppData/Programs/CyInsights by default, creates Start menu
shortcuts and registers its uninstaller in Windows Settings. Desktop shortcuts
and Open-with registration are optional; existing default file handlers are not
replaced. The installer refuses a portable destination and asks that running
product executables be closed before changing files.

The uninstaller removes an explicit list of packaged files, never a recursive
application-folder deletion. User captures, additional files, generated AI
runtimes and LocalAppData/CyInsights settings remain. Running a newer installer
refreshes its packaged-file list and Windows installation metadata.

Prepare the locked, portable compiler from its official download, then package
freshly built product binaries:

```powershell
python Tools/prepare_installer.py
python Tools/package_release.py --binaries bin/Release --output build/packages --version v0.2.2-alpha.5 --nsis build/packaging-tools/nsis-3.12/makensis.exe
```

Use a new output directory for each package build. The output includes the
Windows Setup EXE, an explicitly named Portable ZIP, the explicitly named Update ZIP for the in-app updater,
plugin sources, optional MCP/SDK sources and SHA256SUMS.txt. The installer and
portable ZIP use the same freshly built payload. This command does not publish.

The portable ZIP contains `cyinsights-portable.txt`. Launch the EXE directly;
preferences, layout, native-reader cache, logs and update work live in its `Data/`
directory. Keep the whole folder on a writable drive. Moving the folder preserves
those files; registered Unreal paths and prepared engine readers remain specific
to the host engine installation. The browser/OS can still maintain their own
temporary files and documentation reading preferences. The optional Python
companion also uses Data/AI when launched from this edition.

The compatibility update ZIP has no portable marker. The updater preserves
unlisted files, so it retains an existing portable marker and Data folder. The
installed edition uses LocalAppData/CyInsights, allowing ordinary user updates
without administrator privileges. Alpha releases remain excluded by the updater.

NSIS is used with zlib compression. Its complete upstream notice is retained in
LICENCES/NSIS.txt. No compiler or third-party installer framework is installed
on the user's machine. Authenticode signing remains a separate release step;
without a trusted certificate, the generated installer and binaries are unsigned.
