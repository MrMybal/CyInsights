# Lire les données GPU {#title}

Commencer par les groupes GPU natifs mesurés. Une seule barre MobileSceneRender signifie que seul ce niveau existe. Un autre panneau ne recrée pas les timestamps enfants absents.

## Distinguer les sources {#distinguish-sources}
- Scopes GPU Unreal : mesurés quand RHI/build fournit timestamps et instrumentation.
- Contenus de passes/subpasses Vulkan : arguments, pièces jointes et structure. Les commandes côté CPU ne sont pas des coûts matériels par subpass.
- Collecteur Windows optionnel : commandes, pipelines, shaders, ressources et timestamps disponibles. Activation explicite ; la lecture .utrace normale n'exige pas CyGPUInspector séparé.
- Stages/tuiles Meta : [source Quest](doc:quest-capture) distincte.

GPU frame peut montrer passes, nombres draw/dispatch et dépendances reconstruites. Passes/liens déduits sont étiquetés. Les timestamps de commandes sont optionnels et parfois locaux à la frame. Décrire une ressource/shader ne mesure pas sa durée.

Un Render élevé seul ne prouve pas un goulot pixel shader : il peut inclure vertex et fragment. Croiser parts, attentes texture/mémoire, dimensions, tuiles et changements contrôlés. Binning est une enveloppe, pas un supplément indépendant à ajouter aux enfants. Attentes et recouvrements comptent.

Les lectures d'images/textures/buffers sont désactivées initialement. Le détail peut perturber le GPU : garder le diagnostic court puis retester en [profil léger](doc:profiles). Voir [Glossaire](doc:glossary).
