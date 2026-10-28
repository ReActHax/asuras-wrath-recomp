// Asura's Wrath Recompiled - launcher and installer.
//
// - First run: extracts the player's own game files (Xbox 360 ISO, title
//   update 1, optional DLC packages) next to the executable.
// - Then: settings editor for asura_wrath_recomp.toml (keeps comments and
//   unknown keys) and "Play" button.
//
// Built with the C# compiler included in Windows (.NET Framework 4.x,
// C# 5 syntax only), see package_release.bat:
//   csc /nologo /target:winexe /out:AsuraLauncher.exe
//       /r:System.Windows.Forms.dll /r:System.Drawing.dll AsuraLauncher.cs

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("Asura's Wrath Recompiled Launcher")]
[assembly: System.Reflection.AssemblyProduct("Asura's Wrath Recompiled")]

namespace AsuraLauncher {

static class Lang {
  // Launcher interface language: AsuraLauncher.ini ("language=fr" or "en"),
  // otherwise the Windows language.
  public static readonly bool French = ReadFrench();
  public static string T(string fr, string en) { return French ? fr : en; }

  static string IniPath() {
    return Path.Combine(Path.GetDirectoryName(Application.ExecutablePath), "AsuraLauncher.ini");
  }

  static bool ReadFrench() {
    try {
      if (File.Exists(IniPath())) {
        foreach (string line in File.ReadAllLines(IniPath())) {
          string trimmed = line.Trim().ToLowerInvariant();
          if (trimmed == "language=fr") return true;
          if (trimmed == "language=en") return false;
        }
      }
    } catch (Exception) {
    }
    return CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "fr";
  }

  public static void SaveLanguage(bool french) {
    File.WriteAllText(IniPath(), "language=" + (french ? "fr" : "en") + "\r\n");
  }

  // Xbox 360 language ID matching the Windows language.
  public static string GameLanguage() {
    switch (CultureInfo.CurrentUICulture.TwoLetterISOLanguageName) {
      case "ja": return "2";
      case "de": return "3";
      case "fr": return "4";
      case "es": return "5";
      case "it": return "6";
      default: return "1";
    }
  }
}

static class Paths {
  public static readonly string AppDir =
      Path.GetDirectoryName(Application.ExecutablePath);
  public static readonly string GameExe = Path.Combine(AppDir, "asura_wrath_recomp.exe");
  public static readonly string Toml = Path.Combine(AppDir, "asura_wrath_recomp.toml");
  public static readonly string Extracted = Path.Combine(AppDir, "extracted");
  public static readonly string Xex = Path.Combine(Extracted, "default.xex");
  public static readonly string Xexp = Path.Combine(Extracted, "default.xexp");

  // Same default as the game: Documents\asura_wrath_recomp, unless overridden
  // by user_data_root in the toml.
  public static string UserData(TomlFile toml) {
    string value = toml != null ? toml.GetString("user_data_root") : null;
    if (!string.IsNullOrEmpty(value)) {
      return Path.IsPathRooted(value) ? value : Path.Combine(AppDir, value);
    }
    return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
                        "asura_wrath_recomp");
  }

  public static bool GameFilesPresent() {
    return File.Exists(Xex) && File.Exists(Xexp) &&
           Directory.Exists(Path.Combine(Extracted, "BCGame"));
  }
}

// Known good files (Asura's Wrath, title ID 43430817, title update 1).
static class KnownFiles {
  public const string XexMd5 = "6e1d309f85459b4e5b7200e12d1d9449";
  public const string XexpMd5 = "41a6522e92213cbe70410fcff8002b0c";

  public static string Md5(string path) {
    using (var md5 = MD5.Create())
    using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read,
                                       1 << 20)) {
      byte[] hash = md5.ComputeHash(stream);
      var sb = new StringBuilder();
      foreach (byte b in hash) sb.Append(b.ToString("x2"));
      return sb.ToString();
    }
  }
}

// ---------------------------------------------------------------------------
// TOML (flat key = value lines only, as used by the game).
// ---------------------------------------------------------------------------

class TomlFile {
  readonly string path_;
  readonly List<string> lines_ = new List<string>();

  public TomlFile(string path) {
    path_ = path;
    if (File.Exists(path)) {
      lines_.AddRange(File.ReadAllLines(path, Encoding.UTF8));
    }
  }

  public bool Exists { get { return File.Exists(path_); } }

  int Find(string key) {
    var regex = new Regex("^\\s*" + Regex.Escape(key) + "\\s*=");
    for (int i = 0; i < lines_.Count; ++i) {
      if (regex.IsMatch(lines_[i])) return i;
    }
    return -1;
  }

  // Raw value (without trailing comment), or null.
  public string Get(string key) {
    int index = Find(key);
    if (index < 0) return null;
    string value = lines_[index].Substring(lines_[index].IndexOf('=') + 1).Trim();
    if (!value.StartsWith("\"")) {
      int comment = value.IndexOf('#');
      if (comment >= 0) value = value.Substring(0, comment).Trim();
    }
    return value;
  }

  public string GetString(string key) {
    string value = Get(key);
    if (value == null) return null;
    if (value.Length >= 2 && value.StartsWith("\"")) {
      int end = value.IndexOf('"', 1);
      if (end > 0) return value.Substring(1, end - 1).Replace("\\\\", "\\");
    }
    return value;
  }

  public void Set(string key, string literal) {
    int index = Find(key);
    string line = key + " = " + literal;
    if (index >= 0) {
      lines_[index] = line;
      return;
    }
    if (Find("# --- Launcher") < 0) {
      lines_.Add("");
      lines_.Add("# --- Launcher (options added by AsuraLauncher) ----------------------------");
    }
    lines_.Add(line);
  }

  public void Save() {
    File.WriteAllLines(path_, lines_.ToArray(), new UTF8Encoding(false));
  }
}

// ---------------------------------------------------------------------------
// Settings shown in the launcher.
// ---------------------------------------------------------------------------

enum SettingKind { Bool, Choice, Text }

class Setting {
  public string Key;
  public SettingKind Kind;
  public string Default;  // TOML literal.
  public string Label;
  public string Tip;
  // Shown in red when the value differs from the default.
  public string Warning;
  public string[] Values;  // TOML literals for Choice.
  public string[] Names;
  public Control Editor;

