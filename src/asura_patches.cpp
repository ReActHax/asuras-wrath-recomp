// asura_patches.cpp - Game-specific enhancements for Asura's Wrath (base XEX,
// Title ID 43430817, xex hash 904B784109438B34).
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
#include <rex/ui/keybinds.h>

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

namespace {

// --- 60 FPS ----------------------------------------------------------------
//
// sub_824D7110 returns the game-speed multiplier used by gameplay code:
//
//   lwz  r11, 848(0x832AC2F8)   ; global setting
//   lfs  f1, 0x8207FB18         ; constant scale
//   beqlr                       ; setting == 0 -> constant
//   lfs  f1, 1224(r3)           ; otherwise the per-object scale (30 FPS mode)
//
// Forcing the constant path is the "Unlock FPS" patch (NOP at 0x824D7130).
constexpr uint32_t kConstantSpeedScale = 0x8207FB18;

// sub_8299A620 builds the swap packet and converts the D3D presentation
// interval stored at device+14000 into the number of vblanks to wait:
// 0/1 -> 1, 2 -> 2, 4 -> 3. The game asks for 2 (30 FPS on a 60 Hz vblank);
// the Xenia patch turns `li r11,2` (0x8299A70C) into `li r11,1`.
constexpr uint32_t kDevicePresentInterval = 14000;
constexpr uint32_t kPresentIntervalTwo = 2;
constexpr uint32_t kPresentIntervalOne = 1;

// --- Post-process ----------------------------------------------------------
//
// sub_82740190 renders one post-process pass. It has exactly five callers,
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
    {0x8273F614, 5, 1},   // 0x8273F610: li r5,1
    {0x8273F770, 5, 0},   // 0x8273F76C: li r5,0
    {0x8273F8DC, 4, 4},   // 0x8273F8D8: li r4,4
    {0x82741ED8, 5, 0},   // 0x82741ED4: li r5,0
    {0x82741EF4, -1, 0},  // 0x82741EF0: nop
};

void ToggleFramerate() {
  const bool enable = !REXCVAR_GET(asura_60fps);
  rex::cvar::SetFlagByName("asura_60fps", enable ? "true" : "false");
  REXLOG_INFO("Asura: framerate target {} FPS", enable ? 60 : 30);
}

}  // namespace

REX_HOOK_RAW(sub_824D7110) {
  __imp__sub_824D7110(ctx, base);
  if (REXCVAR_GET(asura_60fps)) {
    PPCRegister temp{};
    temp.u32 = REX_LOAD_U32(kConstantSpeedScale);
    ctx.f1.f64 = double(temp.f32);
  }
}

REX_HOOK_RAW(sub_8299A620) {
  const uint32_t device = ctx.r3.u32;
  bool overridden = false;
  if (REXCVAR_GET(asura_60fps) && device != 0 &&
      REX_LOAD_U32(device + kDevicePresentInterval) == kPresentIntervalTwo) {
    // Only this function and two getters ever read the field; it is restored
    // as soon as the swap packet is built.
    REX_STORE_U32(device + kDevicePresentInterval, kPresentIntervalOne);
    overridden = true;
  }
  __imp__sub_8299A620(ctx, base);
  if (overridden) {
    REX_STORE_U32(device + kDevicePresentInterval, kPresentIntervalTwo);
  }
}

REX_HOOK_RAW(sub_82740190) {
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
  __imp__sub_82740190(ctx, base);
}

void AsuraPatches_RegisterBinds() {
  rex::ui::RegisterBind("bind_asura_toggle_fps", "F6", "Toggle 30/60 FPS (Asura's Wrath)",
                        ToggleFramerate);
}

void AsuraPatches_UnregisterBinds() {
  rex::ui::UnregisterBind("bind_asura_toggle_fps");
}
