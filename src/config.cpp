#include "config.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "files.h"
#include "log.h"

namespace re5cc::config {
namespace {

Settings g_settings;
char g_path[MAX_PATH] = {};
char g_dir[MAX_PATH] = {};
// Watch and Disable as the ini has them, for a rewrite (ini_text).
std::string g_watch_text, g_disable_text;

struct SwitchDef {
  const char* key;
  const char* help;
};
constexpr SwitchDef kSwitches[sw::kCount] = {
#define RE5CC_DEF(name, key, help) {key, help},
    RE5CC_SWITCHES(RE5CC_DEF)
#undef RE5CC_DEF
};

bool truthy(const char* v) {
  return !_stricmp(v, "1") || !_stricmp(v, "on") || !_stricmp(v, "true") || !_stricmp(v, "yes");
}

// The ini: every option documented, then the values the settings hold - the
// defaults on the first start.
std::string ini_text() {
  std::string out =
      "# RE5CabbyCodes\n"
      "#\n"
      "#   ToggleKey   = 0x76  ; the key that hides/shows the panel (0x76 = F7); in play it opens the\n"
      "#                       ; cheats over the game\n"
      "#   <cheat>     = 0     ; each cheat's state when the game starts - the panel writes every change\n"
      "#                       ; back here:\n";
  char line[256];
  for (const SwitchDef& s : kSwitches) {
    std::snprintf(line, sizeof(line), "#     %-20s %s\n", s.key, s.help);
    out += line;
  }
  out +=
      "#   AlwaysShow  = 0     ; debug: draw the panel everywhere, not only on its screens\n"
      "#   Trace       = 0     ; verbose diagnostics in RE5CabbyCodes.log\n"
      "#   DumpImage   = 0     ; reverse engineering: write the unpacked re5dx9.exe and its import map\n"
      "#                       ; beside the DLL once (re5dx9.dumped.exe, re5dx9.imports.txt)\n"
      "#   Watch       =       ; debug: 0x846F9C:2,0x80A99:1 - log those values (offsets from\n"
      "#                       ; re5dx9.exe's base) as they change\n"
      "#   Disable     =       ; comma list of subsystems to turn off: overlay,tick,game,input\n";
  std::snprintf(line, sizeof(line), "ToggleKey = 0x%02X\n", g_settings.toggle_key);
  out += line;
  for (int i = 0; i < sw::kCount; ++i) {
    std::snprintf(line, sizeof(line), "%s = %d\n", kSwitches[i].key, g_settings.on[i] ? 1 : 0);
    out += line;
  }
  std::snprintf(line, sizeof(line), "AlwaysShow = %d\nTrace = %d\nDumpImage = %d\n", g_settings.always_show ? 1 : 0,
                g_settings.trace ? 1 : 0, g_settings.dump_image ? 1 : 0);
  out += line;
  out += "Watch =" + (g_watch_text.empty() ? std::string() : " " + g_watch_text) + "\n";
  out += "Disable =" + (g_disable_text.empty() ? std::string() : " " + g_disable_text) + "\n";
  return out;
}

// `text` into the ini: written beside it and moved over it, so a crash halfway
// leaves the old file rather than half of a new one.
bool write_ini(const std::string& text, DWORD* err) {
  char tmp[MAX_PATH + 8];
  std::snprintf(tmp, sizeof(tmp), "%s.tmp", g_path);
  FILE* f = std::fopen(tmp, "w");  // text: Windows' line ends, as the first start's
  if (!f) {
    *err = GetLastError();
    return false;
  }
  const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
  if (std::fclose(f) != 0 || !ok || !files::replace(tmp, g_path, MOVEFILE_REPLACE_EXISTING, err)) {
    DeleteFileA(tmp);
    return false;
  }
  return true;
}

// "0x846F9C:2, 0x80A99:1" -> the watch list (sizes 1, 2 or 4; 4 when omitted).
void parse_watch(const char* val) {
  g_settings.nwatch = 0;
  const char* p = val;
  while (*p && g_settings.nwatch < kMaxWatch) {
    while (*p == ' ' || *p == '\t' || *p == ',') ++p;
    if (!*p) break;
    char* end = nullptr;
    const unsigned long rva = std::strtoul(p, &end, 0);
    if (end == p) break;
    p = end;
    unsigned size = 4;
    if (*p == ':') {
      size = static_cast<unsigned>(std::strtoul(p + 1, &end, 0));
      p = end;
    }
    if (rva && (size == 1 || size == 2 || size == 4)) {
      g_settings.watch[g_settings.nwatch].rva = static_cast<unsigned>(rva);
      g_settings.watch[g_settings.nwatch].size = size;
      ++g_settings.nwatch;
    }
    while (*p && *p != ',') ++p;
  }
}

// "  GodMode = 1 ; comment" -> whether the line sets `key` (any case, with
// spaces either side of the name, as the parser takes it).
bool line_sets(const char* line, const char* key) {
  while (*line == ' ' || *line == '\t') ++line;
  const size_t n = std::strlen(key);
  if (_strnicmp(line, key, n)) return false;
  line += n;
  while (*line == ' ' || *line == '\t') ++line;
  return *line == '=';
}

}  // namespace

bool save_switches(const bool (&on)[sw::kCount]) {
  if (!g_path[0]) return false;
  bool written[sw::kCount] = {};
  std::string out;
  if (FILE* f = std::fopen(g_path, "r")) {
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
      const char* p = line;
      while (*p == ' ' || *p == '\t') ++p;
      bool replaced = false;
      if (*p != '#' && *p != ';')
        for (int i = 0; i < sw::kCount; ++i)
          if (line_sets(p, kSwitches[i].key)) {
            // A second line for the same key would win on the next load, so it
            // gets the value too.
            out += kSwitches[i].key;
            out += on[i] ? " = 1\n" : " = 0\n";
            written[i] = replaced = true;
            break;
          }
      if (!replaced) {
        out += line;
        if (!out.empty() && out.back() != '\n') out += '\n';
      }
    }
    std::fclose(f);
  }
  for (int i = 0; i < sw::kCount; ++i) {
    g_settings.on[i] = on[i];
    if (written[i]) continue;
    out += kSwitches[i].key;
    out += on[i] ? " = 1\n" : " = 0\n";
  }
  DWORD err = 0;
  if (!write_ini(out, &err)) {
    logf("config: could not save the cheat switches to %s (error %lu)", g_path, static_cast<unsigned long>(err));
    return false;
  }
  return true;
}

