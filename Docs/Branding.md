# Logo de CyInsights

Le logo bleu cyan approuvé est intégré dans l'icône de l'exécutable Windows, les icônes de fenêtre/barre des tâches, le menu principal et l'en-tête **Session**. Le Cy blanc reste au centre ; la puce et les barres de timing évoquent le profiling CPU/GPU.

Les ressources sont embarquées dans l'exécutable, sans lecture d'une image externe au lancement. Windows choisit parmi neuf tailles d'icône (16, 20, 24, 32, 40, 48, 64, 128, 256 px). L'interface décode son PNG avec Windows Imaging Component et le charge une fois dans une texture D3D11 ; la texture est libérée après la fermeture des viewports ImGui. Les tailles d'affichage suivent la mise à l'échelle de l'interface.

Le master approuvé, transparent et inchangé, se trouve dans `Assets/Branding/cyinsights-logo-v1.png`. `Tools/prepare_branding.py` dérive le PNG d'interface de 512 px et l'ICO ; Pillow est nécessaire uniquement à la régénération des assets. Le build et l'application utilisent les ressources prêtes, sans Pillow ni Python pour le logo. `branding-assets.json` conserve leurs empreintes. Le prompt de création est préservé à côté du master.

La release `bin/Release-20261010.1` conserve le compagnon AI Inspector/MCP et les lecteurs UE 5.5–5.8 de la version précédente. Les contrôles de ressources portent sur l'exécutable compilé, incluant les neuf images d'icône, la correspondance exacte du PNG et l'extraction d'icône par Windows. Les preuves de compilation, scan et rendu sont dans `build/release-20261010.1-audit` et son manifest de release.