  public static Setting Bool(string key, bool value, string label, string tip) {
    var s = new Setting();
    s.Key = key;
    s.Kind = SettingKind.Bool;
    s.Default = value ? "true" : "false";
    s.Label = label;
    s.Tip = tip;
    return s;
  }

  public static Setting Choice(string key, string value, string[] values, string[] names,
                               string label, string tip) {
    var s = new Setting();
    s.Key = key;
    s.Kind = SettingKind.Choice;
    s.Default = value;
    s.Values = values;
    s.Names = names;
    s.Label = label;
    s.Tip = tip;
    return s;
  }

  public static Setting Text(string key, string value, string label, string tip) {
    var s = new Setting();
    s.Key = key;
    s.Kind = SettingKind.Text;
    s.Default = value;
    s.Label = label;
    s.Tip = tip;
    return s;
  }

  public Setting Warn(string warning) {
    Warning = warning;
    return this;
  }

  public void Load(string literal) {
    if (literal == null) literal = Default;
    if (Kind == SettingKind.Bool) {
      ((CheckBox)Editor).Checked = literal.Trim().ToLowerInvariant() == "true";
    } else if (Kind == SettingKind.Choice) {
      var combo = (ComboBox)Editor;
      int index = Array.IndexOf(Values, literal.Trim());
      if (index < 0) index = Array.IndexOf(Values, Default);
      combo.SelectedIndex = Math.Max(index, 0);
    } else {
      string text = literal.Trim();
      if (text.Length >= 2 && text.StartsWith("\"") && text.EndsWith("\"")) {
        text = text.Substring(1, text.Length - 2);
      }
      ((TextBox)Editor).Text = text;
    }
  }

  public string Literal() {
    if (Kind == SettingKind.Bool) return ((CheckBox)Editor).Checked ? "true" : "false";
    if (Kind == SettingKind.Choice) {
      int index = ((ComboBox)Editor).SelectedIndex;
      return Values[index < 0 ? 0 : index];
    }
    return "\"" + ((TextBox)Editor).Text.Replace("\"", "").Trim() + "\"";
  }
}

class SettingsPage {
  public string Title;
  public List<Setting> Settings = new List<Setting>();
  public SettingsPage(string title) { Title = title; }
  public SettingsPage Add(Setting setting) { Settings.Add(setting); return this; }
}

static class SettingsCatalog {
  static string T(string fr, string en) { return Lang.T(fr, en); }

