# Installer et préparer les lecteurs natifs {#title}

Extraire le package Windows complet dans un dossier accessible en écriture et lancer CyInsightsApp.exe. Garder CyInsightsUpdater.exe et CyCaptureTool.exe à côté. Le produit PC cible Windows x64 ; les traces Android sont analysées sur PC. Préférences et index sont distincts des originaux.

## Ajouter Unreal {#register-unreal}
Dans Session, développer Sources / Native analyzer. Browse engine folder choisit la racine du moteur ou son dossier Engine ; cliquer Add engine. L'enregistrement est conservé. Prepare native readers compile les adaptateurs locaux adaptés, pas le moteur entier. En-têtes/bibliothèques du moteur et outils C++ compatibles doivent exister.

L'ajout est explicite, sans recherche automatique sur tout le disque. Si un lecteur manque ou est incompatible, ajouter le bon moteur et relancer Analysis > Analyze native timing après préparation. Un pont d'export reste disponible pour les fournisseurs compatibles. Les binaires Epic ne sont pas redistribués dans ce package.

Compatibilité des lecteurs et du plugin sont distinctes. Un SDK local incomplet peut bloquer la préparation même si le plugin couvre cette famille. Garder le .utrace original ; un index persistant est un cache. Un antivirus peut supprimer un exécutable compilé : vérifier fichier et rapport exacts si le lancement échoue.

Suite : [Plugin Unreal](doc:unreal-plugin) ou [Ouvrir une capture](doc:opening-traces).
