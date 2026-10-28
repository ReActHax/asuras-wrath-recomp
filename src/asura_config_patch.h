#pragma once
// asura_config_patch - optional graphics effects for Asura's Wrath.
//
// The game reads its Unreal Engine 3 settings from CookedXbox360\Coalesced_*.bin
// ([SystemSettings] of Xbox360-BCEngine.ini). When an asura_fx_* option differs
// from the original, a patched copy of each Coalesced file is written once to
// CookedXbox360\AsuraPatched and the game's open is redirected to it. The
// originals are never modified.

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// Call before the game data folder is mounted (writes the patched copies).
void AsuraConfig_Prepare(const std::filesystem::path& game_data_root);

// File-open redirection for the patched Coalesced files (nullopt = untouched).
std::optional<std::string> AsuraConfig_Redirect(std::string_view guest_path);

// Summary for the log, once logging is up.
void AsuraConfig_LogStatus();
