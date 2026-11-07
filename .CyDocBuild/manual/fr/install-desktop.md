---
id: install-desktop
title: Installer et préparer les lecteurs natifs
description: Installateur Windows, archive portable et ajout explicite du moteur.
section: Guides
order: 1
parent: desktop
sourceRevision: 7145e6b195b8
---

Le produit PC cible Windows x64 ; les traces Android sont analysées sur PC. Les deux formats Windows incluent l’application, les outils auxiliaires, ce manuel hors ligne complet et les notices de licence.

## Choisir une édition {#choose-edition}

Utiliser l’EXE Setup pour une installation par utilisateur, avec raccourcis dans le menu Démarrer et entrée dans les applications Windows. Aucun droit administrateur n’est requis. Le raccourci bureau et l’ajout à « Ouvrir avec » sont optionnels. Les réglages restent dans LocalAppData/CyInsights ; la désinstallation retire les fichiers livrés en conservant les préférences et captures supplémentaires. Fermer CyInsights et les outils de capture avant l’installation ou la désinstallation.

Pour le ZIP portable, extraire toute l’archive dans un dossier accessible en écriture et lancer CyInsightsApp.exe. Garder CyInsightsUpdater.exe, CyCaptureTool.exe, Documentation/ et cyinsights-portable.txt à côté. Aucun installateur, raccourci ou association de fichiers n’est nécessaire.

## Données portables {#portable-data}

En présence de cyinsights-portable.txt, les préférences, la disposition des fenêtres, le cache des lecteurs natifs, les journaux et les transactions de mise à jour utilisent le dossier Data/ adjacent. Déplacer le dossier complet conserve ces fichiers. Cette édition ne lit pas les préférences de l’édition installée. Les chemins et lecteurs Unreal restent liés à l’installation du moteur sur la machine ; le navigateur et le système peuvent conserver leurs propres données temporaires et préférences de lecture. Les captures originales restent distinctes des réglages et caches d’index.

## Ajouter Unreal {#register-unreal}
Dans Session, développer Sources / Native analyzer. Browse engine folder choisit la racine du moteur ou son dossier Engine ; cliquer Add engine. L'enregistrement est conservé. Prepare native readers compile les adaptateurs locaux adaptés, pas le moteur entier. En-têtes/bibliothèques du moteur et outils C++ compatibles doivent exister.

L'ajout est explicite, sans recherche automatique sur tout le disque. Si un lecteur manque ou est incompatible, ajouter le bon moteur et relancer Analysis > Analyze native timing après préparation. Un pont d'export reste disponible pour les fournisseurs compatibles. Les binaires Epic ne sont pas redistribués dans ce package.

Compatibilité des lecteurs et du plugin sont distinctes. Un SDK local incomplet peut bloquer la préparation même si le plugin couvre cette famille. Garder le .utrace original ; un index persistant est un cache. Un antivirus peut supprimer un exécutable compilé : vérifier fichier et rapport exacts si le lancement échoue.

Suite : [Plugin Unreal](doc:unreal-plugin) ou [Ouvrir une capture](doc:opening-traces).
