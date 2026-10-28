# Texture replacement (HD textures)

[Français](TEXTURE_MODDING.fr.md)

The recompilation can replace any 2D texture of the game with a DDS file of any
size (for example 2× or 4× for HD textures). No HD texture is included: this
document explains how to make them.

Works with both the **D3D12** and **Vulkan** renderers (`gpu_backend`). The
identifiers are the same for the common formats (DXT/BC, 8-bit RGBA), so one
`replace\` folder works for both.

## Folders

In the user data folder (`Documents\asura_wrath_recomp\textures\` by default):

| Folder | Purpose |
|---|---|
| `dump\` | textures extracted from the game (when `texture_dump = true`) |
| `replace\` | your replacement textures (subfolders allowed) |

## Settings

In the launcher, **Textures** tab, or in `asura_wrath_recomp.toml`:

```toml
texture_replacement = true   # use the files in replace\
texture_dump = false         # extract textures to dump\
```

## How to

1. **Dump.** Enable "Dump the game textures" (`texture_dump = true`), start the
   game and go through the scenes you want: a texture is only dumped when the
   game loads it. Each texture is written once to `dump\` as
   `<identifier>_<width>x<height>_<format>.dds`. Dumping slows the game down:
   turn it off afterwards. Video frames are not dumped.
2. **Edit.** Open the DDS in an editor that supports the format (Paint.NET,
   GIMP, Photoshop with the Intel plugin, `texconv`…). Upscale, retouch, keep
   the transparency (alpha channel) if there is one.
3. **Save** it to `replace\`. The file name must **start with the 16-character
   identifier**; the rest is free (`6EB3A07D1A6EEEA6_logo_4k.dds` works).
4. **Restart the game**: the list of files is read at startup. The log
   (`logs\`) shows `Texture replacement: N DDS file(s)` and then one line per
   replaced texture.

## Supported formats

- Compressed: BC1 (DXT1), BC2 (DXT3), BC3 (DXT5), BC4 (ATI1), BC5 (ATI2), BC7.
- Uncompressed: 32-bit RGBA or BGRA.
- Legacy or DX10 DDS headers. sRGB variants are read as regular textures (the
  game handles gamma itself).
- Any size (a multiple of 4 for compressed formats) and any number of mip
  levels: **generate mipmaps** to avoid shimmering in the distance.
- One 2D texture per file (no cube map or array).

To keep video memory usage reasonable, prefer BC1 (no transparency) or BC3/BC7
(with transparency) over uncompressed RGBA.

## Good to know

- The identifier is a hash of the original texture content (base level), its
  size and its format: the same texture used in several places needs a single
  file.
- When the game copies a texture with the GPU (after some episodes), the
  original may show for one frame before being replaced.
- Not supported: cube maps, 3D or array textures, images rendered by the game
  (reflections, screens), videos.
- Do not distribute the dumped game textures as is: they belong to their
  rights holders.

## Technical details

Code: `tools/rexglue/src/graphics/pipeline/texture/replacement.cpp` (shared
logic: DDS, identifiers, file index, dumps) and the end of
`d3d12/texture_cache.cpp` / `vulkan/texture_cache.cpp` (texture creation, GPU
readback, swapping).

Identifier = `XXH3_64bits_withSeed(base level data as loaded on the GPU,
seed = (Xenos format << 32) | (width << 16) | height)`, i.e. exactly the data of
the dumped DDS (after the header). The guest memory is hashed first for an
immediate match; if it doesn't match (the data may have been written by the
GPU), the loaded texture is read back and the replacement is applied on the
next frame. Replacing swaps the texture's D3D12 resource or Vulkan image and
keeps the original to switch back if the content changes.
