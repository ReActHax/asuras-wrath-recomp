#pragma once

// Game-specific enhancement hooks live in asura_patches.cpp; these register
// the hotkeys that go with them.
#include <filesystem>

// Call before the game data folder is mounted (writes the shortened logos).
void AsuraPatches_PrepareGameData(const std::filesystem::path& game_data_root);
// Call once before the guest starts (installs the file redirection).
void AsuraPatches_Setup();
void AsuraPatches_RegisterBinds();
void AsuraPatches_UnregisterBinds();