const Settings& get() { return g_settings; }
const char* dir() { return g_dir; }

void load(const char* dir) {
  std::snprintf(g_dir, sizeof(g_dir), "%s", dir);
  std::snprintf(g_path, sizeof(g_path), "%sRE5CabbyCodes.ini", dir);
  FILE* f = std::fopen(g_path, "r");
  if (!f) {
    DWORD err = 0;
    write_ini(ini_text(), &err);
    logf("config: no %s - wrote one with the defaults", g_path);
    return;
  }
  // An ini from before a switch was added or retired is written again (the
  // documentation, then its values): the retired keys gone, the new ones in.
  bool seen[sw::kCount] = {};
  std::string retired;
  char line[512];
  while (std::fgets(line, sizeof(line), f)) {
    char* p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '#' || *p == ';' || *p == '[' || *p == '\n' || *p == '\r' || !*p) continue;
    char* eq = std::strchr(p, '=');
    if (!eq) continue;
    *eq = '\0';
    char* key = p;
    char* val = eq + 1;
    for (char* e = key + std::strlen(key); e > key && (e[-1] == ' ' || e[-1] == '\t');) *--e = '\0';
    while (*val == ' ' || *val == '\t') ++val;
    // A trailing "; comment" is the file's own documentation style.
    if (char* c = std::strchr(val, ';')) *c = '\0';
    for (char* e = val + std::strlen(val);
         e > val && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t');)
      *--e = '\0';

    bool known = false;
    for (int i = 0; i < sw::kCount; ++i)
      if (!_stricmp(key, kSwitches[i].key)) {
        g_settings.on[i] = truthy(val);
        seen[i] = known = true;
      }
    for (const char* r : sw::kRetired)
      if (!known && !_stricmp(key, r)) {
        retired += retired.empty() ? r : std::string(", ") + r;
        known = true;
      }
    if (known) continue;
    if (!_stricmp(key, "ToggleKey")) {
      const int v = static_cast<int>(std::strtol(val, nullptr, 0));
      if (v > 0 && v < 256) g_settings.toggle_key = v;
    } else if (!_stricmp(key, "AlwaysShow")) {
      g_settings.always_show = truthy(val);
    } else if (!_stricmp(key, "Trace")) {
      g_settings.trace = truthy(val);
    } else if (!_stricmp(key, "DumpImage")) {
      g_settings.dump_image = truthy(val);
    } else if (!_stricmp(key, "Watch")) {
      g_watch_text = val;
      parse_watch(val);
    } else if (!_stricmp(key, "Disable")) {
      g_disable_text = val;
      g_settings.disable_overlay = std::strstr(val, "overlay") != nullptr;
      g_settings.disable_tick = std::strstr(val, "tick") != nullptr;
      g_settings.disable_game = std::strstr(val, "game") != nullptr;
      g_settings.disable_input = std::strstr(val, "input") != nullptr;
    } else {
      logf("config: unknown key '%s' ignored", key);
    }
  }
  std::fclose(f);
  std::string added;
  for (int i = 0; i < sw::kCount; ++i)
    if (!seen[i]) added += (added.empty() ? "" : ", ") + std::string(kSwitches[i].key);
  if (!retired.empty() || !added.empty()) {
    DWORD err = 0;
    if (write_ini(ini_text(), &err))
      logf("config: %s written again for this version, its values kept%s%s%s%s", g_path,
           retired.empty() ? "" : " - no longer a switch: ", retired.c_str(), added.empty() ? "" : " - new: ",
           added.c_str());
    else
      logf("config: %s could not be written again for this version (error %lu) - it works as it is", g_path,
           static_cast<unsigned long>(err));
  }
  char on[512] = "";
  for (int i = 0; i < sw::kCount; ++i) {
    const size_t at = std::strlen(on);
    std::snprintf(on + at, sizeof(on) - at, "%s%s=%d", i ? " " : "", kSwitches[i].key, g_settings.on[i] ? 1 : 0);
  }
  logf("config: ToggleKey=0x%02X %s AlwaysShow=%d Trace=%d DumpImage=%d Watch=%d value(s) Disable=%s%s%s%s",
       g_settings.toggle_key, on, g_settings.always_show, g_settings.trace, g_settings.dump_image, g_settings.nwatch,
       g_settings.disable_overlay ? "overlay " : "", g_settings.disable_tick ? "tick " : "",
       g_settings.disable_game ? "game " : "", g_settings.disable_input ? "input" : "");
}

}  // namespace re5cc::config
