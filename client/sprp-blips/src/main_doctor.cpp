// doctor-valkyrie: the crash log that explains itself.
//
// Loads early so it sees the game's file and library activity from the start,
// then re-asserts its exception filter once the game window exists so that
// load order between .asi files does not decide who reports the crash.
#include <windows.h>

#include "doctor.h"
#include "log.h"
#include "version.h"

namespace {

// The game's window, which is the cheapest "the game is actually up" signal
// there is. We wait on the condition rather than on a fixed delay: a slow
// machine loading a hundred mods must not race us, and a fast one should not
// be made to wait.
bool GameIsUp() {
    return FindWindowA("Grand theft auto San Andreas", nullptr) != nullptr ||
           FindWindowA(nullptr, "GTA: San Andreas") != nullptr;
}

DWORD WINAPI Start(LPVOID) {
    logfile::Open("doctor-valkyrie.log", "doctor-valkyrie");
    logfile::Line("doctor-valkyrie %s", DOCTOR_VALKYRIE_VERSION);

    doctor::Install();

    // Keep Doctor as the active fatal-crash reporter for this launch. Some
    // plugins install a new filter long after the loading screen; reasserting
    // prevents a later reporter from replacing Doctor's single crash window.
    while (WaitForSingleObject(GetCurrentProcess(), 500) == WAIT_TIMEOUT) {
        if (GameIsUp()) {
            doctor::Reassert();
            doctor::CheckCleoErrors();
        }
    }
    return 0;
}

}  // namespace

extern "C" BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        if (HANDLE thread = CreateThread(nullptr, 0, &Start, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
