// asura_config_patch.cpp - see asura_config_patch.h.

#include "asura_config_patch.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

// Every option defaults to the game's original setting and takes effect at the
// next launch (the engine reads these once at boot).
REXCVAR_DEFINE_BOOL(asura_fx_motion_blur, true, "Asura/Effects", "Motion blur")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(asura_fx_depth_of_field, true, "Asura/Effects",
                    "Depth of field (background blur)")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(asura_fx_bloom, true, "Asura/Effects", "Bloom (glow around bright areas)")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(asura_fx_ambient_occlusion, true, "Asura/Effects",
                    "Ambient occlusion (contact shadows)")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(asura_fx_light_shafts, true, "Asura/Effects", "Light shafts (god rays)")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(asura_fx_lens_flares, true, "Asura/Effects", "Lens flares")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(asura_fx_distortion, true, "Asura/Effects",
                    "Distortion (heat haze, shockwaves)")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(asura_fx_radial_blur, true, "Asura/Effects", "Radial blur")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_INT32(asura_fx_texture_detail, 0, "Asura/Effects",
                     "EXPERIMENTAL. Load textures N mip levels sharper than the Xbox 360 did "
                     "(0 = original, 3 = full resolution). Uses more guest memory; lower it "
                     "if the game crashes or textures stay blurry.")
    .range(0, 3)
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_BOOL(asura_fx_high_shadows, false, "Asura/Effects",
                    "EXPERIMENTAL. Raise the shadow map resolution limit (1120 -> 2048).")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace {

constexpr std::string_view kPatchedDir = "AsuraPatched";

// --- Coalesced file format ---------------------------------------------------
// int32 file count, then per file: name, int32 section count, per section:
// name, int32 pair count, per pair: key, value. Integers are big-endian.
// Strings are an int32 length (including the terminator): positive = ANSI
// bytes, negative = UTF-16LE code units, 0 = empty.
struct Str {
  bool wide = false;
  std::u16string text;  // ANSI strings are widened 1:1
};
struct Pair {
  Str key, value;
};
struct Section {
  Str name;
  std::vector<Pair> pairs;
};
struct IniFile {
  Str name;
  std::vector<Section> sections;
};

class Reader {
 public:
  explicit Reader(const std::vector<uint8_t>& d) : d_(d) {}
  bool ok() const { return ok_; }
  bool at_end() const { return pos_ == d_.size(); }
  int32_t I32() {
    if (pos_ + 4 > d_.size()) {
      ok_ = false;
      return 0;
    }
    uint32_t v = (uint32_t(d_[pos_]) << 24) | (uint32_t(d_[pos_ + 1]) << 16) |
                 (uint32_t(d_[pos_ + 2]) << 8) | uint32_t(d_[pos_ + 3]);
    pos_ += 4;
    return static_cast<int32_t>(v);
  }
  Str S() {
    Str s;
    const int32_t n = I32();
    if (!ok_ || n == 0) {
      return s;
    }
    s.wide = n < 0;
    const size_t count = static_cast<size_t>(n < 0 ? -int64_t(n) : n);
    const size_t bytes = count * (s.wide ? 2 : 1);
    if (pos_ + bytes > d_.size() || count == 0) {
      ok_ = false;
      return s;
    }
    for (size_t i = 0; i + 1 < count; ++i) {  // drop the terminator
      s.text.push_back(s.wide ? char16_t(d_[pos_ + 2 * i] | (d_[pos_ + 2 * i + 1] << 8))
                              : char16_t(d_[pos_ + i]));
    }
    pos_ += bytes;
    return s;
  }

 private:
  const std::vector<uint8_t>& d_;
  size_t pos_ = 0;
  bool ok_ = true;
};

void PutI32(std::vector<uint8_t>& out, int32_t v) {
  const uint32_t u = static_cast<uint32_t>(v);
  out.push_back(uint8_t(u >> 24));
  out.push_back(uint8_t(u >> 16));
  out.push_back(uint8_t(u >> 8));
  out.push_back(uint8_t(u));
}

void PutS(std::vector<uint8_t>& out, const Str& s) {
  if (s.text.empty() && !s.wide) {
    PutI32(out, 0);
    return;
  }
  const int32_t count = static_cast<int32_t>(s.text.size() + 1);
  PutI32(out, s.wide ? -count : count);
  for (char16_t c : s.text) {
    out.push_back(uint8_t(c & 0xFF));
    if (s.wide) {
      out.push_back(uint8_t(c >> 8));
    }
  }
  out.push_back(0);
  if (s.wide) {
    out.push_back(0);
  }
}

bool Parse(const std::vector<uint8_t>& data, std::vector<IniFile>& files) {
  Reader r(data);
  const int32_t file_count = r.I32();
  for (int32_t f = 0; r.ok() && f < file_count; ++f) {
    IniFile file;
    file.name = r.S();
    const int32_t section_count = r.I32();
    for (int32_t s = 0; r.ok() && s < section_count; ++s) {
      Section section;
      section.name = r.S();
      const int32_t pair_count = r.I32();
      for (int32_t p = 0; r.ok() && p < pair_count; ++p) {
        Pair pair;
        pair.key = r.S();
        pair.value = r.S();
        section.pairs.push_back(std::move(pair));
      }
      file.sections.push_back(std::move(section));
    }
    files.push_back(std::move(file));
  }
  return r.ok() && r.at_end();
}

std::vector<uint8_t> Serialize(const std::vector<IniFile>& files) {
  std::vector<uint8_t> out;
  PutI32(out, static_cast<int32_t>(files.size()));
  for (const auto& file : files) {
    PutS(out, file.name);
    PutI32(out, static_cast<int32_t>(file.sections.size()));
    for (const auto& section : file.sections) {
      PutS(out, section.name);
      PutI32(out, static_cast<int32_t>(section.pairs.size()));
      for (const auto& pair : section.pairs) {
        PutS(out, pair.key);
        PutS(out, pair.value);
      }
    }
  }
  return out;
}

std::u16string U16(std::string_view s) {
  return std::u16string(s.begin(), s.end());
}

std::string U8(const std::u16string& s) {
  std::string out;
  for (char16_t c : s) {
    out.push_back(c < 0x80 ? char(c) : '?');
  }
  return out;
}

bool EndsWithNoCase(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() &&
         std::equal(suffix.begin(), suffix.end(), text.end() - suffix.size(),
                    [](char a, char b) {
                      return std::tolower(static_cast<unsigned char>(a)) ==
                             std::tolower(static_cast<unsigned char>(b));
                    });
}

// --- Edits ---------------------------------------------------------------------

struct Edit {
  std::string_view key;
  std::string value;
};

std::vector<Edit> WantedEdits() {
  std::vector<Edit> edits;
  auto flag = [&](std::string_view key, bool enabled) {
    if (!enabled) {
      edits.push_back({key, "False"});
    }
  };
  flag("MotionBlur", REXCVAR_GET(asura_fx_motion_blur));
  flag("MotionBlurPause", REXCVAR_GET(asura_fx_motion_blur));
  flag("DepthOfField", REXCVAR_GET(asura_fx_depth_of_field));
  flag("Bloom", REXCVAR_GET(asura_fx_bloom));
  flag("AmbientOcclusion", REXCVAR_GET(asura_fx_ambient_occlusion));
  flag("bAllowLightShafts", REXCVAR_GET(asura_fx_light_shafts));
  flag("LensFlares", REXCVAR_GET(asura_fx_lens_flares));
  flag("Distortion", REXCVAR_GET(asura_fx_distortion));
  flag("FilteredDistortion", REXCVAR_GET(asura_fx_distortion));
  flag("AllowRadialBlur", REXCVAR_GET(asura_fx_radial_blur));
  if (REXCVAR_GET(asura_fx_high_shadows)) {
    edits.push_back({"MaxShadowResolution", "2048"});
    edits.push_back({"MaxWholeSceneDominantShadowResolution", "2048"});
  }
  return edits;
}

// Lowers LODBias=N in a TEXTUREGROUP_* value by `reduce` (not below 0).
bool ReduceLodBias(std::u16string& value, int reduce) {
  const std::u16string tag = u"LODBias=";
  const size_t at = value.find(tag);
  if (at == std::u16string::npos) {
    return false;
  }
  const size_t digit = at + tag.size();
  size_t end = digit;
  while (end < value.size() && value[end] >= u'0' && value[end] <= u'9') {
    ++end;
  }
  if (end == digit) {
    return false;
  }
  const int bias = std::stoi(U8(value.substr(digit, end - digit)));
  const int lowered = std::max(0, bias - reduce);
  if (lowered == bias) {
    return false;
  }
  value.replace(digit, end - digit, U16(std::to_string(lowered)));
  return true;
}

// Applies the edits to [SystemSettings] of Xbox360-BCEngine.ini. The patched
// file must keep the original size (the game's table of contents records it),
// so the length difference is absorbed by the commented-out "#TEXTUREGROUP_*"
// entries of the same section, which the engine never reads.
bool ApplyEdits(std::vector<IniFile>& files, size_t original_size, std::string& error) {
  const auto edits = WantedEdits();
  const int texture_reduce = REXCVAR_GET(asura_fx_texture_detail);
  Section* settings = nullptr;
  for (auto& file : files) {
    if (!EndsWithNoCase(U8(file.name.text), "Xbox360-BCEngine.ini")) {
      continue;
    }
    for (auto& section : file.sections) {
      if (section.name.text == u"SystemSettings") {
        settings = &section;
      }
    }
  }
  if (!settings) {
    error = "[SystemSettings] not found";
    return false;
  }
  for (auto& pair : settings->pairs) {
    const std::string key = U8(pair.key.text);
    for (const auto& edit : edits) {
      if (key == edit.key) {
        pair.value.text = U16(edit.value);
      }
    }
    if (texture_reduce > 0 && key.rfind("TEXTUREGROUP_", 0) == 0) {
      ReduceLodBias(pair.value.text, texture_reduce);
    }
  }

  size_t size = Serialize(files).size();
  // Pad or trim the dead "#..." values until the size matches.
  for (auto& pair : settings->pairs) {
    if (size == original_size) {
      break;
    }
    if (pair.key.text.empty() || pair.key.text[0] != u'#') {
      continue;
    }
    const size_t unit = pair.value.wide ? 2 : 1;
    if (size > original_size) {
      const size_t excess_units = (size - original_size + unit - 1) / unit;
      const size_t cut = std::min(excess_units, pair.value.text.size());
      pair.value.text.resize(pair.value.text.size() - cut);
      size -= cut * unit;
    } else {
      const size_t missing = original_size - size;
      if (missing % unit != 0) {
        continue;
      }
      pair.value.text.append(missing / unit, u' ');
      size += missing;
    }
  }
  if (size != original_size) {
    error = "could not keep the original file size";
    return false;
  }
  return true;
}

// --- State -----------------------------------------------------------------------

std::vector<std::string> g_patched;  // lowercase file names with a patched copy
std::vector<std::string> g_messages;

std::string ToLower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool AnyChange() {
  return !WantedEdits().empty() || REXCVAR_GET(asura_fx_texture_detail) > 0;
}

std::string Fingerprint() {
  std::string fp;
  for (const auto& e : WantedEdits()) {
    fp += std::string(e.key) + "=" + e.value + ";";
  }
  fp += "tex=" + std::to_string(REXCVAR_GET(asura_fx_texture_detail));
  return fp;
}

}  // namespace

