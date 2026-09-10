#include "switches.h"

#include <windows.h>

#include <cstring>

#include "log.h"

namespace switches {
namespace {

bool g_anims = false;
bool g_manhunt = false;
bool g_zoom = false;

bool Present(const char* name) {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (char* slash = strrchr(path, '\\')) {
        slash[1] = '\0';
    }
    strncat_s(path, name, _TRUNCATE);
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

}  // namespace

void Read() {
    g_anims = Present("sprp-noanims.off");
    g_manhunt = Present("sprp-nomanhunt.off");
    g_zoom = Present("sprp-nozoom.off");

    if (g_anims || g_manhunt || g_zoom) {
        logfile::Line("switches: off ->%s%s%s",
                      g_anims ? " animations" : "",
                      g_manhunt ? " manhunt" : "",
                      g_zoom ? " map zoom" : "");
    } else {
        logfile::Line("switches: everything on");
    }
}

void ReadMap() {
    g_zoom = Present("sprp-nozoom.off");
    logfile::Line("switches: map %s", g_zoom ? "off" : "on");
}

void ReadAnimations() {
    g_anims = Present("sprp-noanims.off");
    g_manhunt = Present("sprp-nomanhunt.off");
    logfile::Line("switches: animations %s, Manhunt set %s",
                  g_anims ? "off" : "on", g_manhunt ? "off" : "on");
}

bool AnimsOff() { return g_anims; }
bool ManhuntOff() { return g_manhunt; }
bool ZoomOff() { return g_zoom; }

}  // namespace switches
