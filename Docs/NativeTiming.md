# Lecture directe des timings `.utrace`

L'adaptateur facultatif [NativeTiming](../Tools/NativeTiming/README.md) utilise les
providers de l'analyseur Unreal installé. Il transmet les scopes CPU/GPU et frames
natives en binaire vers notre index paginé, en supprimant le gros CSV des timings
et son parsing décimal. Le `.utrace` source reste intact et `.cycapture` inchangé.
Cette optimisation ne crée aucun détail GPU absent de la trace.

La détection de l'adaptateur correspondant à l'analyseur choisi est automatique.
Sans adaptateur, le pont CSV reste disponible. Si un adaptateur échoue, l'erreur
et son journal sont affichés. Chargement et indexation restent en arrière-plan,
avec phase visible, progression et annulation. Aucun index partiel n'est publié.
Les comparaisons A/B utilisent aussi ce chemin. Les valeurs de compteurs à la
demande, tâches et activités conservent leurs lecteurs existants.

Les timestamps conservent les doubles natifs des providers ; de petites différences
avec un CSV arrondi sont normales. Les frames gardent leurs identités 64 bits,
les scopes incomplets ne deviennent pas des durées mesurées et les horloges restent
celles des providers Unreal.

## Mesure locale et validation

Deux ouvertures de la même capture de **14 204 345 événements** et **2 396 compteurs**,
avec le même exécutable et l'analyseur installé, donnent :

| Étape | Pont CSV | Lecture directe |
| --- | ---: | ---: |
| Décodage + transmission des timings | 33,26 s | 1,84 s |
| Construction de l'index | 9,03 s | 1,70 s |
| Processus ouvert jusqu'à vue prête / snapshot | 42,77 s | 4,05 s |
| Payload temporaire des timings | 1 309 778 441 octets | 454 796 179 octets |

Ce sont des essais locaux avec des fichiers déjà présents sur disque, pas une
garantie pour chaque trace ou matériel. Le gain concerne l'ouverture, pas le FPS.
Le flux et l'index sont temporaires et non compressés : le `.utrace` peut être
beaucoup plus petit que ses événements décodés. Aucun plafond d'acceptation n'a
été ajouté.

`CyNativeTimingValidation` parcourt les payloads sans charger tous les scopes en
RAM. Les empreintes par thread/fonction/identité, début/fin, profondeur et frame
correspondent pour les **14,2 millions d'événements**, à la précision CSV (100 ns).
Les 8 046 groupes correspondent, ainsi que les scopes incomplets. Les tests du
transport couvrent UTF-8, CPU/GPU, frames 64 bits, temps exclusif, annulation,
version inconnue, références et données tronquées.

Les 17 vérifications binaires et les 16 suites natives passent. Les adaptateurs
5.5, 5.6, 5.7 et 5.8 ont été compilés puis exécutés sur des traces synthétiques ;
5.5 utilise une fixture produite par son propre writer, les schemas de frames
plus récents n'étant pas compris par cet ancien analyseur. Navigation continue,
zoom, frames synchronisées, double-clic et histogrammes par plage passent sur
la trace réelle via le lecteur direct. Cela ne constitue pas une validation
matérielle de toutes les plateformes ou de toutes les versions de traces.

```powershell
cmake --build build/Local --target CyNativeTimingValidation
build/Local/CyNativeTimingValidation.exe timing.cynt timing.csv
```

Pour comparer les backends, `CYINSIGHTS_TIMING_CSV=1` force le pont CSV. Retirer
la variable réactive la détection automatique. Les rapports de snapshot indiquent
le backend, les temps de décodage/indexation et le volume des timings.