void AsuraConfig_Prepare(const std::filesystem::path& game_data_root) {
  g_patched.clear();
  g_messages.clear();
  if (!AnyChange()) {
    return;
  }
  std::error_code ec;
  const auto cooked = std::filesystem::absolute(game_data_root, ec) / "BCGame" / "CookedXbox360";
  const auto out_dir = cooked / std::string(kPatchedDir);
  std::filesystem::create_directories(out_dir, ec);

  // Rewrite only when the settings changed since the last launch.
  const auto stamp_path = out_dir / "settings.txt";
  const std::string fingerprint = Fingerprint();
  std::string previous;
  {
    std::ifstream stamp(stamp_path);
    std::getline(stamp, previous);
  }
  const bool reuse = previous == fingerprint;

  bool all_ok = true;
  for (const auto& entry : std::filesystem::directory_iterator(cooked, ec)) {
    const auto name = entry.path().filename().string();
    const auto lower = ToLower(name);
    if (!entry.is_regular_file() || lower.rfind("coalesced_", 0) != 0 ||
        !EndsWithNoCase(name, ".bin")) {
      continue;
    }
    const auto target = out_dir / name;
    if (reuse && std::filesystem::exists(target, ec) &&
        std::filesystem::file_size(target, ec) == entry.file_size(ec)) {
      g_patched.push_back(lower);
      continue;
    }
    std::ifstream in(entry.path(), std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    std::vector<IniFile> files;
    std::string error;
    if (!Parse(data, files) || Serialize(files) != data) {
      g_messages.push_back(name + ": unexpected format, left untouched");
      all_ok = false;
      continue;
    }
    if (!ApplyEdits(files, data.size(), error)) {
      g_messages.push_back(name + ": " + error + ", left untouched");
      all_ok = false;
      continue;
    }
    const auto patched = Serialize(files);
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(patched.data()),
              static_cast<std::streamsize>(patched.size()));
    if (!out.good()) {
      g_messages.push_back(name + ": could not write the patched copy");
      all_ok = false;
      continue;
    }
    g_patched.push_back(lower);
  }
  if (all_ok) {
    std::ofstream stamp(stamp_path, std::ios::trunc);
    stamp << fingerprint << "\n";
  }
}

std::optional<std::string> AsuraConfig_Redirect(std::string_view guest_path) {
  if (g_patched.empty()) {
    return std::nullopt;
  }
  const auto slash = guest_path.find_last_of("\\/");
  if (slash == std::string_view::npos) {
    return std::nullopt;
  }
  const auto directory = guest_path.substr(0, slash);
  const auto name = guest_path.substr(slash + 1);
  if (!EndsWithNoCase(directory, "CookedXbox360") ||
      std::find(g_patched.begin(), g_patched.end(), ToLower(name)) == g_patched.end()) {
    return std::nullopt;
  }
  return std::string(directory) + "\\" + std::string(kPatchedDir) + "\\" + std::string(name);
}

void AsuraConfig_LogStatus() {
  for (const auto& message : g_messages) {
    REXLOG_WARN("Asura effects: {}", message);
  }
  if (!g_patched.empty()) {
    REXLOG_INFO("Asura effects: patched settings active ({})", Fingerprint());
  }
}