  public static List<SettingsPage> Build() {
    var pages = new List<SettingsPage>();

    pages.Add(new SettingsPage(T("Affichage", "Display"))
        .Add(Setting.Bool("fullscreen", true, T("Plein écran", "Fullscreen"),
                          T("Alt+Entrée bascule aussi en jeu.", "Alt+Enter also toggles in game.")))
        .Add(Setting.Choice("resolution_scale", "1", new[] {"1", "2", "3"},
                            new[] {"1× (1280×720)", "2× (2560×1440)", "3× (3840×2160)"},
                            T("Résolution interne", "Internal resolution"),
                            T("Résolution de calcul de l'image (le jeu d'origine est en 720p).",
                              "Resolution the image is rendered at (the original game is 720p)."))
             .Warn(T("⚠ Très gourmand : l'émulation de la mémoire vidéo de la Xbox 360 coûte beaucoup plus cher " +
                     "au-delà de 720p, attendez-vous à de fortes baisses de performances. " +
                     "Pour une image plus nette, préférez 720p avec la mise à l'échelle FSR 1.",
                     "⚠ Very demanding: emulating the Xbox 360 video memory costs much more above 720p, " +
                     "expect large performance drops. For a sharper image, prefer 720p with FSR 1 upscaling.")))
        .Add(Setting.Choice("present_effect", "\"bilinear\"",
                            new[] {"\"bilinear\"", "\"cas\"", "\"fsr\"", "\"fsr2\"", "\"fsr3\""},
                            new[] {T("Bilinéaire (d'origine)", "Bilinear (original)"),
                                   "AMD CAS", "AMD FSR 1", "AMD FSR 2", "AMD FSR 3"},
                            T("Mise à l'échelle", "Upscaling"),
                            T("Agrandissement de l'image 720p vers votre écran. FSR 1 est le plus net.",
                              "How the 720p image is scaled to your screen. FSR 1 is the sharpest.")))
        .Add(Setting.Choice("anisotropic_override", "5",
                            new[] {"-1", "0", "1", "2", "3", "4", "5"},
                            new[] {T("Celui du jeu", "Game default"), T("Désactivé", "Off"),
                                   "1×", "2×", "4×", "8×", "16×"},
                            T("Filtrage anisotrope", "Anisotropic filtering"),
                            T("Textures plus nettes vues en biais.", "Sharper textures at oblique angles.")))
        .Add(Setting.Bool("vsync", true, T("Synchronisation verticale", "Vertical sync"),
                          T("À laisser activée : le rythme du jeu suit le 60 Hz émulé.",
                            "Keep it on: game pacing follows the emulated 60 Hz.")))
        .Add(Setting.Choice("gpu_backend", "\"d3d12\"", new[] {"\"d3d12\"", "\"vulkan\""},
                            new[] {"Direct3D 12 " + T("(recommandé)", "(recommended)"),
                                   "Vulkan " + T("(expérimental)", "(experimental)")},
                            T("Rendu", "Renderer"), "")
             .Warn(T("⚠ Vulkan affiche de nombreux défauts graphiques dans ce jeu (éclairage, effets).",
                     "⚠ Vulkan shows many graphical glitches in this game (lighting, effects).")))
        .Add(Setting.Choice("user_language", Lang.GameLanguage(),
                            new[] {"1", "2", "3", "4", "5", "6"},
                            new[] {"English", "日本語", "Deutsch", "Français", "Español", "Italiano"},
                            T("Langue du jeu", "Game language"),
                            T("Langue des textes et des menus (les seules présentes dans le jeu). Les voix (anglais " +
                              "ou japonais) ne dépendent pas de ce réglage.",
                              "Language of texts and menus (the only ones in the game). Voices (English or " +
                              "Japanese) don't depend on this setting."))));

    pages.Add(new SettingsPage(T("Jeu", "Game"))
        .Add(Setting.Bool("asura_60fps", true, "60 FPS",
                          T("60 images/s avec la bonne vitesse de jeu. F6 bascule 30/60 en jeu.",
                            "60 frames per second at the correct game speed. F6 toggles 30/60 in game.")))
        .Add(Setting.Bool("asura_skip_logos", true,
                          T("Passer les logos au démarrage", "Skip the startup logos"), ""))
        .Add(Setting.Bool("asura_disable_postprocess", false,
                          T("Désactiver le post-traitement", "Disable post-processing"),
                          T("Retire l'accentuation des contours (utile en résolution 2× ou 3×). Change le rendu.",
                            "Removes edge over-sharpening (useful at 2× or 3×). Changes the look."))));

    pages.Add(new SettingsPage(T("Effets", "Effects"))
        .Add(Setting.Bool("asura_fx_motion_blur", true, T("Flou de mouvement", "Motion blur"), ""))
        .Add(Setting.Bool("asura_fx_depth_of_field", true, T("Profondeur de champ", "Depth of field"), ""))
        .Add(Setting.Bool("asura_fx_bloom", true, "Bloom", ""))
        .Add(Setting.Bool("asura_fx_ambient_occlusion", true,
                          T("Occlusion ambiante", "Ambient occlusion"), ""))
        .Add(Setting.Bool("asura_fx_light_shafts", true, T("Rayons de lumière", "Light shafts"), ""))
        .Add(Setting.Bool("asura_fx_lens_flares", true, T("Reflets d'objectif", "Lens flares"), ""))
        .Add(Setting.Bool("asura_fx_distortion", true, T("Distorsion", "Distortion"), ""))
        .Add(Setting.Bool("asura_fx_radial_blur", true, T("Flou radial", "Radial blur"), ""))
        .Add(Setting.Choice("asura_fx_texture_detail", "0", new[] {"0", "1", "2", "3"},
                            new[] {T("D'origine", "Original"), "+1", "+2",
                                   T("+3 (maximum)", "+3 (maximum)")},
                            T("Détail des textures (expérimental)", "Texture detail (experimental)"),
                            T("Textures plus nettes, plus de mémoire. Baissez si le jeu plante.",
                              "Sharper textures, more memory. Lower it if the game crashes.")))
        .Add(Setting.Bool("asura_fx_high_shadows", false,
                          T("Ombres haute définition (expérimental)", "High resolution shadows (experimental)"),
                          "")));

    pages.Add(new SettingsPage(T("Commandes", "Controls"))
        .Add(Setting.Bool("input_press_latch", true,
                          T("Mémoriser les appuis rapides (QTE)", "Remember quick presses (QTE)"),
                          T("Si le jeu ralentit pendant un QTE, un appui très bref peut tomber entre deux " +
                            "lectures de la manette et être perdu. Cette option le garde en mémoire et le " +
                            "transmet au jeu à la lecture suivante. À laisser activé.",
                            "If the game slows down during a QTE, a very short press can happen between two " +
                            "controller reads and be lost. This option remembers it and passes it to the game " +
                            "on the next read. Keep it on.")))
        .Add(Setting.Bool("input_turbo", true, T("Mode turbo", "Turbo mode"),
                          T("Maintenir un bouton turbo revient à le marteler : il est appuyé et relâché " +
                            "automatiquement plusieurs fois par seconde. Le jeu a été conçu pour 30 images/s : " +
                            "en 60 FPS, les QTE où il faut marteler un bouton deviennent nettement plus durs, " +
                            "le turbo compense. F8 l'active ou le désactive en jeu.",
                            "Holding a turbo button is the same as mashing it: it is pressed and released " +
                            "automatically several times per second. The game was designed for 30 frames per " +
                            "second: at 60 FPS, button-mashing QTEs become much harder, turbo makes up for it. " +
                            "F8 toggles it in game.")))
        .Add(Setting.Text("input_turbo_buttons", "\"B\"", T("Boutons turbo", "Turbo buttons"),
                          T("Séparés par des virgules (A, B, X, Y, LB, RB…). B suffit pour les QTE de " +
                            "martèlement et laisse Y libre pour le tir rapide.",
                            "Comma-separated (A, B, X, Y, LB, RB…). B is enough for mashing QTEs and keeps Y " +
                            "free for rapid fire.")))
        .Add(Setting.Choice("input_turbo_rate_hz", "12", new[] {"8", "10", "12", "15", "20"},
                            new[] {"8 /s", "10 /s", "12 /s", "15 /s", "20 /s"},
                            T("Cadence du turbo", "Turbo rate"), "")));

    pages.Add(new SettingsPage("Textures")
        .Add(Setting.Bool("texture_replacement", true,
                          T("Activer le remplacement de textures", "Enable texture replacement"),
                          T("Aucune texture de remplacement n'est fournie. Si vous ajoutez des fichiers DDS " +
                            "(par exemple un pack de textures HD) dans le dossier textures\\replace, ils " +
                            "remplacent les textures d'origine (voir LISEZMOI.txt). Sans fichier, cette " +
                            "option ne change rien.",
                            "No replacement textures are included. If you add DDS files (for example an HD " +
                            "texture pack) to the textures\\replace folder, they replace the original " +
                            "textures (see README.txt). Without files, this option changes nothing.")))
        .Add(Setting.Bool("texture_dump", false,
                          T("Extraire les textures du jeu (pour créer des textures)",
                            "Dump the game textures (to create textures)"),
                          T("Pour les moddeurs. Chaque texture est enregistrée une seule fois dans " +
                            "textures\\dump au moment où le jeu la charge : seules les textures des " +
                            "menus et des passages réellement joués sont extraites. Ralentit le jeu, " +
                            "à désactiver ensuite.",
                            "For modders. Each texture is saved once to textures\\dump when the game " +
                            "loads it: only the textures of the menus and parts actually played are " +
                            "dumped. Slows the game down, turn it off afterwards."))));
    return pages;
  }
}

