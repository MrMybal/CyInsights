---
id: ai
title: AI Inspector, MCP et SDK
description: Outils locaux, clients externes, droits et preuves ciblées.
section: Guides
order: 0
parent: developer-api
sourceRevision: 902403edeac5
---

AI Inspector permet d’exécuter les outils d’analyse natifs et de consulter les constats. Ouvrez **Tools**, choisissez une opération, vérifiez ses paramètres, puis lancez-la. Préparer le formulaire n’exécute pas l’outil. Les outils natifs ne demandent ni modèle externe, ni Python, ni client MCP.

## Connexion externe {#external-connection}
1. Ouvrez **AI Inspector > Connection** et activez le pont local MCP/SDK.
2. Copiez la configuration dans votre client MCP. CyInsightsCli natif est deja fourni : aucun telechargement d’environnement ni installation Python.
3. Free est le profil par défaut : copiez l’URL HTTP et connectez-vous sans jeton. Toutes les opérations exposées sont autorisées, avec validation des arguments. Local contrôlé, Jeton seul et Protégé restent disponibles.
4. Recopiez le PID/document apres redemarrage ou changement de document. Ne publiez pas les jetons du fichier de connexion HTTP prive.

## Profils d’accès locaux {#access-profiles}

| Profil | Jeton HTTP | Opérations |
| --- | --- | --- |
| Free (défaut) | Aucun | Toutes les opérations exposées |
| Local contrôlé | Aucun | Droits de contrôle/capture |
| Jeton seul | Requis | Toutes les opérations exposées |
| Protégé | Requis | Droits de contrôle/capture |

Choisissez le profil dans **Connection**. HTTP écoute sur `127.0.0.1` ; Free
nécessite uniquement l’adresse affichée. Changer le profil redémarre le serveur
actif : recopiez sa nouvelle URL. Les configurations sans choix explicite migrent
vers Free ; les profils déjà choisis sont conservés. Un réglage explicite invalide
utilise Protégé sans réécrire le fichier original. La validation et les conditions
techniques restent actives. Les droits du terminal du fournisseur restent distincts.

## Assistant integre {#integrated-assistant}
Ouvrez **Assistant > Connexion et options**, detectez les fournisseurs/services locaux, choisissez explicitement un fournisseur et un modele, puis connectez-vous. Le panneau CyMCPSDK affiche le Markdown progressif, les outils et erreurs, permet l’annulation et les pieces jointes compatibles. Utilisez une variable d’environnement ou une cle en memoire. L’historique restaure ne renvoie aucun tour. Aucun poids de modele n’est fourni ; Codex, Ollama, LM Studio, llama.cpp et les API compatibles conservent leur authentification et leurs capacites.

Les 39 operations metier et neuf utilitaires de taches/ressources partagent un registre type. Decouverte compacte, stockage temporaire sur disque, TTL/cache, abonnements, mesures sans contenu et fenetre de contexte API se reglent separement. Les reglages de transport/stockage demandent un redemarrage ; reconnectez pour les conversations. L’historique complet reste sauvegarde lorsque la fenetre API est reduite. Les agents natifs gerent leur contexte. Aucun resume automatique n’est appele.

Utilisez job_start/job_status/job_result pour conserver les resultats, job_list avec offset/limit et job_cancel pour annuler. Les references SDK expirent ; les resultats durables restent disponibles jusqu’a suppression explicite. Suivez firstUri/nextUri du manifeste pour lire tout un gros resultat. Une expiration ne rejoue aucune action. job_import copie un ancien rapport JSON avec le controle active ; les anciens environnements et bases sont preserves.

Les serveurs MCP distants optionnels demandent une liste explicite d’outils autorises et une variable de jeton. Aucun n’est contacte par defaut. HTTPS est requis hors boucle locale. La [reference CLI et API native](doc:developer-api) presente les operations publiques.

## Montrer les résultats dans l’interface {#show-evidence}

Utilisez Free/Jeton seul, ou activez **Allow AI application control** dans un
profil contrôlé, pour autoriser les actions natives :
sélectionner et cadrer un intervalle (`set_range`), mettre une fonction exacte en
évidence (`filter_function`), ouvrir son histogramme (`show_histogram`), afficher
un panneau (`focus_view`), épingler un compteur enregistré (`pin_counter`) ou
publier un constat (`publish_finding`). Essayez : « Trouve les frames les plus
lentes, montre-moi l’intervalle et affiche les compteurs utiles. »

L’analyse en lecture seule reste disponible sans contrôle. Dans Local contrôlé
et Protégé, l’enregistrement nécessite le droit de capture distinct. L’accès
passe par les opérations déclarées et les sources enregistrées : l’assistant ne
contrôle pas chaque bouton, ne parcourt pas librement les fichiers et n’inspecte
pas automatiquement toutes les images et heatmaps. Les canaux absents restent
indisponibles. Vérifiez les capacités avant de conclure.

## Examiner une analyse {#compact-workflow}
Sélectionnez un court intervalle suspect avant de lancer un outil. Dans **Method**, suivez les étapes proposées et vérifiez le formulaire de paramètres obtenu. Comparez l’intervalle, les unités et la source d’un constat avec les [frames](doc:frames) ou les [données GPU](doc:gpu-desktop). Une cause probable reste une hypothèse.

Un fournisseur peut continuer à charger après l’acceptation d’une demande ; consultez la progression et la disponibilité affichées. Annulez le chargement si nécessaire. Arrêter une analyse n’arrête pas une capture distante : utilisez **Stop remote trace** pour cet enregistrement. Aucun rapport ne récupère les canaux absents de la capture.

## Investigation ciblée {#analysis-flow}

Les résumés orientent l’étude ; les événements individuels étayent un constat.

```diagram
{"type": "flow", "title": "Investigation ciblée", "nodes": [{"id": "n0", "label": "Vérifier sources/unités", "x": 0, "y": 0}, {"id": "n1", "label": "Repérer les frames lentes", "x": 310, "y": 0}, {"id": "n2", "label": "Choisir une courte zone", "x": 0, "y": 100}, {"id": "n3", "label": "Examiner un domaine", "x": 310, "y": 100}, {"id": "n4", "label": "Comparer un bon cas", "x": 0, "y": 200}, {"id": "n5", "label": "Preuves + limites", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
