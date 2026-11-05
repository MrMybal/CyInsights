# Réseau, stockage, chargement et mémoire {#title}

Ouvrir Network / storage / memory (Activities) et charger le fournisseur utile. Les lecteurs d'activités sont des adaptateurs optionnels distincts. Une trace de timings ne garantit pas ces canaux. Filtrer nature/piste et choisir un court intervalle.

- Réseau : paquets/contenus enregistrés nécessitent NetTrace et son instrumentation. Les bits de contenu ne sont pas automatiquement tous les octets sur le réseau, tout le trafic socket ou une capture externe complète.
- Stockage : opérations de fichiers Unreal, pas toutes les opérations disque de tous les processus.
- Chargement : événements de ressources du moteur quand présents, distincts des fichiers bruts.
- Mémoire : jauges/tags disponibles. LLM exige son instrumentation ; les allocations détaillées, les flags au lancement. L'exploration complète des piles mémoire n'est pas terminée ici.

Commencer par les résumés et détailler une piste suspecte. Comparer les recouvrements CPU/GPU en gardant horloges/disponibilité. Un paquet près d'un à-coup est une corrélation, pas une preuve.

Activer les sources avant enregistrement dans [Profils](doc:profiles). Ouvrir le panneau ou une requête IA n'instrumente pas une ancienne trace. Messages contient les diagnostics du logiciel ; le lecteur natif complet des logs Unreal n'est pas fourni.