// ---------------------------------------------------------------------------
// Xbox 360 disc image (XDVDFS) and STFS package extraction.
// ---------------------------------------------------------------------------

class Xiso {
  const int Sector = 2048;
  static readonly long[] PartitionOffsets = {0, 0x2080000, 0xFD90000, 0x18300000};
  static readonly byte[] Magic = Encoding.ASCII.GetBytes("MICROSOFT*XBOX*MEDIA");

  public struct Entry {
    public string Path;
    public bool Directory;
    public uint Sector;
    public uint Size;
  }

  readonly FileStream stream_;
  readonly long partition_;
  readonly uint rootSector_, rootSize_;

  public Xiso(FileStream stream) {
    stream_ = stream;
    partition_ = -1;
    var header = new byte[28];
    foreach (long offset in PartitionOffsets) {
      if (offset + 32 * Sector + header.Length > stream.Length) continue;
      stream.Position = offset + 32 * Sector;
      ReadExactly(header, header.Length);
      bool match = true;
      for (int i = 0; i < Magic.Length; ++i) {
        if (header[i] != Magic[i]) { match = false; break; }
      }
      if (match) {
        partition_ = offset;
        rootSector_ = BitConverter.ToUInt32(header, 20);
        rootSize_ = BitConverter.ToUInt32(header, 24);
        break;
      }
    }
    if (partition_ < 0) {
      throw new InvalidDataException(Lang.T(
          "Ce fichier n'est pas une image disque Xbox 360 (en-tête XDVDFS introuvable).",
          "This file is not an Xbox 360 disc image (XDVDFS header not found)."));
    }
  }

  void ReadExactly(byte[] buffer, int count) {
    int done = 0;
    while (done < count) {
      int read = stream_.Read(buffer, done, count - done);
      if (read <= 0) throw new EndOfStreamException();
      done += read;
    }
  }

  public List<Entry> Entries() {
    var result = new List<Entry>();
    Walk(rootSector_, rootSize_, "", result, 0);
    return result;
  }

  void Walk(uint sector, uint size, string prefix, List<Entry> result, int depth) {
    if (size == 0 || depth > 32) return;
    var table = new byte[size];
    stream_.Position = partition_ + (long)sector * Sector;
    ReadExactly(table, (int)size);
    var seen = new HashSet<int>();
    var pending = new Stack<int>();
    pending.Push(0);
    while (pending.Count > 0) {
      int offset = pending.Pop();
      if (!seen.Add(offset) || offset + 14 > table.Length) continue;
      ushort left = BitConverter.ToUInt16(table, offset);
      ushort right = BitConverter.ToUInt16(table, offset + 2);
      uint entrySector = BitConverter.ToUInt32(table, offset + 4);
      uint entrySize = BitConverter.ToUInt32(table, offset + 8);
      byte attributes = table[offset + 12];
      int nameLength = table[offset + 13];
      if (left == 0xFFFF && right == 0xFFFF && entrySector == 0xFFFFFFFF) continue;
      if (left != 0 && left != 0xFFFF) pending.Push(left * 4);
      if (right != 0 && right != 0xFFFF) pending.Push(right * 4);
      if (nameLength == 0 || offset + 14 + nameLength > table.Length) continue;
      string name = Encoding.GetEncoding(28591).GetString(table, offset + 14, nameLength);
      if (name.IndexOfAny(System.IO.Path.GetInvalidFileNameChars()) >= 0 || name == "." ||
          name == "..") {
        continue;
      }
      var entry = new Entry();
      entry.Path = prefix.Length > 0 ? prefix + "\\" + name : name;
      entry.Directory = (attributes & 0x10) != 0;
      entry.Sector = entrySector;
      entry.Size = entrySize;
      result.Add(entry);
      if (entry.Directory) Walk(entrySector, entrySize, entry.Path, result, depth + 1);
    }
  }

  // progress(bytes done, bytes total)
  public void Extract(string outDir, Action<long, long, string> progress,
                      Func<bool> cancelled) {
    List<Entry> entries = Entries();
    long total = 0;
    foreach (Entry e in entries) if (!e.Directory) total += e.Size;
    long done = 0;
    var buffer = new byte[8 << 20];
    foreach (Entry e in entries) {
      if (cancelled()) throw new OperationCanceledException();
      string dest = System.IO.Path.Combine(outDir, e.Path);
      if (e.Directory) {
        Directory.CreateDirectory(dest);
        continue;
      }
      Directory.CreateDirectory(System.IO.Path.GetDirectoryName(dest));
      stream_.Position = partition_ + (long)e.Sector * Sector;
      long remaining = e.Size;
      using (var output = new FileStream(dest, FileMode.Create, FileAccess.Write, FileShare.None,
                                         1 << 20)) {
        while (remaining > 0) {
          int chunk = (int)Math.Min(remaining, buffer.Length);
          ReadExactly(buffer, chunk);
          output.Write(buffer, 0, chunk);
          remaining -= chunk;
          done += chunk;
          progress(done, total, e.Path);
          if (cancelled()) throw new OperationCanceledException();
        }
      }
      progress(done, total, e.Path);
    }
  }
}

class Stfs {
  static readonly int[] BlocksPerHashLevel = {0xAA, 0x70E4, 0x4AF768};

  readonly FileStream stream_;
  readonly long baseOffset_;
  readonly int blocksPerHashTable_;
  readonly int fileTableBlockCount_;
  readonly int fileTableBlock_;

  public static bool IsPackage(string path) {
    try {
      using (var stream = File.OpenRead(path)) {
        var magic = new byte[4];
        if (stream.Read(magic, 0, 4) != 4) return false;
        string text = Encoding.ASCII.GetString(magic);
        return text == "CON " || text == "LIVE" || text == "PIRS";
      }
    } catch (Exception) {
      return false;
    }
  }

