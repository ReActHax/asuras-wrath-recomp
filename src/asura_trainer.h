#pragma once
// asura_trainer - two of the three cheats of the RETROBYTE / XYZEngine Aurora trainer
// for Asura's Wrath TU1, reimplemented as recompilation hooks. Toggled from the
// cheat menu only (F9); always off at launch, nothing is saved.

#include <cstddef>

struct AsuraTrainerCheat {
  const char* label;        // shown in the cheat menu
  const char* description;  // tooltip
};

size_t AsuraTrainer_Count();
const AsuraTrainerCheat& AsuraTrainer_Get(size_t index);
bool AsuraTrainer_IsEnabled(size_t index);
void AsuraTrainer_SetEnabled(size_t index, bool enabled);
