# Interface native CyUI

L'integration modulaire actuelle, la revision figee, les vues migrees et les
limites des adaptateurs sont detaillees dans [CyUIMigration](CyUIMigration.md).

CyInsights utilise CyUI pour les thèmes, polices, contrôles et conteneurs Dear
ImGui. La bibliothèque est incluse dans `ThirdParty/CyUI` sous licence MIT ;
CMake la lie à l'unique cible ImGui existante. Le dépôt peut être compilé sans
checkout externe de CyUI. Les polices Windows sont chargées sur la machine et
ne sont pas redistribuées.

- **Session & capture** : ouvrir plusieurs fichiers, enregistrer depuis un
  appareil, ajouter une installation Unreal et consulter les sources reçues.
  Les détails de capture et le collecteur Windows sont repliables. L'écran
  passe à une colonne lorsque la largeur disponible devient insuffisante.
- **Timing Insights** : overview des frames, timeline CPU/GPU, règle de
  navigation et règle de mesure conservent leurs interactions. Les commandes
  de pistes et d'analyse sont regroupées avec des infobulles ; le menu View
  donne aussi accès à tous les panneaux.
- **Appearance** : cinq themes, densites Compact et Comfortable, langue et
  editeur de palette filtre avec validation, annulation/reprise et import/export
  `.cytheme`. La galerie de demonstration n'est plus compilee. Les preferences
  de theme, densite et langue sont enregistrees localement.
- Les couleurs de scopes identifient des catégories. Les fonds de causes
  restent conditionnés à l'analyse des dépassements de budget ; un thème
  ne transforme pas une donnée déduite ou indisponible en mesure.

La barre d'état rappelle les interactions : glisser droit pour déplacer la
timeline, molette pour zoomer, glisser dans la règle inférieure pour mesurer,
F pour cadrer la sélection. Le double clic sur un scope filtre son nom ; le
double clic sur une zone vide retire ce filtre. Les captures ouvertes restent
indépendantes et peuvent être détachées via leur onglet ou le bouton dédié.

Pour vérifier le rendu du véritable exécutable sans interaction manuelle :

```text
CyInsightsApp --snapshot <image.bmp> --ui-theme light-grey <capture.utrace>
CyInsightsApp --snapshot <image.bmp> --ui-theme navy --ui-comfortable --capture-options-view
CyInsightsApp --snapshot <image.bmp> --ui-size 1280x800 --session-view
```

Les options de snapshot, de thème et de taille ne démarrent aucune capture
distante. Les contrôles `--navigation-smoke`, `--interaction-smoke` et
`--multi-trace-smoke` continuent à exercer la navigation, les histogrammes et
les fenêtres natives sur des fichiers locaux.