  public Stfs(FileStream stream) {
    stream_ = stream;
    var head = new byte[0x3B0];
    stream.Position = 0;
    if (stream.Read(head, 0, head.Length) != head.Length) throw new EndOfStreamException();
    string magic = Encoding.ASCII.GetString(head, 0, 4);
    if (magic != "CON " && magic != "LIVE" && magic != "PIRS") {
      throw new InvalidDataException(Lang.T("Ce fichier n'est pas un paquet Xbox 360 (STFS).",
                                            "This file is not an Xbox 360 package (STFS)."));
    }
    uint headerSize = BigEndian32(head, 0x340);
    if (BigEndian32(head, 0x3A9) != 0) {
      throw new InvalidDataException(Lang.T("Format de paquet non pris en charge.",
                                            "Unsupported package format."));
    }
    bool readOnly = (head[0x379 + 2] & 1) != 0;
    blocksPerHashTable_ = readOnly ? 1 : 2;
    fileTableBlockCount_ = head[0x379 + 3] | (head[0x379 + 4] << 8);
    fileTableBlock_ = head[0x379 + 5] | (head[0x379 + 6] << 8) | (head[0x379 + 7] << 16);
    baseOffset_ = (headerSize + 0xFFF) & ~0xFFFL;
  }

  static uint BigEndian32(byte[] data, int offset) {
    return ((uint)data[offset] << 24) | ((uint)data[offset + 1] << 16) |
           ((uint)data[offset + 2] << 8) | data[offset + 3];
  }

  long BlockOffset(int blockIndex) {
    long block = blockIndex;
    foreach (int levelBase in BlocksPerHashLevel) {
      block += (long)((blockIndex + levelBase) / levelBase) * blocksPerHashTable_;
      if (blockIndex < levelBase) break;
    }
    return baseOffset_ + (block << 12);
  }

  int NextBlock(int blockIndex) {
    int groupFirst = blockIndex - blockIndex % 0xAA;
    long table = BlockOffset(groupFirst) - 0x1000L * blocksPerHashTable_;
    stream_.Position = table + (blockIndex % 0xAA) * 0x18 + 0x15;
    var next = new byte[3];
    stream_.Read(next, 0, 3);
    return (next[0] << 16) | (next[1] << 8) | next[2];
  }

  byte[] ReadChain(int start, int count, bool contiguous) {
    var data = new byte[(long)count * 0x1000];
    int block = start;
    for (int i = 0; i < count; ++i) {
      stream_.Position = BlockOffset(block);
      int done = 0;
      while (done < 0x1000) {
        int read = stream_.Read(data, i * 0x1000 + done, 0x1000 - done);
        if (read <= 0) break;
        done += read;
      }
      block = contiguous ? block + 1 : NextBlock(block);
    }
    return data;
  }

  // Extracts a root-level file by name. Returns false if not found.
  public bool ExtractFile(string fileName, string destination) {
    byte[] table = ReadChain(fileTableBlock_, fileTableBlockCount_, false);
    for (int offset = 0; offset + 0x40 <= table.Length; offset += 0x40) {
      int flags = table[offset + 0x28];
      int nameLength = flags & 0x3F;
      if (nameLength == 0 || (flags & 0x80) != 0) continue;
      string name = Encoding.ASCII.GetString(table, offset, nameLength);
      if (!string.Equals(name, fileName, StringComparison.OrdinalIgnoreCase)) continue;
      int blocks = table[offset + 0x29] | (table[offset + 0x2A] << 8) | (table[offset + 0x2B] << 16);
      int start = table[offset + 0x2F] | (table[offset + 0x30] << 8) | (table[offset + 0x31] << 16);
      int size = (int)BigEndian32(table, offset + 0x34);
      byte[] data = ReadChain(start, blocks, (flags & 0x40) != 0);
      using (var output = new FileStream(destination, FileMode.Create)) {
        output.Write(data, 0, Math.Min(size, data.Length));
      }
      return true;
    }
    return false;
  }
}

// ---------------------------------------------------------------------------
// Installation window (game files).
// ---------------------------------------------------------------------------

class SetupForm : Form {
  readonly TextBox isoBox_ = new TextBox();
  readonly TextBox tuBox_ = new TextBox();
  readonly TextBox dlcBox_ = new TextBox();
  readonly ProgressBar progress_ = new ProgressBar();
  readonly Label status_ = new Label();
  readonly Button installButton_ = new Button();
  readonly Button cancelButton_ = new Button();
  readonly BackgroundWorker worker_ = new BackgroundWorker();
  volatile bool cancel_;

  static string T(string fr, string en) { return Lang.T(fr, en); }

  public SetupForm() {
    Text = T("Installation des fichiers du jeu", "Game files installation");
    FormBorderStyle = FormBorderStyle.FixedDialog;
    MaximizeBox = false;
    MinimizeBox = false;
    StartPosition = FormStartPosition.CenterParent;
    ClientSize = new Size(620, 360);
    Font = new Font("Segoe UI", 9f);

    var intro = new Label();
    intro.SetBounds(12, 10, 596, 64);
    intro.Text = T(
        "Le jeu n'est pas fourni : indiquez votre propre copie d'Asura's Wrath (Xbox 360). " +
        "Les fichiers sont extraits à côté du programme (environ 7 Go). " +
        "La mise à jour 1 (TU) est obligatoire, les DLC sont facultatifs.",
        "The game is not included: select your own copy of Asura's Wrath (Xbox 360). " +
        "Files are extracted next to the program (about 7 GB). " +
        "Title update 1 (TU) is required, DLC are optional.");
    Controls.Add(intro);

    AddPicker(T("Image disque (ISO) :", "Disc image (ISO):"), isoBox_, 80, false,
              "ISO (*.iso)|*.iso|" + T("Tous les fichiers", "All files") + "|*.*");
    AddPicker(T("Mise à jour 1 (fichier TU_...) :", "Title update 1 (TU_... file):"), tuBox_, 135,
              false, T("Tous les fichiers", "All files") + "|*.*");
    AddPicker(T("Dossier des DLC (facultatif) :", "DLC folder (optional):"), dlcBox_, 190, true,
              null);

    progress_.SetBounds(12, 255, 596, 22);
    progress_.Maximum = 1000;
    Controls.Add(progress_);
    status_.SetBounds(12, 282, 596, 36);
    Controls.Add(status_);

    installButton_.Text = T("Installer", "Install");
    installButton_.SetBounds(412, 322, 96, 28);
    installButton_.Click += delegate { StartInstall(); };
    Controls.Add(installButton_);
    cancelButton_.Text = T("Annuler", "Cancel");
    cancelButton_.SetBounds(514, 322, 94, 28);
    cancelButton_.Click += delegate {
      if (worker_.IsBusy) {
        cancel_ = true;
      } else {
        DialogResult = DialogResult.Cancel;
        Close();
      }
    };
    Controls.Add(cancelButton_);
    AcceptButton = installButton_;

    worker_.WorkerReportsProgress = true;
    worker_.DoWork += Work;
    worker_.ProgressChanged += delegate(object sender, ProgressChangedEventArgs e) {
      progress_.Value = Math.Max(0, Math.Min(1000, e.ProgressPercentage));
      if (e.UserState != null) status_.Text = (string)e.UserState;
    };
    worker_.RunWorkerCompleted += Completed;
    FormClosing += delegate(object sender, FormClosingEventArgs e) {
      if (worker_.IsBusy) {
        cancel_ = true;
        e.Cancel = true;
      }
    };
  }

