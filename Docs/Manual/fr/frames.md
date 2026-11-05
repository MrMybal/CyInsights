# Frames et causes probables {#title}

Frames suit les timings. Choisir le budget : 60 FPS vaut environ 16,67 ms, 72 FPS 13,89 ms, 90 FPS 11,11 ms. La navigation trouve des candidats lents, sans prouver la cause.

## Lire les couleurs {#interpret-colors}
Le fond indique un goulot présumé GameThread, RenderThread, GPU ou mixte quand les preuves suffisantes dépassent le budget. Neutre signifie sans cause attribuée, pas que tout est rapide. Les couleurs de catégories des scopes restent distinctes du fond de diagnostic.

Attentes/cadencements reconnus sont séparés du travail actif. EngineLoop/Frame peut inclure synchronisation ou inactivité. Comparer intervalles actifs et attentes connues. Une attente longue peut être la conséquence d'un autre thread ou du GPU.

## Limites du pipeline {#pipeline-limits}
CPU et GPU peuvent traiter des frames différentes simultanément. Un scalaire GPU observé pendant une frame de jeu peut être retardé, sans identité exacte de frame GPU. Parents et enfants se recouvrent. Les inclusifs imbriqués sur plusieurs threads ne sont pas un pourcentage d'occupation.

Choisir un intervalle court, examiner le domaine suspect, comparer plusieurs mauvaises frames à une bonne zone puis vérifier par capture contrôlée. Voir [GPU](doc:gpu-desktop) et [Comparaison](doc:comparison).
