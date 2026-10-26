// asura_patches.cpp - Game-specific enhancements for Asura's Wrath (TITLE UPDATE 1 XEX v0.0.1.5;
// addresses below were remapped from the base XEX: hooks +0x98/+0x10/+0x330,
// Title ID 43430817, base xex hash 904B784109438B34, call sites +0x330).
//
// Every patch is a hook on a recompiled guest function. The generated code
// defines each sub_XXXXXXXX as a weak alias of __imp__sub_XXXXXXXX, so a strong
// definition here replaces it everywhere (direct calls and the indirect-call
// table) while __imp__ stays reachable as the original. Nothing under
// generated/ is edited, so regenerating the recompilation keeps these patches.
//
// All options are live cvars: they show up in the settings overlay (F4),
// can be passed on the command line (--asura_60fps) and are saved in
// asura_wrath_recomp.toml next to the executable.
//
// Addresses come from the Xenia Canary game patch for this title
// (patches/43430817 - Asura's Wrath.patch.toml, author: boma) and were
// re-checked against the recompiled code.

#include "generated/default/asura_wrath_init.h"

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/file_open_filter.h>
#include <rex/ui/keybinds.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "asura_patches.h"

REXCVAR_DEFINE_BOOL(asura_60fps, false, "Asura",
                    "Run the game at 60 FPS instead of 30. Gameplay speed stays correct; "
                    "button-mashing QTEs count presses per frame, so if one feels harder, "
                    "toggle back to 30 FPS with the hotkey (default F6) for that sequence.")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(asura_disable_postprocess, false, "Asura",
                    "Skip the game's post-process passes. Removes the edge-detection "
                    "over-correction and color smearing that appear with resolution_scale > 1, "
                    "at the cost of the original look.")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(asura_skip_logos, false, "Asura",
                    "Skip the studio logo movies at startup (Bink, SpeedTree, Unreal Engine 3, "
                    "Capcom, CyberConnect2). Takes effect at the next launch.")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace {

// --- 60 FPS ----------------------------------------------------------------
//
// sub_824D71A8 returns the game-speed multiplier used by gameplay code:
//
//   lwz  r11, 848(0x832AC2F8)   ; global setting
//   lfs  f1, 0x8207FB18         ; constant scale
//   beqlr                       ; setting == 0 -> constant
//   lfs  f1, 1224(r3)           ; otherwise the per-object scale (30 FPS mode)
//
// Forcing the constant path is the "Unlock FPS" patch (NOP at 0x824D7130).
constexpr uint32_t kConstantSpeedScale = 0x8207FB18;

// sub_8299A630 builds the swap packet and converts the D3D presentation
// interval stored at device+14000 into the number of vblanks to wait:
// 0/1 -> 1, 2 -> 2, 4 -> 3. The game asks for 2 (30 FPS on a 60 Hz vblank);
// the Xenia patch turns `li r11,2` (0x8299A70C) into `li r11,1`.
constexpr uint32_t kDevicePresentInterval = 14000;
constexpr uint32_t kPresentIntervalTwo = 2;
constexpr uint32_t kPresentIntervalOne = 1;

// --- Post-process ----------------------------------------------------------
//
// sub_827404C0 renders one post-process pass. It has exactly five callers,
// which are the five sites the Xenia "Resolution Scaling Fix" patch
// overwrites. The patch replaces each `bl` with the instruction below, so the
// pass is skipped and only that register changes; the hook reproduces it per
// call site (identified by the return address in LR).
struct SkippedPass {
  uint32_t return_address;
  int reg;  // 4 or 5, -1 = nop
  uint32_t value;
};
constexpr SkippedPass kSkippedPasses[] = {
    {0x8273F944, 5, 1},   // 0x8273F940: li r5,1
    {0x8273FAA0, 5, 0},   // 0x8273FA9C: li r5,0
    {0x8273FC0C, 4, 4},   // 0x8273FC08: li r4,4
    {0x82742208, 5, 0},   // 0x82742204: li r5,0
    {0x82742224, -1, 0},  // 0x82742220: nop
};

// --- Intro movies ------------------------------------------------------------
//
// The startup sequence plays the studio logos as Bink movies from
// D:\BCGame\Movies, and the game script waits for each one to finish: hiding
// a logo leaves it on a black screen. Instead, a one-frame copy of each logo
// is written once to BCGame\Movies\AsuraShortLogos and the game's open is
// redirected to it, so every logo "plays" for a single frame. The original
// files are never modified and the option can be switched at will.
constexpr std::string_view kLogoMovies[] = {
    "20_binklogo", "20_speedtreelogo", "20_ue3logo", "40_capcomlogo", "40_cc2logo",
};
constexpr std::string_view kShortLogoDir = "AsuraShortLogos";

bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), text.begin(), [](char expected, char actual) {
           // std::equal passes (prefix char, text char); prefixes are lowercase.
           return std::tolower(static_cast<unsigned char>(actual)) == expected;
         });
}