  void AddPicker(string label, TextBox box, int top, bool folder, string filter) {
    var caption = new Label();
    caption.Text = label;
    caption.SetBounds(12, top, 596, 20);
    Controls.Add(caption);
    box.SetBounds(12, top + 22, 500, 24);
    Controls.Add(box);
    var browse = new Button();
    browse.Text = T("Parcourir…", "Browse…");
    browse.SetBounds(518, top + 21, 90, 26);
    browse.Click += delegate {
      if (folder) {
        using (var dialog = new FolderBrowserDialog()) {
          if (dialog.ShowDialog(this) == DialogResult.OK) box.Text = dialog.SelectedPath;
        }
      } else {
        using (var dialog = new OpenFileDialog()) {
          dialog.Filter = filter;
          if (dialog.ShowDialog(this) == DialogResult.OK) box.Text = dialog.FileName;
        }
      }
    };
    Controls.Add(browse);
  }

  class Job {
    public string Iso, Tu, Dlc, UserData;
  }

  void StartInstall() {
    var job = new Job();
    job.Iso = isoBox_.Text.Trim().Trim('"');
    job.Tu = tuBox_.Text.Trim().Trim('"');
    job.Dlc = dlcBox_.Text.Trim().Trim('"');
    job.UserData = Paths.UserData(new TomlFile(Paths.Toml));
    bool haveGame = File.Exists(Paths.Xex);
    if (!haveGame && !File.Exists(job.Iso)) {
      MessageBox.Show(this, T("Choisissez l'image disque (ISO) du jeu.",
                              "Select the game disc image (ISO)."), Text);
      return;
    }
    if (!File.Exists(Paths.Xexp) && !File.Exists(job.Tu)) {
      MessageBox.Show(this, T("Choisissez le fichier de la mise à jour 1 (TU_...).",
                              "Select the title update 1 file (TU_...)."), Text);
      return;
    }
    if (job.Dlc.Length > 0 && !Directory.Exists(job.Dlc)) {
      MessageBox.Show(this, T("Le dossier des DLC n'existe pas.", "The DLC folder doesn't exist."),
                      Text);
      return;
    }
    if (File.Exists(job.Iso)) {
      long needed = new FileInfo(job.Iso).Length;
      var drive = new DriveInfo(Path.GetPathRoot(Paths.AppDir));
      if (drive.AvailableFreeSpace < needed) {
        MessageBox.Show(this, T("Espace disque insuffisant : il faut ", "Not enough disk space: ") +
                                  (needed / 1000000000.0).ToString("0.0") + T(" Go.", " GB needed."),
                        Text);
        return;
      }
    }
    cancel_ = false;
    installButton_.Enabled = false;
    worker_.RunWorkerAsync(job);
  }

  void Report(int permille, string text) { worker_.ReportProgress(permille, text); }

  void Work(object sender, DoWorkEventArgs e) {
    var job = (Job)e.Argument;
    var result = new List<string>();

    // 1. Game disc.
    if (File.Exists(job.Iso)) {
      Report(0, T("Lecture de l'image disque…", "Reading the disc image…"));
      using (var stream = new FileStream(job.Iso, FileMode.Open, FileAccess.Read, FileShare.Read,
                                         1 << 20)) {
        var iso = new Xiso(stream);
        iso.Extract(Paths.Extracted, delegate(long done, long total, string path) {
          int permille = total > 0 ? (int)(done * 800 / total) : 0;
          worker_.ReportProgress(permille, T("Extraction : ", "Extracting: ") + path + "  (" +
                                               (done / 1000000000.0).ToString("0.00") + " / " +
                                               (total / 1000000000.0).ToString("0.00") +
                                               T(" Go)", " GB)"));
        }, delegate { return cancel_; });
      }
      result.Add(T("Jeu extrait.", "Game extracted."));
    }
    if (!File.Exists(Paths.Xex)) {
      throw new InvalidDataException(T("default.xex introuvable dans l'image disque.",
                                       "default.xex not found in the disc image."));
    }
    Report(810, T("Vérification de default.xex…", "Checking default.xex…"));
    if (KnownFiles.Md5(Paths.Xex) != KnownFiles.XexMd5) {
      result.Add(T("ATTENTION : default.xex ne correspond pas à la version attendue, le jeu risque de ne pas fonctionner.",
                   "WARNING: default.xex doesn't match the expected version, the game may not work."));
    }

    // 2. Title update.
    if (File.Exists(job.Tu)) {
      Report(840, T("Extraction de la mise à jour…", "Extracting the title update…"));
      using (var stream = new FileStream(job.Tu, FileMode.Open, FileAccess.Read)) {
        if (!new Stfs(stream).ExtractFile("default.xexp", Paths.Xexp)) {
          throw new InvalidDataException(T("default.xexp introuvable dans le paquet de mise à jour.",
                                           "default.xexp not found in the title update package."));
        }
      }
      if (KnownFiles.Md5(Paths.Xexp) != KnownFiles.XexpMd5) {
        File.Delete(Paths.Xexp);
        throw new InvalidDataException(T("Ce n'est pas la mise à jour 1 attendue (TU_11K620N_...).",
                                         "This is not the expected title update 1 (TU_11K620N_...)."));
      }
      result.Add(T("Mise à jour 1 installée.", "Title update 1 installed."));
    }

    // 3. DLC packages (installed by the game on its next start).
    if (job.Dlc.Length > 0) {
      string dlcDir = Path.Combine(job.UserData, "dlc");
      Directory.CreateDirectory(dlcDir);
      var files = new List<string>();
      foreach (string file in Directory.GetFiles(job.Dlc, "*", SearchOption.AllDirectories)) {
        if (Stfs.IsPackage(file)) files.Add(file);
      }
      int index = 0;
      foreach (string file in files) {
        if (cancel_) throw new OperationCanceledException();
        ++index;
        Report(860 + index * 130 / Math.Max(files.Count, 1),
               T("Copie des DLC : ", "Copying DLC: ") + index + "/" + files.Count);
        string dest = Path.Combine(dlcDir, Path.GetFileName(file));
        if (!File.Exists(dest) || new FileInfo(dest).Length != new FileInfo(file).Length) {
          File.Copy(file, dest, true);
        }
      }
      result.Add(files.Count + T(" paquet(s) de DLC copié(s), installés au prochain lancement du jeu.",
                                 " DLC package(s) copied, installed on the next game start."));
    }

    // 4. Texture replacement folders and readme.
    string textures = Path.Combine(job.UserData, "textures");
    Directory.CreateDirectory(Path.Combine(textures, "replace"));
    string readme = Path.Combine(Paths.AppDir, Lang.French ? "TEXTURE_MODDING.fr.md"
                                                           : "TEXTURE_MODDING.md");
    if (File.Exists(readme)) {
      File.Copy(readme, Path.Combine(textures, Lang.French ? "LISEZMOI.txt" : "README.txt"), true);
    }
    Report(1000, T("Terminé.", "Done."));
    e.Result = string.Join("\r\n", result.ToArray());
  }

