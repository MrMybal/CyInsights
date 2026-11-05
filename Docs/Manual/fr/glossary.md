# Unités, preuves et glossaire {#title}

| Terme | Sens |
| --- | --- |
| Mesuré | Observation source avec limites de couverture/horloge |
| Déduit | Calcul/reconstruction, pas une autre mesure matérielle |
| Indisponible / N/A | Absent, non pris en charge, périmé ou inutilisable ; pas zéro |
| Partiel / connu | Seulement la portion instrumentée connue |
| Inclusif / exclusif | Avec enfants / sans leurs intervalles connus |
| Instant d'observation | Réception/observation, pas forcément exécution |
| Identité de frame | Attributions CPU, rendu et GPU peuvent différer |
| ms / us | 1 milliseconde = 1000 microsecondes |
| Hz / FPS | Mises à jour / frames par seconde ; polling n'est pas mesure |
| Draw call | Commande de dessin, pas nombre de meshes/objets visibles |
| Triangles d'entrée | Géométrie avant culling et amplification |
| Render pass / subpass | Travail Vulkan sur pièces jointes / étape interne |
| Tuile GPU | Région de surface avec coûts de stages échantillonnés |
| Binning / Render | Enveloppe / rendu pouvant inclure vertex et fragment |
| .utrace / .cycapture / .cyovr | Trace Unreal / conteneur combiné / snapshot tuiles |

Trois subpasses sont trois étapes internes, pas trois barres GPU chronométrées. Contenus/arguments ne donnent ni coût matériel par étape ni début de rasterisation. Tuile chaude et image situent une zone, pas un mesh/shader unique ou œil exact.

Frame neutre, histogramme vide et compteur absent peuvent signifier preuves insuffisantes. Garder unités, bornes, source, disponibilité et incertitudes. Retour à [Commencer ici](doc:welcome) ou [Dépannage](doc:troubleshooting).
