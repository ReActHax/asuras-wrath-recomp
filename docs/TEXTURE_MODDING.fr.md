# Remplacement de textures (textures HD)

[English](TEXTURE_MODDING.md)

La recompilation peut remplacer n'importe quelle texture 2D du jeu par un
fichier DDS, de n'importe quelle taille (par exemple ×2 ou ×4 pour des
textures HD). Aucune texture HD n'est fournie : ce document explique comment
en créer.

Fonctionne avec les rendus **D3D12** et **Vulkan** (`gpu_backend` dans le
toml). Les identifiants sont les mêmes pour les formats courants (DXT/BC,
RGBA 8 bits) : un même dossier `replace\` sert aux deux.

## Dossiers

Dans le dossier de données utilisateur
(`Documents\asura_wrath_recomp\textures\` par défaut) :

| Dossier | Rôle |
|---|---|
| `dump\` | textures extraites du jeu (si `texture_dump = true`) |
| `replace\` | vos textures de remplacement (sous-dossiers autorisés) |

## Réglages

Dans le launcher, onglet **Textures**, ou dans `asura_wrath_recomp.toml` :

```toml
texture_replacement = true   # utiliser les fichiers de replace\
texture_dump = false         # extraire les textures dans dump\
```

## Méthode

1. **Extraire.** Activez « Extraire les textures du jeu » (`texture_dump = true`),
   lancez le jeu et passez par les scènes voulues : une texture n'est extraite
   qu'au moment où le jeu la charge. Chaque texture rencontrée est écrite une seule fois dans
   `dump\`, sous le nom `<identifiant>_<largeur>x<hauteur>_<format>.dds`.
   L'extraction ralentit le jeu : remettez `false` ensuite. Les images des
   vidéos ne sont pas extraites.
2. **Modifier.** Ouvrez le DDS dans un éditeur qui gère ce format
   (Paint.NET, GIMP, Photoshop avec le greffon Intel, `texconv`…). Agrandissez,
   retouchez, gardez la transparence (canal alpha) s'il y en a une.
3. **Enregistrer** dans `replace\`. Le nom doit **commencer par les 16
   caractères de l'identifiant** ; la suite est libre
   (`6EB3A07D1A6EEEA6_logo_4k.dds` convient).
4. **Relancer le jeu** : la liste des fichiers est lue au démarrage.
   Le journal (`logs\`) affiche `Texture replacement: N DDS file(s)` puis une
   ligne par texture remplacée.

## Formats acceptés

- Compressés : BC1 (DXT1), BC2 (DXT3), BC3 (DXT5), BC4 (ATI1), BC5 (ATI2), BC7.
- Non compressés : RGBA ou BGRA 32 bits.
- En-têtes DDS classiques ou DX10. Les variantes sRGB sont lues comme des
  textures normales (le jeu gère lui-même le gamma).
- Taille libre (multiple de 4 pour les formats compressés), niveaux de détail
  (mipmaps) libres : **générez-les** pour éviter le scintillement au loin.
- Une seule texture 2D par fichier (pas de cubemap ni de tableau).

Pour garder la mémoire vidéo raisonnable, préférez BC1 (sans transparence) ou
BC3/BC7 (avec transparence) au RGBA non compressé.

## Bon à savoir

- L'identifiant est un hachage du contenu de la texture d'origine (niveau de
  base), de sa taille et de son format : une texture identique utilisée à
  plusieurs endroits n'a besoin que d'un seul fichier.
- Quand le jeu recopie une texture par la carte graphique (après certains
  chapitres), l'original peut apparaître pendant une image avant le
  remplacement.
- Non pris en charge : cubemaps, textures 3D ou en tableau, images rendues par
  le jeu (reflets, écrans), vidéos.
- Ne distribuez pas les textures extraites du jeu telles quelles : elles
  appartiennent à leurs ayants droit.

## Détails techniques

Code : `tools/rexglue/src/graphics/pipeline/texture/replacement.cpp` (logique
commune : DDS, identifiants, index des fichiers, dumps) et la fin de
`d3d12/texture_cache.cpp` / `vulkan/texture_cache.cpp` (création des textures,
relecture GPU, échange).
Identifiant = `XXH3_64bits_withSeed(données du niveau de base telles que
chargées sur le GPU, graine = (format Xenos << 32) | (largeur << 16) | hauteur)`,
soit exactement les données du DDS extrait (après l'en-tête). Le remplacement
échange la ressource (D3D12) ou l'image (Vulkan) de la texture et conserve l'originale pour revenir
en arrière si le contenu change.
