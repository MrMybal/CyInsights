# Naviguer et mesurer dans la timeline {#title}

## Naviguer {#navigate}
- Glisser droit déplace le temps horizontalement et les pistes verticalement.
- La molette zoome autour du pointeur. Fit cadre la capture ; Frame parcourt les frames ; F cadre la sélection.
- Replier les pistes inutiles. Le GPU est au-dessus de GameThread. La hauteur suit l'imbrication visible au lieu de réserver du vide.

La règle supérieure sert à naviguer. La règle fine supplémentaire dessous mesure : glisser gauche de A à B choisit bornes et durée. Un glissement inversé produit aussi un intervalle ordonné. Frames et timings partagent la vue.

## Sélectionner et filtrer {#select-and-filter}
Un clic sur un scope sélectionne l'événement. Le double clic filtre tous les éléments de ce nom exact. Le double clic sur fond vide retire ce filtre. Le clic droit ouvre histogramme/filtre exact ; un glissement droit reste un déplacement.

Distributions, statistiques de fonctions, frames, compteurs et activités compatibles utilisent l'intervalle sélectionné ou indiqué dans le panneau. Vérifier les bornes avant comparaison. Fit selection change la vue, pas les timestamps. Recherche et filtrage exact sont distincts. Aucune sélection ne crée les données absentes.

Voir [Fonctions](doc:functions), [Frames](doc:frames) et [Commandes](doc:shortcuts).
