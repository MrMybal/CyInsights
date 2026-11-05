# Capturer Unreal et le GPU Meta ensemble {#title}

Groupes GPU natifs Unreal et profiling matériel Meta se complètent. GPU Profiling, GPU Metrics et GPU Render Stage Trace sont distincts ; activer Meta n'ajoute pas de scopes Unreal à un ancien .utrace.

Préparer le détail avant la création du contexte GPU cible. Dans Trace Analysis by Perfetto de MQDH, choisir métriques/stages voulus ; Render Pass Diagnosis demande les stages détaillés. Enregistrer. Un jeu déjà lancé peut nécessiter une fermeture/réouverture manuelle complète. Ce mode ajoute du surcoût : garder les diagnostics courts.

## Enregistrer dans CyInsights {#record-in-cyinsights}
1. Connecter le plugin et vérifier sa réponse.
2. Activer Meta dans Session / Capture options ; outils Android autorisés et chemin appareil pris en charge doivent exister.
3. Garder la scène visible et le casque éveillé. La capture normale ne relance pas le jeu ; une relance est explicite.
4. Démarrer/arrêter la trace. Le logiciel finalise un .cycapture avec timings, stages, compteurs et métadonnées reçus, puis peut l'ouvrir automatiquement.
5. Vérifier types reçus, précision et alignement des horloges, pas seulement l'acceptation du départ.

Une source grossière reste grossière. Binning, Render, LoadColor/LoadDepthStencil, StoreColor/StoreDepthStencil n'apparaissent que reçus. Un stage seul n'identifie ni shader, mesh, œil, ni frame exacte. Voir [Heatmaps](doc:quest-heatmap) et [Dépannage](doc:troubleshooting).
