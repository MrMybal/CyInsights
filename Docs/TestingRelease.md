# Release de test — 11 octobre 2026

L'application actuelle est dans [bin/Release-20261011/CyInsightsApp.exe](../bin/Release-20261011/CyInsightsApp.exe). Elle ajoute l'enregistrement manuel des moteurs et [Frame stats](FrameStats.md), avec snapshots, courbes et tableau par frame dans la même trace. Elle intègre le [logo bleu cyan](Branding.md) et conserve [AI Inspector et 17 outils CyToolsCore/MCP](AiIntegration.md), avec analyses compactes, observations dans la timeline et commandes de capture réseau. [Filtrage exact et histogrammes](TimelineFunctionAnalysis.md), [Distributions et Tasks](TraceAnalysis.md) restent disponibles. Les anciennes alertes antivirus ne sont pas déclarées résolues par cette mise à jour.

## Ouvrir une capture

Le collecteur Windows détaillé est désormais interne au seul plugin CyInsights,
désactivé par défaut. Voir [Windows GPU collector](WindowsGpuCollector.md) pour
l'activer explicitement. Aucune installation de CyGPUInspector n'est nécessaire.

1. Lancer le nouvel exécutable et cliquer sur **Open capture**.
2. Sélectionner une `.utrace` ou une `.cycapture`. Le panneau de chargement affiche la phase, le temps écoulé, la progression de l'indexation et **Cancel**. Une erreur éventuelle reste visible avec son journal.
3. Dans la version avec enregistrement manuel, ouvrir **Session**, saisir le dossier racine du moteur Unreal dans **Engine root**, puis cliquer **Add engine** ou utiliser **Browse engine folder**. La sélection et la préparation sont mémorisées. Revenir sur **Timing Insights** et lancer **Analysis > Analyze native timing** si nécessaire. L'analyseur Epic reste une dépendance locale et n'est pas redistribué avec cette application.
4. Zoomer avec la molette. Sur l'aperçu supérieur ou la règle, maintenir le bouton gauche : gauche/droite déplace le temps, haut/bas zoome. Shift + glissement gauche sur la règle sélectionne l'intervalle ; `F` le cadre. Le bouton du milieu déplace aussi l'axe des pistes. Les scopes traversent les frames sans être découpés par frame.

**Double-cliquer un scope** filtre tous les scopes portant le même nom exact et atténue les autres. **Clic droit > Histogram of this function** ouvre sa distribution sous Timing. **Shift + glissement sur la règle** choisit une plage ; avec **Follow timeline selection**, les analyses ouvertes se mettent à jour après le relâchement. **Whole capture** désactive ce suivi ; **Use selected interval** le réactive.

Ouvrir **Distributions** pour choisir le thread, les coûts avec/sans attentes ou les durées de scopes, puis **Analyze**. Un clic sur une tranche parcourt ses valeurs ; un double-clic revient sur la timeline. Ouvrir **Tasks** pour les cycles de vie natifs et délais disponibles. Une trace sans canal `task` montre uniquement des scopes CPU candidats ; les nouvelles captures du plugin incluent ce canal par défaut. Les adaptateurs `NativeTasks/5.x` utilisent l'installation Unreal choisie dans Session.

**Timing > View Mode > Target fps** règle le budget des couleurs. Survoler une frame pour les coûts avec/sans attentes reconnues ; **Next slow frame** cherche un dépassement. La couleur indique une cause probable, avec association Render/GPU par recouvrement temporel.

Les plafonds de CSV et de nombre d'événements sont supprimés. Tous les timings sont conservés dans un index sur disque ; la fenêtre visible se charge en arrière-plan. Les compteurs se chargent à la demande avec **Load counter samples**. Le pont Epic produit encore un CSV intermédiaire volumineux ; voir [chargement des grandes traces](LargeTraceLoading.md).

Les vues présentes couvrent les timings, compteurs, relations d'appel dérivées, distributions, tâches et comparaison A/B. Logs, mémoire, réseau, chargement et autres providers Insights ne sont pas encore tous implémentés. Une trace sans données GPU ne peut pas afficher de timings GPU mesurés.

## Vérifications de la révision logo 10.1

- 483 checks réussis dans 13 suites ; ressources de l'exécutable vérifiées, neuf tailles d'icône et extraction native Windows.
- PNG embarqué identique à l'asset d'interface ; logo master approuvé conservé sans modification.
- Session et menu réellement rendus ; ouverture de la trace de 14 192 325 événements et navigation vérifiées, source inchangée.
- Scan Kaspersky du dossier terminé le 10 octobre à 00:07:52 : 5551 objets traités, zéro détection, suspicion, fichier ignoré ou erreur.
- Compagnon CyToolsCore privé et lecteurs UE 5.5–5.8 conservés. Les validations MCP complètes du 9 octobre restent une référence datée de la révision IA.

