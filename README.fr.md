# Asura's Wrath Recompiled — fork amélioré

[English](README.md)

<img alt="Asura's Wrath" src="https://github.com/user-attachments/assets/339f7f1b-85d4-48ac-9b5f-ac4fad276dad" />

*Illustration originale de Satoshi Sakai.*

Une version PC Windows native d'**Asura's Wrath** (Xbox 360, identifiant
`43430817`), obtenue en recompilant statiquement l'exécutable PowerPC d'origine
en C++ avec le SDK [ReXGlue](https://github.com/rexglue/rexglue-sdk).

Ce dépôt est un fork de [PauloUbirajara/asuras-wrath-recomp](https://github.com/PauloUbirajara/asuras-wrath-recomp)
qui ajoute des correctifs de stabilité, la mise à jour 1 et les DLC, le 60 FPS,
des options de confort, un launcher/installateur et le remplacement de textures.

> **Aucun fichier du jeu n'est fourni.** Il vous faut votre propre copie du jeu
> (image disque Xbox 360) et sa mise à jour 1.

> **Ce projet n'est pas maintenu.** Il est publié tel quel, comme une
> expérience terminée. Les tickets et pull requests ne recevront probablement
> pas de réponse : n'hésitez pas à le forker.

---

## Sommaire

1. [Fonctionnalités](#fonctionnalités)
2. [Jouer (téléchargement et installation)](#jouer-téléchargement-et-installation)
3. [Launcher et réglages](#launcher-et-réglages)
4. [Touches en jeu](#touches-en-jeu)
5. [Remplacement de textures (textures HD)](#remplacement-de-textures-textures-hd)
6. [Problèmes connus](#problèmes-connus)
7. [Compiler soi-même](#compiler-soi-même)
8. [Organisation du projet](#organisation-du-projet)
9. [Crédits](#crédits)
10. [Mentions légales](#mentions-légales)

---

## Fonctionnalités

Par rapport à la recompilation d'origine :

**Correctifs**
- Plantage au démarrage sous Windows (`0xC0000409`, chemin de l'exécutable).
- Plantages aléatoires (corruption du tas) : la liste des manettes était
  reconstruite par plusieurs threads sans verrou.

**Contenu du jeu**
- **Mise à jour 1** intégrée (corrige des blocages, par exemple à l'épisode 7).
- **DLC** : les paquets sont installés automatiquement au lancement.

**Améliorations**
- **60 FPS** avec la bonne vitesse de jeu (F6 bascule 30/60 en jeu).
- **Commandes adaptées aux QTE** : aucun appui perdu pendant un ralentissement,
  et un **turbo** facultatif (maintenir un bouton au lieu de le marteler) pour
  les QTE de martèlement, plus durs en 60 FPS.
- **Logos de démarrage passés.**
- **Effets désactivables** : flou de mouvement, profondeur de champ, bloom,
  occlusion ambiante, rayons de lumière, reflets d'objectif, distorsion, flou
  radial, plus des textures et ombres plus fines (expérimental).
- Mise à l'échelle **AMD FidelityFX** (CAS, FSR) et résolutions internes plus
  élevées.
- **Menu de triche (F9)** : recherche mémoire (façon Cheat Engine), valeurs
  modifiables ou bloquées, mode dieu et Burst instantané.
- **Remplacement de textures** : n'importe quelle texture peut être remplacée
  par votre propre fichier DDS, en n'importe quelle résolution — la base pour
  des packs de textures HD (aucun n'est fourni).
- **Launcher / installateur** : installe les fichiers du jeu depuis votre image
  disque et modifie les réglages, en français ou en anglais.

---

## Jouer (téléchargement et installation)

Il faut Windows 10/11 64 bits, une carte graphique DirectX 12, environ 7 Go
d'espace libre, et :

- votre **image disque d'Asura's Wrath** (`.iso`, Xbox 360) ;
- la **mise à jour 1**, fichier `TU_11K620N_000000K000000.0000000000085`
  (obligatoire : le code recompilé vise cette version) ;
- facultatif : vos paquets de **DLC** (les fichiers de
  `Content\0000000000000000\43430817\00000002` sur une Xbox 360 ou vos
  sauvegardes).

Étapes :

1. Téléchargez le dernier `AsurasWrathRecompiled_windows_x64.zip` sur la
   [page des Releases](https://github.com/ReActHax/asuras-wrath-recomp/releases).
2. Décompressez-le dans le dossier de votre choix (pas dans `Program Files`).
3. Lancez **`AsuraLauncher.exe`**. Au premier lancement, il demande l'image
   disque, le fichier de mise à jour et le dossier des DLC, puis extrait et
   vérifie le tout (quelques minutes).
4. Ajustez les réglages si vous le souhaitez, puis cliquez sur **Jouer**.

Les sauvegardes, journaux, DLC et textures sont dans
`Documents\asura_wrath_recomp`.

Pour mettre à jour, décompressez une nouvelle version par-dessus l'ancienne :
les fichiers du jeu (`extracted\`) et vos réglages (`asura_wrath_recomp.toml`)
sont conservés.

---

## Launcher et réglages

`AsuraLauncher.exe` modifie `asura_wrath_recomp.toml`, à côté du jeu (les
commentaires et les options inconnues sont conservés). Chaque option est
expliquée dans le launcher et dans
[`asura_wrath_recomp.example.toml`](asura_wrath_recomp.example.toml).

| Onglet | Options |
| --- | --- |
| Affichage | plein écran, résolution interne (1× à 3×), mise à l'échelle (bilinéaire, CAS, FSR), filtrage anisotrope, synchronisation verticale, rendu (D3D12/Vulkan), langue du jeu |
| Jeu | 60 FPS, passer les logos, désactiver le post-traitement |
| Effets | flou de mouvement, profondeur de champ, bloom, occlusion ambiante, rayons de lumière, reflets d'objectif, distorsion, flou radial, détail des textures, ombres haute définition |
| Commandes | mémoriser les appuis rapides (QTE), turbo, boutons et cadence du turbo |
| Textures | remplacement de textures, extraction des textures, ouvrir le dossier des textures |
| Infos | touches en jeu, dossier de données, installer / réparer les fichiers du jeu |

Conseillé : résolution interne 1× avec la mise à l'échelle **FSR 1**. Les
résolutions internes plus élevées sont plus nettes mais coûtent très cher, car
la mémoire vidéo de la Xbox 360 (EDRAM) doit être émulée à cette résolution.

Toutes les options s'utilisent aussi en ligne de commande, par exemple
`asura_wrath_recomp.exe --resolution_scale=2 --gpu_backend=d3d12`.

---

## Touches en jeu

| Touche | Action |
| --- | --- |
| F4 | Réglages en jeu |
| F6 | 30 / 60 FPS |
| F8 | Turbo activé / désactivé |
| F9 | Menu de triche |
| Alt+Entrée | Plein écran |

---

## Remplacement de textures (textures HD)

Toute texture 2D du jeu peut être remplacée par un fichier DDS de n'importe
quelle taille : extrayez les textures, modifiez-les ou agrandissez-les, puis
déposez-les dans `textures\replace`. Aucun pack n'est fourni. Voir
[docs/TEXTURE_MODDING.fr.md](docs/TEXTURE_MODDING.fr.md).

---

## Problèmes connus

- **Vulkan** affiche de nombreux défauts graphiques dans ce jeu (éclairage,
  effets) : utilisez Direct3D 12.
- Les résolutions internes au-delà de 1× sont très gourmandes.
- `fsr2` / `fsr3` ne sont pas de vrais upscalers temporels ici (pas de vecteurs
  de mouvement).
- `asura_fx_texture_detail` et `asura_fx_high_shadows` sont expérimentaux et
  utilisent plus de mémoire.
- L'ultra-large n'est pas pris en charge (jeu en 720p fixe, HUD et cinématiques).

---

## Compiler soi-même

Pour les développeurs. Les joueurs utilisent les
[releases](https://github.com/ReActHax/asuras-wrath-recomp/releases).

Il faut :
- Visual Studio 2022 (ou les Build Tools) avec *Développement Desktop en C++*,
  *Outils Clang C++ pour Windows* et *Outils CMake C++* ;
- Git ;
- les fichiers du jeu : votre image disque extraite plus le `default.xexp` de
  la mise à jour 1, dans `extracted\` à la racine du dépôt (le générateur de
  code lit `extracted\default.xex` et applique `default.xexp`). Le plus simple
  est de les installer avec `AsuraLauncher.exe` d'une release, puis de copier
  ou lier son dossier `extracted`.

```bat
git clone --recursive https://github.com/ReActHax/asuras-wrath-recomp.git
cd asuras-wrath-recomp
rem extracted\ doit contenir default.xex, default.xexp et BCGame\
package_release.bat
```

Lancez `package_release.bat` depuis l'*Invite de commandes développeur pour
VS 2022*. Il configure et compile avec CMake (preset `win-amd64-release`,
D3D12 + Vulkan + FidelityFX), copie le jeu dans
`out\dist\asura_wrath_recomp_windows_x86_64`, compile le launcher avec le
compilateur C# inclus dans Windows et crée l'archive de publication dans
`out\release`. L'archive ne contient jamais de fichiers du jeu.

Compilation manuelle :

```bat
cmake --preset win-amd64-release -DREXGLUE_USE_VULKAN=ON -DREXGLUE_USE_D3D12=ON -DREXGLUE_ENABLE_FIDELITYFX=ON
cmake --build out/build/win-amd64-release --parallel
```

Notes techniques (fonctionnement des patchs, adresses, pièges) :
[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) (en anglais).

Les versions Linux et Android viennent du projet d'origine et n'ont pas été
testées avec les modifications de ce fork.

---

## Organisation du projet

| Chemin | Contenu |
| --- | --- |
| `src/asura_patches.cpp` | 60 FPS, post-traitement, logos, raccourcis |
| `src/asura_config_patch.cpp` | effets désactivables (copies modifiées de la configuration `Coalesced_*.bin` du jeu) |
| `src/asura_cheats.cpp` | menu de triche F9 et recherche mémoire |
| `src/asura_trainer.cpp` | mode dieu et Burst instantané |
| `src/asurawrath_app.h` | point d'entrée, chemins, préparation des données du jeu |
| `includes/funcs.toml` | adresses des fonctions pour la mise à jour 1 |
| `generated/` | code du jeu recompilé (sortie du générateur) |
| `tools/rexglue` | fork du SDK ReXGlue ([ReActHax/asura-wrath-rexglue-sdk](https://github.com/ReActHax/asura-wrath-rexglue-sdk)) : remplacement de textures, turbo, installation des DLC, correctifs… |
| `tools/launcher` | `AsuraLauncher.cs` (launcher / installateur) |
| `tools/re` | petits outils pour lire le code généré |
| `docs/` | guide des textures, notes techniques |

---

## Crédits

- **PauloUbirajara** — recompilation d'origine d'Asura's Wrath et fork du SDK.
- **Tom Clay et contributeurs** — SDK [ReXGlue](https://github.com/rexglue/rexglue-sdk).
- **Ben Vanik et les contributeurs de Xenia** — [Xenia](https://github.com/xenia-project/xenia), base du runtime ReXGlue.
- **boma** — patchs Xenia Canary pour Asura's Wrath (60 FPS, post-traitement),
  qui ont permis de trouver les fonctions à modifier ([xenia-canary/game-patches](https://github.com/xenia-canary/game-patches)).
- **AxR / RETROBYTE / XYZEngine** — trainer Aurora pour Asura's Wrath, dont le
  mode dieu et le Burst instantané ont été reproduits.
- **AMD** — FidelityFX (CAS, FSR).
- **kenney.nl** — éléments de manette du portage Android d'origine.
- **Aymeric (ReActHax)** — ce fork : tests, direction, correctif du plantage au
  démarrage.
- L'essentiel du code et de la documentation de ce fork a été écrit avec l'aide
  de **Claude** (Anthropic), un assistant IA, sous la direction et avec les
  tests d'Aymeric.

---

## Mentions légales

Asura's Wrath est © CAPCOM CO., LTD. Ce projet n'est ni affilié à Capcom,
CyberConnect2 ou Microsoft, ni approuvé par eux. Aucun fichier du jeu n'est
distribué : vous devez posséder le jeu et fournir votre propre image disque et
mise à jour. N'utilisez pas ce projet pour le piratage.

Le projet d'origine n'a pas de fichier de licence ; le SDK ReXGlue et Xenia
sont sous licence BSD 3 clauses (voir `tools/rexglue/LICENSE`). Les
modifications de ce fork sont fournies telles quelles, sans garantie.
