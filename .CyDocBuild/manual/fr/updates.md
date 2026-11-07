---
id: updates
title: Mises à jour, préférences et notices
description: Versions stables, continuité de signature et données.
section: Guides
order: 20
parent: reference
sourceRevision: fab8f5e9936a
---

Utiliser Help > Updates sous Windows ou Updates sur Quest. Recherche automatique au plus quotidienne, désactivable. Seules les nouvelles versions stables sont proposées : brouillons, alpha, bêta et candidates exclus. Télécharger les alpha manuellement.

Windows prépare/vérifie le ZIP complet. Dossier accessible en écriture et exécutables libérés par les autres instances sont nécessaires. L'outil sauvegarde les remplacements et tente un retour arrière en cas d'échec. Les captures peuvent être rouvertes après redémarrage ; une mise à jour normale n'efface pas volontairement réglages/enregistrements.

L’édition portable conserve les transactions de mise à jour et les préférences dans son dossier Data/ adjacent. Une mise à jour depuis le ZIP Windows de compatibilité conserve son marqueur portable et les fichiers Data existants. L’édition installée utilise le stockage de l’utilisateur ; relancer Setup actualise aussi les métadonnées Windows de l’installation et la liste des fichiers du désinstalleur.

Quest conserve la variante, vérifie identité/version/arm64, SHA-256 et continuité de signature avant Android. Confirmation/droit d'installation peuvent être requis. Un autre certificat bloque la mise à jour en place. Garder la clé de développement ; désinstaller avec perte de données n'est pas normal. Une couche mise à jour peut exiger de rouvrir le jeu.

Appearance conserve le thème, la densité et la langue de l’interface. Le manuel a son propre choix anglais/français et fonctionne hors ligne. LICENSE et les notices tierces, dont les licences MIT de CyUI et de CyDoc, restent fournies. Elles décrivent les composants inclus ; elles n’évaluent pas juridiquement une autre redistribution.

CyInsights et CyInsightOVR utilisent AGPL-3.0-or-later. Le plugin Unreal CyInsights utilise MIT. CyUI et CyDoc utilisent MIT ; leurs notices complètes et les licences des autres composants accompagnent l’application.

Erreurs réseau, quotas et packages stables absents sont signalés. Compilation/signature ne sont pas un verdict antivirus. Voir [Dépannage](doc:troubleshooting).
