# CyInsights 0.2.2 Alpha 7

**Windows x64 · ALPHA**

## Accès MCP local Free par défaut

CyInsights compile désormais CyMCPSDK **0.7.1**, révision
`34d4bcae126bc83d1a2925b5047dac36e3d09d92`, depuis ses sources natives figées.
Activez le pont dans **AI Inspector > Connection**, copiez son URL HTTP et
connectez-vous avec cette adresse seule. Free autorise les opérations exposées
sans confirmation supplémentaire de l’application. La validation des arguments,
la disponibilité des données et la propriété des captures restent contrôlées.
HTTP écoute uniquement sur `127.0.0.1`.

Le panneau de connexion propose quatre profils persistants :

| Profil | Jeton HTTP | Droits sur les opérations |
| --- | --- | --- |
| Free (défaut) | Aucun | Toutes les opérations exposées |
| Local contrôlé | Aucun | Droits de contrôle/capture |
| Jeton seul | Requis | Toutes les opérations exposées |
| Protégé | Requis | Droits de contrôle/capture |

Les configurations sans profil explicite migrent vers Free sans perdre les autres
réglages. Les choix explicites existants sont conservés. Un réglage explicite
invalide ou illisible utilise Protégé sans réécrire le fichier original. Changer
de profil redémarre le serveur HTTP actif : recopiez ensuite sa nouvelle adresse.

La CLI native ajoute `--url URL` pour les connexions avec l’adresse seule. Les
fichiers privés de connexion et les tubes nommés de l’utilisateur Windows restent
pris en charge. Chat, CLI et MCP partagent les droits des opérations ; les droits
du terminal et des fichiers du fournisseur natif restent distincts. Le manuel
hors ligne anglais/français et le guide d’intégration expliquent ces profils.

## Téléchargements

- **Windows-x64-Setup.exe** : installation utilisateur, assistant/CLI natifs et
  manuel CyDoc hors ligne anglais/français complet.
- **Windows-x64-Portable.zip** : extraire toute l’archive ; préférences dans `Data/`.
- **Windows-x64-Update.zip** : paquet complet de mise à jour de l’application.
- **AI-SDK.zip** : CLI native, contrats publics, guide et notices ; ces composants
  sont déjà intégrés à l’application Windows.
- **Unreal-Plugins-Source.zip** : sources du plugin Unreal optionnel.

Les APK Quest restent inchangés : utilisez les
[téléchargements Quest Alpha 5](https://github.com/MrMybal/CyInsights/releases/tag/v0.2.2-alpha.5).

## Vérifications et limites de l’alpha

Les tests locaux Windows x64 couvrent les quatre profils sur de vraies instances :
lecture et modification sans jeton en Free, refus selon les droits métier,
rejet des jetons absents ou incorrects dans Protégé et Jeton seul, puis modification
autorisée dans Protégé. Les parcours CLI et chat couvrent la réponse progressive,
un vrai changement de filtre, l’annulation, la restauration et les pièces jointes
avec un serveur de test du protocole. L’inférence auprès d’un vrai fournisseur
et les autres plateformes de bureau restent à valider.

Les binaires Windows ne sont pas signés. Cette alpha expérimentale s’installe
manuellement ; la mise à jour automatique conserve la sélection des versions stables.

[Guide d’intégration](AiIntegration.md) ·
[Manuel français](https://mrmybal.github.io/CyInsights/Documentation.html#/fr/ai) ·
[English](Release-0.2.2-alpha.7.md)
