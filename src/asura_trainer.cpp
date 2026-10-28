// asura_trainer.cpp - see asura_trainer.h.
//
// The Aurora trainer (XYZEngine, "Asuras Wrath" by AxR / RETROBYTE, for TU1)
// patches the game in memory. Its three cheats, decoded from the trainer:
//
// God Mode - detour at 0x8229CD20 in sub_8229CD10, the engine routine that
//   copies an int property value (*dest = *src when ArrayDim == 1). When the
//   destination belongs to Asura's health component (object vtable 0x821A28F0
//   at dest-276, tag 0x20702800 at dest-224) and the next field (dest+4, the
//   maximum) is 500, 1500 or 5000, the new value is replaced by the maximum.
//
// Instant Burst - same detour on the float version, sub_8229E2A8 at
//   0x8229E2B8: when the field after the destination (dest+8) is 1000.0, r15
//   is 0x40515700 and the new value is above 1, the value becomes 1000.0.
//
// The trainer's third cheat, Instant Wrath, rewrites the UnrealScript FMin
// native for the whole game (0x82253E68). It is left out on purpose: the wrath
// gauge drives scripted sequences, and changing FMin everywhere is too risky.

#include "asura_trainer.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <set>
#include <utility>

#include "generated/default/asura_wrath_init.h"

#include <rex/hook.h>
#include <rex/logging.h>

namespace {

constexpr AsuraTrainerCheat kCheats[] = {
    {"Mode dieu", "La vie d'Asura reste au maximum."},
    {"Burst instantane", "La jauge de Burst se remplit d'un coup."},
};

// Cheats live only in memory: always off at launch, never written to the toml.
enum : size_t { kGodMode, kInstantBurst, kCheatCount };
std::atomic<bool> g_enabled[kCheatCount];

constexpr uint32_t kHealthOwnerVtable = 0x821A28F0;
constexpr uint32_t kHealthOwnerTag = 0x20702800;
constexpr uint32_t kBurstContext = 0x40515700;
constexpr float kBurstFull = 1000.0f;

bool IsHealthMax(uint32_t value) {
  return value == 500 || value == 1500 || value == 5000;
}

// The trainer identifies the burst copy by a heap address in r15. Heap layout
// can differ from a real console, so log the contexts seen once per run to
// make a mismatch easy to diagnose.
void NoteBurstCandidate(uint32_t lr, uint32_t r15) {
  static std::mutex mutex;
  static std::set<std::pair<uint32_t, uint32_t>> seen;
  std::lock_guard lock(mutex);
  if (seen.size() < 32 && seen.insert({lr, r15}).second) {
    REXLOG_INFO("Asura trainer: burst-like float copy from {:08X} (r15 {:08X}){}", lr, r15,
                r15 == kBurstContext ? " - matches" : "");
  }
}

}  // namespace

// God Mode: int property copy.
REX_HOOK_RAW(sub_8229CD10) {
  if (g_enabled[kGodMode].load(std::memory_order_relaxed) && REX_LOAD_U32(ctx.r3.u32 + 64) == 1) {
    const uint32_t dest = ctx.r4.u32;
    const uint32_t current = REX_LOAD_U32(dest);
    const uint32_t max = REX_LOAD_U32(dest + 4);
    if (current > 10 && max > 10 && IsHealthMax(max) &&
        REX_LOAD_U32(dest - 224) == kHealthOwnerTag &&
        REX_LOAD_U32(dest - 276) == kHealthOwnerVtable) {
      // Same register state as the original fast path.
      REX_STORE_U32(dest, max);
      ctx.r11.u64 = max;
      ctx.r10.u64 = 1;
      ctx.r3.u64 = dest;
      return;
    }
  }
  __imp__sub_8229CD10(ctx, base);
}

// Instant Burst: float property copy.
REX_HOOK_RAW(sub_8229E2A8) {
  if (g_enabled[kInstantBurst].load(std::memory_order_relaxed) && REX_LOAD_U32(ctx.r3.u32 + 64) == 1) {
    const uint32_t dest = ctx.r4.u32;
    PPCRegister next{};
    next.u32 = REX_LOAD_U32(dest + 8);
    if (next.f32 == kBurstFull) {
      const uint32_t lr = static_cast<uint32_t>(ctx.lr);
      NoteBurstCandidate(lr, ctx.r15.u32);
      PPCRegister value{};
      value.u32 = REX_LOAD_U32(ctx.r5.u32);
      if (ctx.r15.u32 == kBurstContext && value.f32 > 1.0f) {
        PPCRegister full{};
        full.f32 = kBurstFull;
        REX_STORE_U32(dest, full.u32);
        ctx.r11.u64 = ctx.r3.u64;
        ctx.r10.u64 = 1;
        ctx.f0.f64 = double(kBurstFull);
        ctx.r3.u64 = dest;
        return;
      }
    }
  }
  __imp__sub_8229E2A8(ctx, base);
}

size_t AsuraTrainer_Count() {
  return std::size(kCheats);
}

const AsuraTrainerCheat& AsuraTrainer_Get(size_t index) {
  return kCheats[index];
}

bool AsuraTrainer_IsEnabled(size_t index) {
  return index < kCheatCount && g_enabled[index].load();
}

void AsuraTrainer_SetEnabled(size_t index, bool enabled) {
  if (index < kCheatCount) {
    g_enabled[index].store(enabled);
    REXLOG_INFO("Asura trainer: {} {}", kCheats[index].label, enabled ? "on" : "off");
  }
}
