#pragma once
// asura_cheats - in-game cheat menu (default key F9).
//
// A small Cheat Engine: search guest memory for a value, narrow the results
// as the value changes in game, then add addresses to a cheat list where each
// entry can be edited or frozen on the fly. The list is saved to
// asura_cheats.txt in the user data folder. Heap addresses usually change
// when a level is (re)loaded: entries then need to be searched again.

#include <filesystem>

namespace rex::ui {
class ImGuiDrawer;
}

void AsuraCheats_Init(const std::filesystem::path& user_data_root);
void AsuraCheats_RegisterBinds(rex::ui::ImGuiDrawer* drawer);
void AsuraCheats_Shutdown();
// True while the menu is open (the app then lets ImGui keep the mouse).
bool AsuraCheats_IsMenuOpen();
