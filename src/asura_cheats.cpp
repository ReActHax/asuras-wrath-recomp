// asura_cheats.cpp - see asura_cheats.h.

#include "asura_cheats.h"
#include "asura_trainer.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <imgui.h>

#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/keybinds.h>

namespace {

using rex::memory::BaseHeap;
using rex::memory::HeapAllocationInfo;
using rex::memory::Memory;

// --- Guest memory access -----------------------------------------------------

enum class ValueType : int { kInt32 = 0, kFloat, kInt16, kByte };
constexpr const char* kTypeNames[] = {"Entier 32 bits", "Flottant", "Entier 16 bits", "Octet"};
constexpr const char* kTypeTags[] = {"i32", "f32", "i16", "u8"};

uint32_t TypeSize(ValueType type) {
  switch (type) {
    case ValueType::kInt32:
    case ValueType::kFloat:
      return 4;
    case ValueType::kInt16:
      return 2;
    case ValueType::kByte:
      return 1;
  }
  return 4;
}

Memory* GuestMemory() {
  auto* runtime = rex::Runtime::instance();
  return runtime ? runtime->memory() : nullptr;
}

// True when [address, address + size) is committed, readable and writable
// guest memory (writing elsewhere would fault the host process).
bool IsAccessible(Memory* memory, uint32_t address, uint32_t size) {
  if (!memory || address < 0x00010000) {
    return false;
  }
  auto* heap = memory->LookupHeap(address);
  if (!heap) {
    return false;
  }
  HeapAllocationInfo info;
  if (!heap->QueryRegionInfo(address & ~0xFFFu, &info)) {
    return false;
  }
  const uint32_t needed = rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite;
  return (info.state & rex::memory::kMemoryAllocationCommit) && (info.protect & needed) == needed &&
         uint64_t(address) + size <= uint64_t(info.base_address) + info.region_size;
}

// Raw value as stored by the guest (big-endian), widened to 32 bits.
uint32_t ReadRaw(const uint8_t* p, ValueType type) {
  switch (type) {
    case ValueType::kInt32:
    case ValueType::kFloat:
      return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
    case ValueType::kInt16:
      return (uint32_t(p[0]) << 8) | p[1];
    case ValueType::kByte:
      return p[0];
  }
  return 0;
}

void WriteRaw(uint8_t* p, ValueType type, uint32_t raw) {
  switch (type) {
    case ValueType::kInt32:
    case ValueType::kFloat:
      p[0] = uint8_t(raw >> 24);
      p[1] = uint8_t(raw >> 16);
      p[2] = uint8_t(raw >> 8);
      p[3] = uint8_t(raw);
      break;
    case ValueType::kInt16:
      p[0] = uint8_t(raw >> 8);
      p[1] = uint8_t(raw);
      break;
    case ValueType::kByte:
      p[0] = uint8_t(raw);
      break;
  }
}

double RawToNumber(uint32_t raw, ValueType type) {
  switch (type) {
    case ValueType::kInt32:
      return double(int32_t(raw));
    case ValueType::kFloat:
      return double(std::bit_cast<float>(raw));
    case ValueType::kInt16:
      return double(int16_t(uint16_t(raw)));
    case ValueType::kByte:
      return double(uint8_t(raw));
  }
  return 0;
}

uint32_t NumberToRaw(double value, ValueType type) {
  switch (type) {
    case ValueType::kInt32:
      return uint32_t(int32_t(std::llround(value)));
    case ValueType::kFloat:
      return std::bit_cast<uint32_t>(float(value));
    case ValueType::kInt16:
      return uint16_t(int16_t(std::llround(value)));
    case ValueType::kByte:
      return uint8_t(std::llround(value));
  }
  return 0;
}

std::string FormatValue(uint32_t raw, ValueType type) {
  char text[64];
  if (type == ValueType::kFloat) {
    std::snprintf(text, sizeof(text), "%.4g", RawToNumber(raw, type));
  } else {
    std::snprintf(text, sizeof(text), "%.0f", RawToNumber(raw, type));
  }
  return text;
}

bool ReadValue(Memory* memory, uint32_t address, ValueType type, uint32_t* out) {
  if (!IsAccessible(memory, address, TypeSize(type))) {
    return false;
  }
  *out = ReadRaw(memory->TranslateVirtual<uint8_t*>(address), type);
  return true;
}

bool WriteValue(Memory* memory, uint32_t address, ValueType type, uint32_t raw) {
  if (!IsAccessible(memory, address, TypeSize(type))) {
    return false;
  }
  WriteRaw(memory->TranslateVirtual<uint8_t*>(address), type, raw);
  return true;
}

struct Region {
  uint32_t base;
  uint32_t size;
};

// Committed read/write guest memory where the game keeps its data: the
// virtual heap and the three physical views. The XEX image is left out.
std::vector<Region> WritableRegions(Memory* memory) {
  static constexpr std::pair<uint64_t, uint64_t> kRanges[] = {
      {0x40000000, 0x80000000}, {0xA0000000, 0xC0000000},
      {0xC0000000, 0xE0000000}, {0xE0000000, 0x100000000}};
  std::vector<Region> regions;
  for (const auto& [start, end] : kRanges) {
    uint64_t address = start;
    while (address < end) {
      auto* heap = memory->LookupHeap(uint32_t(address));
      HeapAllocationInfo info{};
      if (!heap || !heap->QueryRegionInfo(uint32_t(address), &info) || info.region_size == 0) {
        address += 0x10000;
        continue;
      }
      const uint32_t needed = rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite;
      if ((info.state & rex::memory::kMemoryAllocationCommit) && (info.protect & needed) == needed) {
        const uint64_t region_end = std::min<uint64_t>(end, uint64_t(address) + info.region_size);
        regions.push_back({uint32_t(address), uint32_t(region_end - address)});
      }
      address += info.region_size;
    }
  }
  return regions;
}

// --- Search ------------------------------------------------------------------

enum class Compare : int { kEqual = 0, kChanged, kUnchanged, kIncreased, kDecreased };
constexpr const char* kCompareNames[] = {"= valeur", "a change", "n'a pas change", "a augmente",
                                         "a diminue"};

bool NumbersEqual(double a, double b, ValueType type) {
  if (type != ValueType::kFloat) {
    return a == b;
  }
  if (!std::isfinite(a)) {
    return false;
  }
  return std::fabs(a - b) <= std::max(0.0005, std::fabs(b) * 1e-4);
}

constexpr size_t kMaxResults = 10'000'000;

struct Search {
  ValueType type = ValueType::kInt32;
  bool truncated = false;
  std::vector<uint32_t> addresses;
  std::vector<uint32_t> values;  // raw value at the last scan
  bool active = false;
  size_t scanned_bytes = 0;

