# Chargement des grandes traces

La release `Release-20261008.1` supprime les plafonds de taille du CSV et de nombre d'événements du timing natif. Le fichier `.utrace` original reste intact. Sa taille ne doit pas être confondue avec celle de l'export CSV intermédiaire, beaucoup plus volumineux : `20261008_142920.utrace` occupe 49 648 645 octets, tandis que ses 14 192 325 événements occupent 1 308 743 231 octets dans l'export Epic.

## Chargement et stockage

L'application affiche d'abord une fenêtre de chargement. L'analyseur Epic installé analyse la trace et exporte les timings sans interface. Pendant cette phase, CyInsights affiche la phase, le temps écoulé et la taille de l'export qui croît ; aucun pourcentage d'analyse n'est inventé. Ensuite, la lecture séquentielle du CSV affiche l'avancement en octets et le nombre d'événements indexés. **Cancel** annule les tâches appartenant à cette ouverture. Les erreurs restent visibles avec le chemin du journal et une commande de copie des détails.

Tous les événements sont écrits dans `timing.cyindex` sous le cache temporaire `%TEMP%/CyInsights/<session>`. L'index utilise des offsets de fichier sur 64 bits et des blocs séparés par piste et profondeur. Les buffers sont vidés progressivement ; leurs seuils déterminent les écritures, jamais l'acceptation ou la perte d'événements. Les métadonnées, les guides de frames, la fenêtre d'affichage et un cache réutilisable de blocs restent en mémoire dans l'application. L'analyseur Epic conserve son propre modèle pendant l'export.

Une requête de fenêtre lit les blocs pertinents en arrière-plan, avec des scopes voisins en réserve pour le déplacement. Le modèle et les événements correspondant à cette fenêtre sont publiés ensemble. L'ordre des pistes reste stable ; leur hauteur suit les profondeurs présentes dans l'intervalle visible. Le glissement reste actif jusqu'au relâchement du bouton, indépendamment des rechargements. Les scopes qui traversent une frame ou une borne visible gardent leurs timestamps continus. Le graphique Frames partage désormais cet axe et cet intervalle.

Depuis `Release-20261009.1`, la recherche des blocs est indexée par piste/profondeur et les blocs récemment lus sont réutilisés. Le budget de 32 Mio concerne uniquement les données de ce cache en mémoire, avec éviction ; il ne refuse aucune capture et ne supprime aucun événement source. Voir [les mesures de vitesse du graphique](GraphPerformance.md).

Depuis `Release-20261009.2`, le décodage CSV par lots, la préparation des pistes/profondeurs et la construction des indices de pistes utilisent un pool partagé de workers. L'ingestion reste dans l'ordre source, les résultats sont réunis dans un ordre déterministe et l'annulation attend la fin des travaux démarrés avant de libérer les données. La préparation est adaptative et réglable dans Session. Voir [la préparation multithread et ses mesures](MultithreadedPreparation.md).

Le CSV intermédiaire et le cache sur disque nécessitent de l'espace disque. Les erreurs système de lecture, écriture ou allocation sont affichées ; aucun budget arbitraire de taille ou d'événements ne refuse une `.utrace`. Le pont Epic reste nécessaire. Cette version recrée son index à chaque ouverture ; la réutilisation persistante du cache et un décodeur natif direct restent à réaliser. Les archives conservent leurs règles distinctes de validation de format.

## Affichage et statistiques

Les scopes plus petits qu'un pixel sont regroupés par piste, profondeur et colonne d'écran. Le nombre d'événements et leur étendue sont conservés ; le tooltip signale explicitement une **agrégation d'affichage dérivée**. Un groupe ne reçoit pas l'identité unique d'un timer ou d'un objet. Zoomer recharge les événements qui deviennent discernables. Les scopes discernables restent individuels.

Les statistiques de l'intervalle et les comparaisons A/B utilisent tous les enregistrements natifs de l'intervalle, indépendamment du regroupement visuel. Les coûts exclusifs sont déduits des enfants immédiats contenus. Si l'ordre source ne permet pas une imbrication fiable, les coûts exclusifs deviennent indisponibles ; les coûts inclusifs restent calculés. Les scopes inachevés sont recensés séparément, sans inventer leur durée.

Les valeurs des compteurs se chargent à la demande avec **Load counter samples**. Le catalogue ne retarde pas l'ouverture des timings par une seconde analyse complète des valeurs. Les plafonds de catalogue et d'échantillons sont retirés, mais les valeurs restent actuellement en mémoire.

## Navigation

- Molette sur l'aperçu supérieur ou les pistes : zoom.
- Bouton gauche maintenu sur l'aperçu supérieur ou la règle : mouvement horizontal pour déplacer le temps, vertical pour zoomer.
- Shift + glissement gauche sur la règle : sélectionner l'intervalle de statistiques.
- Bouton du milieu sur les pistes : déplacement horizontal ; Shift + molette : défilement vertical.

## Vérification

Les huit suites isolées passent 249 vérifications, dont la conservation des événements dans l'index, le regroupement par pixel, les scopes traversant les bornes, les statistiques sur les données complètes et l'annulation. Sur la trace de 49,6 Mo, le test de stockage conserve exactement 14 192 325 événements ; une vue de toute la capture à 2 000 pixels représente ces événements par 52 416 enregistrements d'affichage. Le cache binaire occupe 454 154 408 octets.

La même `.utrace` a été ouverte dans l'application, rendue puis fermée normalement. Le test injecte les événements d'entrée dans sa propre interface ImGui et vérifie le déplacement horizontal, le zoom vertical et le zoom à la molette. L'empreinte SHA-256 du fichier source avant et après est identique. Voir [le journal d'ouverture](../build/release-20261008.1-audit/opening.log), [le résultat](../build/release-20261008.1-audit/user-opening.json) et [le rendu réel](../build/release-20261008.1-audit/ui-user-trace.png).

Une seconde ouverture réelle a validé `20260928_162518.utrace`, de **1 403 637 916 octets**. L'export contient **353 528 278 événements**, tous présents dans le cache de **11 312 904 904 octets** (8 octets d'en-tête et 32 par événement). Le CSV intermédiaire occupe **30 743 346 326 octets**. La taille et les offsets dépassent donc 4 Go sans troncature. L'interface rend les pistes et statistiques, les trois gestes de navigation passent et l'application se ferme normalement. L'empreinte source reste identique.

Cette première ouverture a pris environ **1 050 secondes**, soit **17 min 30 s**, sur cette machine : le pont CSV reste coûteux. Les échantillons de mémoire de l'application, pris toutes les dix secondes pendant le traitement, montent jusqu'à environ 155 Mio ; ce relevé n'est pas une mesure exhaustive du pic, et l'analyseur Epic utilise sa propre mémoire pendant l'export. Ces résultats valident ces deux captures, pas tous les fichiers possibles. Voir [le résultat de la grande capture](../build/release-20261008.1-audit/large-opening.json) et [son rendu réel](../build/release-20261008.1-audit/ui-large-trace.png).

La release `Release-20261009.3` conserve le stockage sans plafond et ajoute des diagnostics par frame préparés pendant l’indexation. Les résultats restent en cache pendant la navigation ; la lecture initiale jetée a été retirée. Voir [FrameDiagnostics.md](FrameDiagnostics.md).
