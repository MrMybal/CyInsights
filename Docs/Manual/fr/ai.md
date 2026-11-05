# AI Inspector, MCP et SDK {#title}

AI Inspector comprend Method guidée et catalogue Tools natif. Ils fonctionnent localement sans CyAiCodex, Python ni client MCP. Préparer n'exécute pas l'outil. Le modèle conversationnel externe tourne dans le client choisi ; aucun fournisseur de modèle n'est intégré.

## Connexion externe {#external-connection}
Dans AI Inspector > Connection, activer explicitement le pont local MCP/SDK du document. La lecture seule suffit à l'analyse. Droits de contrôle et de capture distante sont distincts, désactivés initialement. Employer l'endpoint généré pour ce processus/document ; un nouveau processus peut demander une nouvelle configuration. Garder les jetons privés.

Le package autonome AI/ fournit compagnon MCP/SDK, dépendances Python hors ligne et méthode. Découvrir schémas/fournisseurs au lieu de supposer leur disponibilité. Le catalogue actuel a 39 opérations partagées et peut évoluer.

## Parcours ciblé {#compact-workflow}
Découvrir génération/fournisseurs, trouver quelques frames lentes, choisir un court intervalle, classer les exclusifs, examiner GPU/histogramme utile et produire des constats nuancés. Commencer par résumés et limites 5–20. Demander événements/bindings/classes seulement si utiles. Interface et MCP partagent les analyses natives.

Attendre Completed/Failed/Cancelled des tâches longues. Exporter avant de terminer une tâche éphémère. Annuler l'analyse n'arrête pas une capture distante détenue : l'arrêter explicitement. Finir la tâche IA ne supprime jamais une trace persistante. Aucune requête ne récupère un canal absent.

Un rapport précise génération, bornes, unités, mesures, résultats calculés, hypothèse et vérification concrète. Voir [Glossaire](doc:glossary) et [Comparaison](doc:comparison).
