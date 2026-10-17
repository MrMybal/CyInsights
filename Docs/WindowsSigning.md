# Signature et provenance des versions Windows

Une signature Authenticode reconnue identifie l'éditeur et vérifie l'intégrité
du fichier. Elle ne garantit pas l'absence de détection antivirus. Les
attestations GitHub attestent le dépôt, le commit et le workflow de compilation ;
elles ne remplacent pas une signature Authenticode d'éditeur.

## Signature avec un certificat existant

`Tools/SignWindowsRelease.ps1` signe les exécutables CyInsights, ses lecteurs
natifs et son collecteur Windows dans le dossier choisi. Il ne crée aucun
certificat, ne modifie aucun magasin de confiance et ne signe pas les DLL Epic
ou les runtimes tiers. La clé reste dans le magasin Windows ou le périphérique
du fournisseur ; aucun PFX, mot de passe ou secret n'est passé en argument.

```powershell
./Tools/SignWindowsRelease.ps1 -Directory ./bin/Release -CertificateThumbprint <empreinte-certificat> -TimestampUrl <URL-RFC3161-du-fournisseur>
./Tools/SignWindowsRelease.ps1 -Directory ./bin/Release -VerifyOnly -RequireSigned
```

Le certificat doit être valide, destiné à la signature de code et avoir une
chaîne reconnue par Windows. Les signatures sont SHA-256, horodatées en RFC
3161 et vérifiées après signature. `windows-signatures.json` contient les
empreintes finales et l'état réel de chaque fichier. Un certificat auto-signé
ne constitue pas une signature d'éditeur publiquement reconnue.

`Tools/package_release.py --require-signed` refuse un produit non signé,
non reconnu ou non horodaté. Le packaging revérifie les signatures et les
empreintes ; il ne se fie pas à un ancien manifeste. Sans cette option, les
candidats de développement portent explicitement `signed: false` si nécessaire.

## GitHub

Le workflow manuel `release-candidate.yml` compile depuis le dépôt sur un
runner Windows, exécute les tests et fournit un candidat avec ses SHA-256 et
ses signatures réelles. Il ne publie pas automatiquement de release. L'option
`attest` ajoute une attestation de provenance GitHub ; sur un dépôt privé,
GitHub exige actuellement Enterprise Cloud. Ne pas rendre le dépôt public
uniquement pour activer cette option.

Pour un projet open source public éligible, SignPath Foundation propose une
signature reconnue liée au dépôt après acceptation du projet. Cette possibilité
n'est pas activée et ne signe pas automatiquement notre dépôt privé. Un autre
certificat ou service reconnu peut être configuré pour les distributions privées.

## Réanalyse antivirus

Conserver le nom de détection, l'heure, l'origine de compilation et l'empreinte
du fichier concerné. Un fichier disparu n'a pas d'empreinte connue ; une
nouvelle compilation est un autre objet à identifier séparément. Préparer un
dossier limité au logiciel, sans captures de jeu, clés ni chemins personnels.
Les transmissions à Kaspersky nécessitent une autorisation explicite. Un scan
sans détection décrit uniquement le fichier et les bases utilisés à cet instant.

Sources officielles consultées le 13 octobre 2026 :

- [Signature Windows](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control)
- [SignTool](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)
- [Attestations GitHub](https://docs.github.com/en/actions/concepts/security/artifact-attestations)
- [Disponibilité selon le plan GitHub](https://github.com/actions/attest)
- [SignPath Foundation](https://signpath.org/)
- [Réanalyse Kaspersky](https://support.kaspersky.com/1870)
