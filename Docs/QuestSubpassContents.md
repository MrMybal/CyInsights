# Contenu des sous-passes Vulkan

Le collecteur Quest peut enregistrer le contenu observé de chaque sous-passe,
en complément du temps GPU de la passe parente. Une nouvelle capture avec le
plugin actualisé est nécessaire : les anciennes barres ne contiennent pas ces
informations et ne peuvent pas être complétées après coup.

La collecte est active lorsque le collecteur de passes Vulkan est actif
(`quest_gpu_completion` dans le profil de base Android). Elle ajoute des
compteurs côté CPU et un événement de trace par sous-passe résolue. Elle
n'ajoute aucun timestamp GPU, aucune attente GPU et aucune lecture de texture
ou de buffer. Le surcoût CPU des interceptions et des événements reste à
mesurer sur une application Quest réelle ; aucune garantie de coût nul.

## Informations enregistrées

- Identité de passe locale au processus/capture, soumission, ordinal, taille
  de la zone de rendu et index de sous-passe, commençant à zéro.
- Nombre d'appels directs `vkCmdDraw` et indexés `vkCmdDrawIndexed`.
- Appels `vkCmdDrawIndirect` et `vkCmdDrawIndexedIndirect`, avec le nombre de
  commandes demandé. Le contenu des buffers indirects reste inconnu.
- Quantités de sommets/indices demandées par les appels directs, multipliées
  par leurs arguments d'instances. Ces quantités ne sont pas des mesures
  d'invocations de shaders, de triangles visibles ou d'objets uniques.
- Les buffers secondaires sont comptés à chaque `vkCmdExecuteCommands`,
  lorsqu'ils ont été enregistrés pendant la capture. Sinon la couverture est
  explicitement partielle. Les passes encore en vol à la fin de la trace peuvent être absentes ; les
  événements de contenu sont publiés après résolution de la mesure parente.
  Les autres familles de draw, dont les mesh shaders
  et les variantes indirect-count, ne font pas partie de cette première
  interception : les compteurs portent sur les API listées ci-dessus.

Une sous-passe contenant un draw direct de trois sommets fournit un indice sur
son contenu, mais ne prouve pas qu'il s'agit d'un traitement plein écran ou
d'une opération Unreal particulière. Aucun nom BasePass, éclairage ou
transparence n'est inventé à partir de ces compteurs.

## Interface et SDK

Cliquer sur une barre Vulkan individuelle ouvre **Selection / Vulkan subpass
contents**. Chaque sous-passe présente ses compteurs. Une barre agrégeant
plusieurs passes ou une association ambiguë ne fournit pas d'identité unique.

`query_vulkan_passes` expose les mêmes données au SDK/MCP et aux outils natifs.
Il accepte `start`, `end`, un `passId` facultatif, `offset` et `limit` (limite de réponse,
pas de capture). Sans bornes explicites, la plage sélectionnée dans la timeline
est utilisée, ou la plage visible si aucune sélection n’existe. Les grands compteurs et identifiants sont des chaînes
décimales. Les bornes `parentStart`/`parentEnd` appartiennent à la passe entière,
jamais à l'exécution individuelle de la sous-passe. Les coûts GPU individuels
et les noms Unreal restent explicitement indisponibles.

## Format et grandes traces

Les événements `CyInsightsVulkan.Contents` restent dans le `.utrace` original.
Le lecteur écrit un cache reconstructible `vulkan-pass-details.cyvp` : magic
`CYVP0001`, enregistrements fixes de 112 octets, footer `CYVPDONE` et nombre
d'enregistrements uint64. Le format exact est défini dans
`Tools/NativeTiming/VulkanPassDetailFormat.hpp`.

Les bornes GPU sont alignées par la même relation que la piste Vulkan parente,
qui demeure approximative. Les sous-passes ne reçoivent pas de durée fictive.
Le lecteur prépare les données sur disque ; l'application les mappe en lecture
seule et conserve des bornes par bloc de 4 096 lignes. Les détails ne sont pas
dupliqués dans une grande collection d'objets RAM. La validation se fait pendant
l'import asynchrone ; les requêtes SDK utilisent un worker et la cancellation.

## Validation au 12 octobre 2026

Les tests couvrent l'instancing, les réexécutions de buffers secondaires, les
valeurs indirectes inconnues, la sélection d'une passe, les plages temporelles,
les associations ambiguës, les entiers supérieurs à 2^53, la cancellation et
les caches incomplets. Une trace native synthétique explicitement identifiée
produit une passe et trois sous-passes via le vrai analyseur Unreal installé.
Les modules Android compilent sur Meta UE 5.5.3 et UE 5.8.3 ; les quatre
lecteurs natifs UE 5.5–5.8 compilent également. Les 21 suites C++, huit tests SDK
et onze tests Python passent.

Aucun test supplémentaire sur Beyond ou le casque n'a été effectué après le
changement de branche demandé par l'utilisateur. L'interception en conditions
Quest et son surcoût restent à valider. Le nouveau binaire de l'application a
été supprimé après une détection heuristique Kaspersky
`VHO:Backdoor.Win64.Agent.gen` avant lancement ; le panneau est implémenté et
compilé, mais sa vérification visuelle reste bloquée. Ce résultat n'établit
pas un faux positif. Aucune exclusion ou protection antivirus n'a été modifiée.

Les [stages matériels Meta](MetaQuestGpu.md) constituent une autre source :
ces compteurs Vulkan ne les remplacent pas.

Mise à jour du 13 octobre : la compilation propre dans le même dossier produit
un exécutable qui passe les scans Kaspersky et s'ouvre normalement. Le panneau
a désormais été vérifié visuellement avec la trace synthétique, et sa requête
SDK a été exercée via le vrai transport natif, y compris les plages et la
pagination. Les détails Quest réels restent à valider sur une nouvelle capture ;
aucun lancement de Beyond n'a été effectué. Voir [le suivi de sécurité](SecurityReview.md).
