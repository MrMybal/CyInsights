# Choisir les graphes et textes du HUD {#title}

Ouvrir Display & HUD > Graphs & HUD.

| Colonne | Destination |
| --- | --- |
| HUD graph | Courbe dans le jeu |
| HUD value | Texte numérique dans le jeu |
| Overview | Courbe dans la configuration seulement |

Choisir graphe seul, valeur seule ou les deux. Add draw calls + triangles ajoute les deux pour les compteurs principaux par seconde sans effacer les autres choix ; ce ne sont pas seulement les variantes par frame. All HUD graphs choisit les courbes listées ; Clear HUD efface les choix. Recherche et filtres GPU/CPU/Memory/Device réduisent le tableau.

Start HUD applique et garde la configuration ouverte. Les cases modifiées pendant le HUD sont enregistrées/appliquées après un court délai ; Apply HUD réessaie explicitement. Stop HUD est distinct de la heatmap. Neuf positions incluent Top center, Center et Bottom center. Taille/position des statistiques et opacité/position des tuiles sont indépendantes.

Les valeurs natives Meta utilisent son HUD. Les métriques supplémentaires passent par l'interface publique personnalisée depuis la couche Vulkan du jeu Development choisi : Meta les associe à la vraie application VR. Employer l'APK Vulkan, la cible exacte et une réouverture complète après préparation/mise à jour. Définitions/réglages acceptés ne prouvent pas la visibilité ; cette nouvelle voie commune de l'alpha attend sa validation visuelle finale et signale les erreurs.

[N/A] signifie absent/périmé/inutilisable, pas zéro. [partial] signifie total connu incomplet. Voir [Compteurs](doc:quest-counters) et [Dépannage](doc:troubleshooting).