  void Completed(object sender, RunWorkerCompletedEventArgs e) {
    installButton_.Enabled = true;
    if (e.Error != null) {
      string message = e.Error is OperationCanceledException
                           ? T("Installation annulée.", "Installation cancelled.")
                           : T("Échec de l'installation : ", "Installation failed: ") + e.Error.Message;
      status_.Text = message;
      MessageBox.Show(this, message, Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
      return;
    }
    MessageBox.Show(this, (string)e.Result, Text, MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
    DialogResult = DialogResult.OK;
    Close();
  }
}

// ---------------------------------------------------------------------------
// Main window.
// ---------------------------------------------------------------------------

class MainForm : Form {
  readonly List<SettingsPage> pages_ = SettingsCatalog.Build();
  readonly ToolTip toolTip_ = new ToolTip();
  readonly Label gameStatus_ = new Label();
  readonly Button playButton_ = new Button();
  TomlFile toml_;

  static string T(string fr, string en) { return Lang.T(fr, en); }

  public MainForm() {
    Text = "Asura's Wrath Recompiled";
    FormBorderStyle = FormBorderStyle.FixedSingle;
    MaximizeBox = false;
    StartPosition = FormStartPosition.CenterScreen;
    ClientSize = new Size(560, 560);
    Font = new Font("Segoe UI", 9f);
    try {
      Icon = Icon.ExtractAssociatedIcon(File.Exists(Paths.GameExe) ? Paths.GameExe
                                                                   : Application.ExecutablePath);
    } catch (Exception) {
    }
    toolTip_.AutoPopDelay = 15000;

    var tabs = new TabControl();
    tabs.SetBounds(10, 10, 540, 450);
    Controls.Add(tabs);
    foreach (SettingsPage page in pages_) tabs.TabPages.Add(BuildPage(page));
    tabs.TabPages.Add(BuildInfoPage());

    gameStatus_.SetBounds(12, 468, 536, 36);
    Controls.Add(gameStatus_);

    var defaults = new Button();
    defaults.Text = T("Par défaut", "Defaults");
    defaults.SetBounds(10, 515, 100, 32);
    defaults.Click += delegate {
      foreach (SettingsPage page in pages_)
        foreach (Setting s in page.Settings) s.Load(null);
    };
    Controls.Add(defaults);

    var save = new Button();
    save.Text = T("Enregistrer", "Save");
    save.SetBounds(330, 515, 100, 32);
    save.Click += delegate { SaveSettings(true); };
    Controls.Add(save);

    playButton_.Text = T("Jouer", "Play");
    playButton_.Font = new Font("Segoe UI", 10f, FontStyle.Bold);
    playButton_.SetBounds(440, 515, 110, 32);

    // Launcher language (restarts the launcher).
    var language = new ComboBox();
    language.DropDownStyle = ComboBoxStyle.DropDownList;
    language.Items.AddRange(new object[] {"Français", "English"});
    language.SelectedIndex = Lang.French ? 0 : 1;
    language.SetBounds(120, 519, 110, 24);
    language.SelectedIndexChanged += delegate {
      bool french = language.SelectedIndex == 0;
      if (french == Lang.French) return;
      try {
        SaveSettings(false);
        Lang.SaveLanguage(french);
        Application.Restart();
      } catch (Exception ex) {
        MessageBox.Show(this, ex.Message, Text);
      }
    };
    Controls.Add(language);
    toolTip_.SetToolTip(language, T("Langue du launcher", "Launcher language"));
    playButton_.Click += delegate { Play(); };
    Controls.Add(playButton_);
    AcceptButton = playButton_;

    LoadSettings();
    Shown += delegate {
      if (!Paths.GameFilesPresent()) RunSetup();
    };
  }

  Label AddText(TabPage tab, string text, int top, Color color) {
    var label = new Label();
    label.Text = text;
    label.ForeColor = color;
    label.AutoSize = false;
    Size size = TextRenderer.MeasureText(text, Font, new Size(470, 0),
                                         TextFormatFlags.WordBreak);
    label.SetBounds(34, top, 470, size.Height + 2);
    tab.Controls.Add(label);
    return label;
  }

  TabPage BuildPage(SettingsPage page) {
    var tab = new TabPage(page.Title);
    tab.AutoScroll = true;
    int top = 12;
    foreach (Setting s in page.Settings) {
      if (s.Kind == SettingKind.Bool) {
        var check = new CheckBox();
        check.Text = s.Label;
        check.SetBounds(16, top, 490, 24);
        s.Editor = check;
        tab.Controls.Add(check);
        top += 26;
      } else {
        var label = new Label();
        label.Text = s.Label;
        label.SetBounds(16, top + 4, 220, 20);
        tab.Controls.Add(label);
        Control editor;
        if (s.Kind == SettingKind.Choice) {
          var combo = new ComboBox();
          combo.DropDownStyle = ComboBoxStyle.DropDownList;
          combo.Items.AddRange(s.Names);
          editor = combo;
        } else {
          editor = new TextBox();
        }
        editor.SetBounds(240, top, 270, 24);
        s.Editor = editor;
        tab.Controls.Add(editor);
        top += 30;
      }
      if (s.Tip.Length > 0) {
        Label tip = AddText(tab, s.Tip, top, SystemColors.GrayText);
        top += tip.Height + 2;
      }
      if (s.Warning != null) {
        Label warning = AddText(tab, s.Warning, top, Color.Firebrick);
        warning.Visible = false;
        top += warning.Height + 2;
        Setting setting = s;
        EventHandler update = delegate { warning.Visible = setting.Literal() != setting.Default; };
        var combo = s.Editor as ComboBox;
        if (combo != null) combo.SelectedIndexChanged += update;
        var check = s.Editor as CheckBox;
        if (check != null) check.CheckedChanged += update;
      }
      top += 8;
    }
    if (page.Title == "Textures") {
      top += 10;
      var replace = new Button();
      replace.Text = T("Ouvrir le dossier des textures", "Open the textures folder");
      replace.SetBounds(16, top, 240, 28);
      replace.Click += delegate {
        string dir = Path.Combine(Paths.UserData(toml_), "textures");
        Directory.CreateDirectory(Path.Combine(dir, "replace"));
        Process.Start("explorer.exe", "\"" + dir + "\"");
      };
      tab.Controls.Add(replace);
    }
    return tab;
  }

  TabPage BuildInfoPage() {
    var tab = new TabPage(T("Infos", "About"));
    var text = new Label();
    text.SetBounds(16, 14, 500, 220);
    text.Text = T(
        "Touches en jeu :\r\n" +
        "  F4  réglages en jeu\r\n" +
        "  F6  30 / 60 FPS\r\n" +
        "  F8  turbo activé / désactivé\r\n" +
        "  F9  menu de triche\r\n" +
        "  Alt+Entrée  plein écran\r\n\r\n" +
        "Les réglages sont enregistrés dans asura_wrath_recomp.toml (à côté du jeu). " +
        "Les sauvegardes et les journaux sont dans le dossier de données.",
        "In-game keys:\r\n" +
        "  F4  in-game settings\r\n" +
        "  F6  30 / 60 FPS\r\n" +
        "  F8  turbo on / off\r\n" +
        "  F9  cheat menu\r\n" +
        "  Alt+Enter  fullscreen\r\n\r\n" +
        "Settings are stored in asura_wrath_recomp.toml (next to the game). " +
        "Saves and logs are in the data folder.");
    tab.Controls.Add(text);

    var data = new Button();
    data.Text = T("Ouvrir le dossier de données", "Open the data folder");
    data.SetBounds(16, 245, 240, 28);
    data.Click += delegate {
      string dir = Paths.UserData(toml_);
      Directory.CreateDirectory(dir);
      Process.Start("explorer.exe", "\"" + dir + "\"");
    };
    tab.Controls.Add(data);

    var setup = new Button();
    setup.Text = T("Installer / réparer les fichiers du jeu…", "Install / repair the game files…");
    setup.SetBounds(16, 280, 300, 28);
    setup.Click += delegate { RunSetup(); };
    tab.Controls.Add(setup);
    return tab;
  }

  void RunSetup() {
    using (var setup = new SetupForm()) setup.ShowDialog(this);
    UpdateStatus();
  }

  void UpdateStatus() {
    bool ok = Paths.GameFilesPresent();
    bool exe = File.Exists(Paths.GameExe);
    gameStatus_.ForeColor = ok && exe ? Color.DarkGreen : Color.DarkRed;
    if (!exe) {
      gameStatus_.Text = T("asura_wrath_recomp.exe est introuvable à côté du launcher.",
                           "asura_wrath_recomp.exe was not found next to the launcher.");
    } else if (!ok) {
      gameStatus_.Text = T("Fichiers du jeu absents : onglet Infos → Installer.",
                           "Game files missing: About tab → Install.");
    } else {
      gameStatus_.Text = T("Fichiers du jeu installés.", "Game files installed.");
    }
    playButton_.Enabled = ok && exe;
  }

  void LoadSettings() {
    bool created = !File.Exists(Paths.Toml);
    toml_ = new TomlFile(Paths.Toml);
    foreach (SettingsPage page in pages_) {
      foreach (Setting s in page.Settings) {
        s.Load(toml_.Get(s.Key));
      }
    }
    if (created) SaveSettings(false);
    UpdateStatus();
  }

  bool SaveSettings(bool notify) {
    try {
      toml_ = new TomlFile(Paths.Toml);
      foreach (SettingsPage page in pages_)
        foreach (Setting s in page.Settings) toml_.Set(s.Key, s.Literal());
      toml_.Save();
      if (notify) {
        MessageBox.Show(this, T("Réglages enregistrés.", "Settings saved."), Text);
      }
      return true;
    } catch (Exception ex) {
      MessageBox.Show(this, T("Impossible d'enregistrer les réglages : ", "Can't save the settings: ") +
                                ex.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
      return false;
    }
  }

  void Play() {
    if (!SaveSettings(false)) return;
    try {
      var start = new ProcessStartInfo(Paths.GameExe);
      start.WorkingDirectory = Paths.AppDir;
      start.UseShellExecute = false;
      Process.Start(start);
      Close();
    } catch (Exception ex) {
      MessageBox.Show(this, ex.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
    }
  }
}

static class Program {
  [STAThread]
  static void Main() {
    Application.EnableVisualStyles();
    Application.SetCompatibleTextRenderingDefault(false);
    Application.Run(new MainForm());
  }
}

}  // namespace AsuraLauncher