bool EndsWithNoCase(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() &&
         StartsWithNoCase(text.substr(text.size() - suffix.size()), suffix);
}

uint32_t ReadLE32(const std::vector<char>& data, size_t offset) {
  uint32_t value = 0;
  std::memcpy(&value, data.data() + offset, sizeof(value));
  return value;  // Bink headers are little-endian, like the host.
}

void WriteLE32(std::vector<char>& data, size_t offset, uint32_t value) {
  std::memcpy(data.data() + offset, &value, sizeof(value));
}

// Bink 1 layout: 'BIK?' | size-8 | frames | largest frame | frames | width |
// height | fps num | fps den | flags | audio tracks (0 for the logos) | frame
// offset table (frames + 1 entries, bit 0 = keyframe) | frame data.
// Keeping the whole header and table but declaring a single frame and cutting
// the file after it gives a valid one-frame movie.
bool WriteOneFrameCopy(const std::filesystem::path& source, const std::filesystem::path& target) {
  std::ifstream in(source, std::ios::binary);
  std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  constexpr size_t kTableOffset = 44;
  if (data.size() < kTableOffset + 8 || std::memcmp(data.data(), "BIK", 3) != 0 ||
      ReadLE32(data, 40) != 0) {
    return false;
  }
  const uint32_t frame_start = ReadLE32(data, kTableOffset) & ~1u;
  const uint32_t frame_end = ReadLE32(data, kTableOffset + 4) & ~1u;
  if (frame_start <= kTableOffset || frame_end <= frame_start || frame_end > data.size()) {
    return false;
  }
  data.resize(frame_end);
  WriteLE32(data, 4, frame_end - 8);              // file size - 8
  WriteLE32(data, 8, 1);                          // frame count
  WriteLE32(data, 12, frame_end - frame_start);   // largest frame (decoders check it)
  WriteLE32(data, 16, 1);
  std::ofstream out(target, std::ios::binary | std::ios::trunc);
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  return out.good();
}

// Lowercase names of the logo movies that have a one-frame copy on disk.
std::vector<std::string> g_short_logos;
size_t g_short_logos_failed = 0;

std::string ToLower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

// Runs before the game data folder is mounted: the virtual filesystem lists
// the folder when it mounts it, so files created later would stay invisible to
// the game.
void PrepareShortLogos(const std::filesystem::path& game_root) {
  g_short_logos.clear();
  g_short_logos_failed = 0;
  std::error_code ec;
  const auto movies = std::filesystem::absolute(game_root, ec) / "BCGame" / "Movies";
  const auto short_dir = movies / std::string(kShortLogoDir);
  std::filesystem::create_directories(short_dir, ec);
  for (const auto& entry : std::filesystem::directory_iterator(movies, ec)) {
    const auto name = entry.path().filename().string();
    if (!entry.is_regular_file() || !EndsWithNoCase(name, ".bik") ||
        std::none_of(std::begin(kLogoMovies), std::end(kLogoMovies),
                     [&](std::string_view p) { return StartsWithNoCase(name, p); })) {
      continue;
    }
    const auto target = short_dir / name;
    if (std::filesystem::exists(target, ec) || WriteOneFrameCopy(entry.path(), target)) {
      g_short_logos.push_back(ToLower(name));
    } else {
      g_short_logos_failed++;
    }
  }
}