  void Reset() {
    addresses.clear();
    values.clear();
    active = false;
    truncated = false;
    scanned_bytes = 0;
  }

  void First(Memory* memory, ValueType new_type, double target) {
    Reset();
    type = new_type;
    const uint32_t size = TypeSize(type);
    for (const auto& region : WritableRegions(memory)) {
      const uint8_t* base = memory->TranslateVirtual<const uint8_t*>(region.base);
      scanned_bytes += region.size;
      for (uint32_t offset = 0; offset + size <= region.size; offset += size) {
        const uint32_t raw = ReadRaw(base + offset, type);
        if (NumbersEqual(RawToNumber(raw, type), target, type)) {
          if (addresses.size() >= kMaxResults) {
            truncated = true;
            break;
          }
          addresses.push_back(region.base + offset);
          values.push_back(raw);
        }
      }
      if (truncated) {
        break;
      }
    }
    active = true;
  }

  void Next(Memory* memory, Compare compare, double target) {
    // Addresses are sorted (regions are scanned in order), so walk the
    // current committed regions alongside them instead of querying each one.
    const auto regions = WritableRegions(memory);
    const uint32_t size = TypeSize(type);
    size_t region = 0;
    size_t kept = 0;
    for (size_t i = 0; i < addresses.size(); ++i) {
      const uint32_t address = addresses[i];
      while (region < regions.size() &&
             uint64_t(regions[region].base) + regions[region].size <= address) {
        ++region;
      }
      if (region == regions.size() || address < regions[region].base ||
          uint64_t(address) + size > uint64_t(regions[region].base) + regions[region].size) {
        continue;  // memory released since the last scan
      }
      const uint32_t raw = ReadRaw(memory->TranslateVirtual<const uint8_t*>(address), type);
      const double now = RawToNumber(raw, type);
      const double before = RawToNumber(values[i], type);
      bool keep = false;
      switch (compare) {
        case Compare::kEqual:
          keep = NumbersEqual(now, target, type);
          break;
        case Compare::kChanged:
          keep = raw != values[i];
          break;
        case Compare::kUnchanged:
          keep = raw == values[i];
          break;
        case Compare::kIncreased:
          keep = now > before;
          break;
        case Compare::kDecreased:
          keep = now < before;
          break;
      }
      if (keep) {
        addresses[kept] = address;
        values[kept] = raw;
        ++kept;
      }
    }
    addresses.resize(kept);
    values.resize(kept);
  }
};

// --- Cheat list ----------------------------------------------------------------

struct Cheat {
  std::string name;
  ValueType type = ValueType::kInt32;
  uint32_t address = 0;
  uint32_t frozen_raw = 0;
  bool frozen = false;
};

class CheatList {
 public:
  std::mutex mutex;
  std::vector<Cheat> cheats;

