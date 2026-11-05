# Compteurs et statistiques par frame {#title}

Counters et Frame stats exposent des sources distinctes. Déclarer un compteur prouve nom/type, pas la capture d'échantillons. Charger les valeurs à la demande, filtrer par recherche/disponibilité puis examiner ou épingler les séries utiles.

Les snapshots légers du plugin comprennent observations GameThread/RenderThread/RHI/GPU, intervalle et slots/capacité UObject. L'agrégation Stats optionnelle collecte les groupes actifs configurés, par exemple Game, UObjects et GPU. L'activer avant la capture. Le plugin ne recrée pas les groupes absents du build et n'exécute pas toutes les commandes HUD stat.

Frame stats offre courbes synchronisées et tables par frame. Vérifier unité, disponibilité, frame source et instant d'observation avant de limiter l'intervalle. Les frames absentes restent absentes ; un scalaire GPU/RT retardé ne prouve pas l'identité de la frame GPU.

Une jauge (mémoire utilisée, occupation) n'est pas un cumul d'octets/événements. De grands entiers dépassent la précision flottante ; le SDK conserve leur forme exacte quand disponible. Les classes visuelles réduisent les données sans créer de mesures. Une interface à 20 Hz ne transforme pas une source Meta de 1 Hz en mesures à 20 Hz.

Voir [Profils](doc:profiles), [Activités](doc:activities) et [Compteurs Quest](doc:quest-counters).