std::optional<std::string> ShortenIntroLogo(std::string_view guest_path) {
  if (g_short_logos.empty() || !REXCVAR_GET(asura_skip_logos)) {
    return std::nullopt;
  }
  const auto slash = guest_path.find_last_of("\\/");
  if (slash == std::string_view::npos) {
    return std::nullopt;
  }
  const auto directory = guest_path.substr(0, slash);
  const auto name = guest_path.substr(slash + 1);
  // Only names with a copy are redirected; the localized probes the game makes
  // first (40_CAPCOMLogo_FRA.bik) do not exist and must keep failing normally.
  if (!EndsWithNoCase(directory, "movies") ||
      std::find(g_short_logos.begin(), g_short_logos.end(), ToLower(name)) ==
          g_short_logos.end()) {
    return std::nullopt;
  }
  REXLOG_INFO("Asura: skipping logo movie {}", name);
  return std::string(directory) + "\\" + std::string(kShortLogoDir) + "\\" + std::string(name);
}

void ToggleFramerate() {
  const bool enable = !REXCVAR_GET(asura_60fps);
  rex::cvar::SetFlagByName("asura_60fps", enable ? "true" : "false");
  REXLOG_INFO("Asura: framerate target {} FPS", enable ? 60 : 30);
}

}  // namespace

REX_HOOK_RAW(sub_824D71A8) {
  __imp__sub_824D71A8(ctx, base);
  if (REXCVAR_GET(asura_60fps)) {
    PPCRegister temp{};
    temp.u32 = REX_LOAD_U32(kConstantSpeedScale);
    ctx.f1.f64 = double(temp.f32);
  }
}

REX_HOOK_RAW(sub_8299A630) {
  const uint32_t device = ctx.r3.u32;
  bool overridden = false;
  if (REXCVAR_GET(asura_60fps) && device != 0 &&
      REX_LOAD_U32(device + kDevicePresentInterval) == kPresentIntervalTwo) {
    // Only this function and two getters ever read the field; it is restored
    // as soon as the swap packet is built.
    REX_STORE_U32(device + kDevicePresentInterval, kPresentIntervalOne);
    overridden = true;
  }
  __imp__sub_8299A630(ctx, base);
  if (overridden) {
    REX_STORE_U32(device + kDevicePresentInterval, kPresentIntervalTwo);
  }
}

REX_HOOK_RAW(sub_827404C0) {
  if (REXCVAR_GET(asura_disable_postprocess)) {
    const uint32_t lr = static_cast<uint32_t>(ctx.lr);
    for (const auto& pass : kSkippedPasses) {
      if (pass.return_address != lr) {
        continue;
      }
      if (pass.reg == 4) {
        ctx.r4.u64 = pass.value;
      } else if (pass.reg == 5) {
        ctx.r5.u64 = pass.value;
      }
      return;
    }
  }
  __imp__sub_827404C0(ctx, base);
}

void AsuraPatches_PrepareGameData(const std::filesystem::path& game_data_root) {
  PrepareShortLogos(game_data_root);
}

void AsuraPatches_Setup() {
  REXLOG_INFO("Asura: {} shortened logo movies ready{}", g_short_logos.size(),
              g_short_logos_failed ? " (some could not be written)" : "");
  rex::system::SetFileOpenFilter(ShortenIntroLogo);
}

void AsuraPatches_RegisterBinds() {
  rex::ui::RegisterBind("bind_asura_toggle_fps", "F6", "Toggle 30/60 FPS (Asura's Wrath)",
                        ToggleFramerate);
}

void AsuraPatches_UnregisterBinds() {
  rex::system::SetFileOpenFilter(nullptr);
  rex::ui::UnregisterBind("bind_asura_toggle_fps");
}
