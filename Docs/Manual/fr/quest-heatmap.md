# Tuiles GPU et surimpression heatmap {#title}

GPU heatmap montre un snapshot. Heatmap overlay contrôle une surimpression de développement en direct, distincte des statistiques. Il faut APK Vulkan, cible débogable choisie, couche externe préparée, données Meta détaillées compatibles et réouverture du jeu après préparation.

1. Vérifier cible/disponibilité dans Heatmap overlay.
2. Préparer couche/profiling avant le nouveau contexte GPU ; rouvrir le jeu manuellement une fois.
3. Choisir Extra HUD ou Fullscreen (approximate), position, plafond de coût et opacité.
4. Start démarre ; Apply ajuste. Stop masque sans retirer la préparation. Uninstall target integration la retire et impose préparation/réouverture la fois suivante.

La couleur suit coût échantillonné/échelle ; faible opacité préserve l'image. L'alignement plein écran reste approximatif. Des dimensions de surface n'identifient ni œil, frame ni projection. La grille du manuel est indiquée comme exemple, sans données live.

Binning est une enveloppe. Render inclut vertex et fragment. LoadColor/LoadDepthStencil, StoreColor/StoreDepthStencil décrivent les transferts quand disponibles. Une tuile chaude localise un travail suspect, pas un mesh/shader unique. Ne pas additionner les enveloppes qui se recouvrent.

Aucun overlay personnalisé universel n'est validé pour tous les APK commerciaux non modifiés. Source absente/périmée/mauvaise cible : indisponible, pas simulée. Voir [Enregistrement](doc:tile-recording) et [GPU](doc:gpu-desktop).
