# Navigation et pistes — 9 octobre 2026

La release `Release-20261009` corrige les interruptions du glissement pendant les lectures sur disque, le graphique de frames figé sur toute la capture et la hauteur excessive des pistes.

Le geste conserve ses coordonnées et son intervalle de départ jusqu'au relâchement effectif du bouton. Il continue quand la souris quitte la règle ou quand un nouveau modèle est publié. Les lectures en cours se terminent ; la dernière fenêtre demandée est ensuite chargée. Des scopes voisins sont conservés pour afficher le déplacement pendant cette lecture. L'indication de mise à jour reste dans la barre d'outils, sans fenêtre susceptible de prendre le focus. Le chargement initial garde sa progression et son bouton d'annulation.

Les barres de Frames utilisent le même début, la même fin et la même largeur temporelle que les scopes. Leurs frontières suivent donc les guides et les frames de GameThread. Les noms restent des indices natifs quand l'export les fournit ; sinon ils sont explicitement des candidats dérivés des ticks.

Chaque piste occupe les niveaux nécessaires aux scopes de l'intervalle visible. Les profondeurs enregistrées ailleurs restent conservées dans les données et reviennent lorsqu'on navigue vers elles. Une piste inactive garde seulement son en-tête. Le tri des pistes conserve l'ordre de la capture lorsque les fenêtres se remplacent.

Les huit suites isolées passent 256 vérifications. Elles couvrent notamment le changement de hauteur selon l'intervalle, le maintien des profondeurs, les barres de frames traversant une borne et leur regroupement à faible résolution. Le test d'interface sur `20261008_142920.utrace` maintient le bouton pendant plusieurs secondes et plusieurs publications de modèles, puis teste la règle en sortant du widget, le déplacement sur les scopes et la molette. Il vérifie l'alignement temporel et en pixels, la continuité du geste et la réduction des lignes réservées.

Preuves : [résultat d'ouverture](../build/release-20261009-audit/opening.json), [rendu réel](../build/release-20261009-audit/ui-navigation.png), [tests](../build/release-20261009-audit/build.log), [scan Kaspersky](../build/release-20261009-audit/kaspersky-scan.txt). Les sources et licences sont conservées ; aucun moteur ou plugin Unreal n'est modifié par cette correction de l'application.