  void Load(const std::filesystem::path& path) {
    std::lock_guard lock(mutex);
    cheats.clear();
    std::ifstream in(path);
    std::string line;
    // Format, one cheat per line: type|address hex|value|name
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::stringstream fields(line);
      std::string tag, address, value, name;
      if (!std::getline(fields, tag, '|') || !std::getline(fields, address, '|') ||
          !std::getline(fields, value, '|')) {
        continue;
      }
      std::getline(fields, name);
      Cheat cheat;
      for (int t = 0; t < 4; ++t) {
        if (tag == kTypeTags[t]) {
          cheat.type = ValueType(t);
        }
      }
      try {
        cheat.address = uint32_t(std::stoul(address, nullptr, 16));
        cheat.frozen_raw = NumberToRaw(std::stod(value), cheat.type);
      } catch (...) {
        continue;
      }
      cheat.name = name;
      cheats.push_back(cheat);
    }
  }
};

CheatList g_list;
std::filesystem::path g_save_path;
std::atomic<bool> g_freeze_running{false};
std::thread g_freeze_thread;

// Rewrites frozen values ~100 times per second.
void FreezeLoop() {
  while (g_freeze_running.load()) {
    if (auto* memory = GuestMemory()) {
      std::lock_guard lock(g_list.mutex);
      for (const auto& cheat : g_list.cheats) {
        if (cheat.frozen) {
          WriteValue(memory, cheat.address, cheat.type, cheat.frozen_raw);
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

// --- Menu ------------------------------------------------------------------------

class CheatMenu : public rex::ui::ImGuiDialog {
 public:
  explicit CheatMenu(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    (void)io;
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Cheats (F9)##asura", nullptr, ImGuiWindowFlags_NoCollapse)) {
      ImGui::End();
      return;
    }
    Memory* memory = GuestMemory();
    if (!memory) {
      ImGui::TextUnformatted("Le jeu n'est pas encore lance.");
      ImGui::End();
      return;
    }
    DrawTrainer();
    ImGui::Separator();
    DrawCheatList(memory);
    ImGui::Separator();
    DrawSearch(memory);
    ImGui::End();
  }

 private:
  void DrawTrainer() {
    ImGui::TextUnformatted("Cheats du trainer (actifs immediatement)");
    for (size_t i = 0; i < AsuraTrainer_Count(); ++i) {
      const auto& cheat = AsuraTrainer_Get(i);
      bool enabled = AsuraTrainer_IsEnabled(i);
      if (ImGui::Checkbox(cheat.label, &enabled)) {
        AsuraTrainer_SetEnabled(i, enabled);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", cheat.description);
      }
      if (i + 1 < AsuraTrainer_Count()) {
        ImGui::SameLine();
      }
    }
  }

  void DrawCheatList(Memory* memory) {
    ImGui::TextUnformatted("Mes cheats");
    std::lock_guard lock(g_list.mutex);
    int remove = -1;
    if (ImGui::BeginTable("cheats", 5,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Bloquer", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Nom");
      ImGui::TableSetupColumn("Adresse", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableSetupColumn("Valeur", ImGuiTableColumnFlags_WidthFixed, 110);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24);
      ImGui::TableHeadersRow();
      for (size_t i = 0; i < g_list.cheats.size(); ++i) {
        Cheat& cheat = g_list.cheats[i];
        ImGui::PushID(int(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Checkbox("##frozen", &cheat.frozen);
        ImGui::TableNextColumn();
        char name[64];
        std::snprintf(name, sizeof(name), "%s", cheat.name.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##name", name, sizeof(name))) {
          cheat.name = name;
        }
        ImGui::TableNextColumn();
        ImGui::Text("%08X", cheat.address);
        ImGui::TableNextColumn();
        uint32_t current = cheat.frozen_raw;
        const bool readable = ReadValue(memory, cheat.address, cheat.type, &current);
        if (!cheat.frozen && readable) {
          cheat.frozen_raw = current;  // follow the game until frozen
        }
        double value = RawToNumber(cheat.frozen ? cheat.frozen_raw : current, cheat.type);
        ImGui::SetNextItemWidth(-1);
        if (!readable) {
          ImGui::TextDisabled("invalide");
        } else if (ImGui::InputDouble("##value", &value, 0, 0,
                                      cheat.type == ValueType::kFloat ? "%.4g" : "%.0f",
                                      ImGuiInputTextFlags_EnterReturnsTrue)) {
          cheat.frozen_raw = NumberToRaw(value, cheat.type);
          WriteValue(memory, cheat.address, cheat.type, cheat.frozen_raw);
        }
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("x")) {
          remove = int(i);
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    if (remove >= 0) {
      g_list.cheats.erase(g_list.cheats.begin() + remove);
    }
    if (g_list.cheats.empty()) {
      ImGui::TextDisabled("Vide : trouve une valeur ci-dessous puis \"Ajouter\".");
    }
    ImGui::TextDisabled("Valeur : Entree pour l'ecrire. Bloquer : la garde fixe en jeu.");
    if (ImGui::Button("Enregistrer")) {
      SaveUnlocked();
    }
    ImGui::SameLine();
    if (ImGui::Button("Recharger")) {
      reload_pending_ = true;
    }
    if (reload_pending_) {
      reload_pending_ = false;
      LoadUnlocked();
    }
  }

  void DrawSearch(Memory* memory) {
    ImGui::TextUnformatted("Recherche en memoire");
    int type = int(search_.active ? search_.type : pending_type_);
    ImGui::SetNextItemWidth(160);
    if (search_.active) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Combo("Type", &type, kTypeNames, 4)) {
      pending_type_ = ValueType(type);
    }
    if (search_.active) {
      ImGui::EndDisabled();
    }
    ImGui::SetNextItemWidth(160);
    ImGui::InputDouble("Valeur", &target_, 0, 0, "%.4g");

    if (ImGui::Button("Nouvelle recherche")) {
      search_.First(memory, pending_type_, target_);
    }
    if (search_.active) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(140);
      ImGui::Combo("##compare", &compare_, kCompareNames, 5);
      ImGui::SameLine();
      if (ImGui::Button("Suivante")) {
        search_.Next(memory, Compare(compare_), target_);
      }
      ImGui::SameLine();
      if (ImGui::Button("Effacer")) {
        search_.Reset();
      }
      ImGui::Text("%zu resultat(s) (%.0f Mo parcourus)", search_.addresses.size(),
                  double(search_.scanned_bytes) / (1024.0 * 1024.0));
      if (search_.truncated) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1),
                           "Valeur trop courante : liste limitee, recommence avec une autre.");
      }
    } else {
      ImGui::TextDisabled(
          "Ex. la vie : cherche sa valeur, prends un coup, puis \"a diminue\" + Suivante.");
    }

    if (!search_.active || search_.addresses.empty()) {
      return;
    }
    constexpr size_t kShown = 200;
    if (search_.addresses.size() > kShown) {
      ImGui::TextDisabled("Affichage des %zu premiers : affine la recherche.", kShown);
    }
    if (ImGui::BeginTable("results", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_RowBg,
                          ImVec2(0, 220))) {
      ImGui::TableSetupColumn("Adresse");
      ImGui::TableSetupColumn("Valeur");
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70);
      ImGui::TableHeadersRow();
      const size_t count = std::min(kShown, search_.addresses.size());
      for (size_t i = 0; i < count; ++i) {
        const uint32_t address = search_.addresses[i];
        uint32_t raw = 0;
        const bool readable = ReadValue(memory, address, search_.type, &raw);
        ImGui::PushID(int(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%08X", address);
        ImGui::TableNextColumn();
        if (readable) {
          ImGui::TextUnformatted(FormatValue(raw, search_.type).c_str());
        } else {
          ImGui::TextDisabled("-");
        }
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("Ajouter")) {
          std::lock_guard lock(g_list.mutex);
          Cheat cheat;
          char name[32];
          std::snprintf(name, sizeof(name), "Cheat %zu", g_list.cheats.size() + 1);
          cheat.name = name;
          cheat.type = search_.type;
          cheat.address = address;
          cheat.frozen_raw = raw;
          g_list.cheats.push_back(cheat);
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
  }

  // g_list.mutex is held by the caller.
  void SaveUnlocked() {
    std::ofstream out(g_save_path, std::ios::trunc);
    out << "# Asura's Wrath cheats: type|address|value|name (types: i32 f32 i16 u8)\n";
    for (const auto& cheat : g_list.cheats) {
      char address[16];
      std::snprintf(address, sizeof(address), "%08X", cheat.address);
      out << kTypeTags[int(cheat.type)] << '|' << address << '|'
          << FormatValue(cheat.frozen_raw, cheat.type) << '|' << cheat.name << '\n';
    }
    REXLOG_INFO("Asura cheats: {} entries saved", g_list.cheats.size());
  }

  void LoadUnlocked() {
    std::vector<Cheat> loaded;
    {
      // CheatList::Load takes the lock itself; parse into a temporary list.
      CheatList temp;
      temp.Load(g_save_path);
      loaded = std::move(temp.cheats);
    }
    g_list.cheats = std::move(loaded);
  }

  Search search_;
  ValueType pending_type_ = ValueType::kInt32;
  double target_ = 0;
  int compare_ = int(Compare::kDecreased);
  bool reload_pending_ = false;
};

rex::ui::ImGuiDrawer* g_drawer = nullptr;
std::unique_ptr<CheatMenu> g_menu;

void ToggleMenu() {
  if (g_menu) {
    g_menu.reset();
  } else if (g_drawer) {
    g_menu = std::make_unique<CheatMenu>(g_drawer);
  }
}

}  // namespace

void AsuraCheats_Init(const std::filesystem::path& user_data_root) {
  g_save_path = user_data_root / "asura_cheats.txt";
  g_list.Load(g_save_path);
  if (!g_freeze_running.exchange(true)) {
    g_freeze_thread = std::thread(FreezeLoop);
  }
}

void AsuraCheats_RegisterBinds(rex::ui::ImGuiDrawer* drawer) {
  g_drawer = drawer;
  rex::ui::RegisterBind("bind_asura_cheats", "F9", "Cheat menu (Asura's Wrath)", ToggleMenu);
}

void AsuraCheats_Shutdown() {
  rex::ui::UnregisterBind("bind_asura_cheats");
  g_menu.reset();
  if (g_freeze_running.exchange(false) && g_freeze_thread.joinable()) {
    g_freeze_thread.join();
  }
}

bool AsuraCheats_IsMenuOpen() {
  return g_menu != nullptr;
}
