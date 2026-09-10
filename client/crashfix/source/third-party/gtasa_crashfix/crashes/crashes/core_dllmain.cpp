#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "crashes.h"

static DWORD WINAPI InstallCrashGuards(void*)
{
    // Match upstream's startup point without applying its unrelated gameplay,
    // rendering, updater, fast-loader, windowed-mode or reporting features.
    for (unsigned i = 0; i < 300 && *reinterpret_cast<volatile int*>(0xB6F5F0) == 0; ++i)
        Sleep(100);

    DWORD oldProtect = 0;
    if (VirtualProtect(reinterpret_cast<void*>(0x401000), 0x4A3000,
                       PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        InitHooks_CrashFixHacks();
        DWORD ignored = 0;
        VirtualProtect(reinterpret_cast<void*>(0x401000), 0x4A3000,
                       oldProtect, &ignored);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(0x401000), 0x4A3000);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, InstallCrashGuards, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