![Logo dans la fenêtre Session](../build/release-20261010.1-audit/ui-session.png)

## Vérifications de la révision IA 9.6

- 483 vérifications natives réussies sur 13 suites et cinq tests Python du compagnon.
- MCP officiel testé avec le compagnon effectivement livré : permissions, ouverture des 14 192 325 événements, histogrammes compacts, diagnostics, sélection/filtre, annulation, génération périmée et tâches candidates. Source inchangée.
- Commandes réseau testées avec un endpoint synthétique local : démarrage, réception, arrêt et assemblage dans une archive unique. Ceci n'est pas une nouvelle capture matérielle Quest.
- Scan Kaspersky du dossier livré terminé le 9 octobre à 22:00:53 : 7727 objets traités, zéro détection, suspicion, fichier ignoré ou erreur.
- Analyse réactive des plages chargées ; le décodage pendant un enregistrement reste indisponible. Voir [la couverture des outils](CapabilityCoverage.md).

- Rendu réel d'AI Inspector sous Timing ; les tests de glissement/zoom passent pendant 128 rechargements, avec Frames alignées et hauteurs dynamiques. Les cinq interactions filtre/histogramme/sélection passent également.

![Fenêtre AI Inspector réellement rendue sous la timeline](../build/release-20261009.6-audit/ui-ai.png)

## Références et vérifications de la révision précédente 9.5

- Reconstruction depuis un nouveau dossier de build, avec compilation complète des sources et empreintes des entrées conservées.
- 452 vérifications réussies sur douze suites, aucune en échec.
- Scan Kaspersky terminé : zéro détection, suspicion, fichier ignoré ou erreur sur le nouvel exécutable.
- Ouverture de `20261008_142920.utrace` (49 648 645 octets), conservation de ses 14 192 325 événements, rendu et fermeture normale. Empreinte de la trace inchangée.
- Interactions réelles : double-clic sans zoom, histogramme depuis le clic droit, filtre temporel automatique et pagination conservant les statistiques.
- Requêtes de fonctions mesurées sur 14,2 millions d'événements : Tick 414,8 → 4,28 ms ; ExecuteForegroundTask 487,7 → 60,35 ms ; MobileSceneRender 658,0 → 87,26 ms. Le premier export CSV reste coûteux.
- Distribution réelle de 6009 intervalles GameThread et inspection de 1 223 247 scopes CPU candidats dans cette trace sans TaskTrace. Histogramme logarithmique et percentiles rendus dans l'application.
- Capture UE 5.6 enregistrée avec `task`, terminée normalement : validation des lecteurs inchangés en 9.4, puis nouvelle inspection dans cette application ; les lecteurs 5.5–5.8 donnent les mêmes 64 406 événements, 12 716 tâches et 84 liens. Les 12 716 tâches sont accessibles dans la vue globale ; une sélection affiche son cycle de vie et ses délais. Trace inchangée.
- Glissements prolongés pendant plusieurs rechargements, sortie de la règle, déplacement sur les scopes, zoom vertical et molette. Frames alignées sur le même axe ; hauteur des pistes adaptée aux scopes visibles. Voir [les corrections de navigation](NavigationFixes.md).
- Validation de la release précédente (8 octobre), conservée comme référence du stockage : ouverture d'une seconde trace de 1,4 Go : 353 528 278 événements conservés, cache de 11,3 Go, rendu et navigation réussis, empreinte source inchangée. La première analyse a pris environ 17 min 30 s ; le pont CSV reste coûteux.

![Interface réelle : fonction filtrée et histogramme de la sélection](../build/release-20261009.5-audit/ui-interactions.png)

Le nouveau fichier n'est pas signé. Le scan effectué est un résultat ponctuel, pas une certification de sécurité et pas une confirmation de faux positif pour l'ancien exécutable. Les fenêtres détachables, changements de DPI et captures GPU matérielles n'ont pas été retestés sur cette release.

Les licences sont conservées dans le dossier de release ; les sources correspondantes se trouvent dans ce projet et ses dépendances stagées. Le [rapport de sécurité](SecurityReview.md) et le [manifest de release](../bin/Release-20261010.1/release-manifest.json) donnent les empreintes et preuves de vérification.
