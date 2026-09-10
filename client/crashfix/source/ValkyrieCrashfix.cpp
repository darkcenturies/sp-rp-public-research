#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

using QWORD = unsigned long long;
using GetStreamFn = DWORD (__stdcall *)(int, int);
using GetPosFn = QWORD (__stdcall *)(DWORD, DWORD);
using GetLenFn = QWORD (__stdcall *)(DWORD, DWORD);
using SetPosFn = BOOL (__stdcall *)(DWORD, QWORD, DWORD);
using BytesToSecondsFn = double (__stdcall *)(DWORD, QWORD);
using SecondsToBytesFn = QWORD (__stdcall *)(DWORD, double);
using ErrorFn = int (__stdcall *)();

static HMODULE selfModule;
static GetStreamFn getStream;
static GetPosFn getPosition;
static GetLenFn getLength;
static SetPosFn setPosition;
static BytesToSecondsFn bytesToSeconds;
static SecondsToBytesFn secondsToBytes;
static ErrorFn getError;
static CRITICAL_SECTION logLock;
static volatile LONG shuttingDown;
static void* poolTrampoline;
static bool patchProtectionFailure=false;

struct Hook { const char* name; BYTE* address; size_t length; BYTE original[16]; BYTE patched[16]; };
static std::vector<Hook> installed;

static std::string LogPath() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(selfModule, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) strcpy_s(slash + 1, MAX_PATH - (slash + 1 - path), "Valkyrie Crashfix.log");
    return path;
}

static void Log(const char* fmt, ...) {
    EnterCriticalSection(&logLock);
    FILE* f = nullptr;
    fopen_s(&f, LogPath().c_str(), "a");
    if (f) {
        SYSTEMTIME st{}; GetLocalTime(&st);
        fprintf(f, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
        fputc('\n', f); fclose(f);
    }
    LeaveCriticalSection(&logLock);
}

static bool Readable(const void* p, size_t bytes) {
    if (!p || bytes == 0) return false;
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery(p, &m, sizeof(m)) || m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if (!(m.Protect & readable)) return false;
    uintptr_t a = reinterpret_cast<uintptr_t>(p), end = a + bytes;
    return end >= a && end <= reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
}

static bool Writable(const void* p, size_t bytes) {
    if (!Readable(p, bytes)) return false;
    MEMORY_BASIC_INFORMATION m{}; VirtualQuery(p, &m, sizeof(m));
    return (m.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

static int BassError() { return getError ? getError() : -1; }
static DWORD ResolveStream(int id, const char* op) {
    if (!getStream) return 0;
    DWORD h = getStream(0, id);
    if (!h) Log("%s: CLEO stream %d is missing", op, id);
    return h;
}

static bool Duration(DWORD h, double& seconds, QWORD* bytes = nullptr) {
    if (!h || !getLength || !bytesToSeconds) return false;
    QWORD length = getLength(h, 0);
    if (length == ~QWORD(0)) { Log("BASS_ChannelGetLength failed: handle=%08lX error=%d", h, BassError()); return false; }
    double s = bytesToSeconds(h, length);
    if (!std::isfinite(s) || s <= 0.0 || s > 864000.0) { Log("Invalid duration: handle=%08lX bytes=%llu seconds=%.6f error=%d", h, length, s, BassError()); return false; }
    seconds = s; if (bytes) *bytes = length; return true;
}

static bool GetSeconds(int id, float* out, const char* op, bool lengthOnly) {
    if (!Writable(out, sizeof(float))) { Log("%s: output pointer rejected", op); return false; }
    *out = 0.0f;
    DWORD h = ResolveStream(id, op); if (!h) return false;
    double duration = 0.0; QWORD lengthBytes = 0;
    if (!Duration(h, duration, &lengthBytes)) return false;
    QWORD value = lengthOnly ? lengthBytes : getPosition(h, 0);
    if (value == ~QWORD(0)) { Log("%s: BASS position failed handle=%08lX error=%d", op, h, BassError()); return false; }
    double seconds = lengthOnly ? duration : bytesToSeconds(h, value);
    if (!std::isfinite(seconds) || seconds < 0.0) { Log("%s: invalid result %.6f", op, seconds); return false; }
    if (seconds > duration) seconds = duration;
    *out = static_cast<float>(seconds);
    return true;
}

extern "C" int __cdecl Hook3210A(int opcode, int streamId, float* out) { return opcode == 0x3210 && GetSeconds(streamId, out, "3210-A", false); }
extern "C" int __cdecl Hook3210B(int opcode, int streamId, float* out) { return opcode == 0x3210 && GetSeconds(streamId, out, "3210-B", false); }
extern "C" int __cdecl Hook3212(int opcode, int streamId, float* out) { return opcode == 0x3212 && GetSeconds(streamId, out, "3212", true); }
extern "C" int __cdecl Hook3211(int opcode, int streamId, float seconds) {
    if (opcode != 0x3211 || !std::isfinite(seconds)) { Log("3211: rejected opcode/time"); return 0; }
    DWORD h = ResolveStream(streamId, "3211"); if (!h) return 0;
    double duration = 0.0; if (!Duration(h, duration)) return 0;
    double safe = std::clamp(static_cast<double>(seconds), 0.0, duration);
    QWORD pos = secondsToBytes(h, safe);
    if (pos == ~QWORD(0)) { Log("3211: seconds conversion failed handle=%08lX error=%d", h, BassError()); return 0; }
    if (!setPosition(h, pos, 0)) { Log("3211: set position failed handle=%08lX pos=%llu error=%d", h, pos, BassError()); return 0; }
    return 1;
}

struct Pattern { const BYTE* bytes; const char* mask; size_t size; int callOffset; const char* name; void* hook; };
static BYTE* FindUnique(BYTE* base, size_t size, const Pattern& p) {
    BYTE* found = nullptr;
    for (size_t i = 0; i + p.size <= size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < p.size; ++j) if (p.mask[j] == 'x' && base[i+j] != p.bytes[j]) { ok=false; break; }
        if (ok) { if (found) return nullptr; found = base + i + p.callOffset; }
    }
    return found;
}

static bool TextSection(HMODULE module, BYTE*& base, size_t& size) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    if (!Readable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<BYTE*>(module) + dos->e_lfanew);
    if (!Readable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) return false;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i=0;i<nt->FileHeader.NumberOfSections;i++,sec++) if (!memcmp(sec->Name, ".text", 5)) { base=reinterpret_cast<BYTE*>(module)+sec->VirtualAddress; size=sec->Misc.VirtualSize; return true; }
    return false;
}

static bool MakeCall(Hook& h, BYTE* at, void* target, const char* name) {
    if (!Readable(at, 5) || at[0] != 0xE8) return false;
    h.name=name; h.address=at; h.length=5; memcpy(h.original, at, 5); h.patched[0]=0xE8;
    DWORD rel=DWORD(target)-DWORD(at+5); memcpy(h.patched+1,&rel,4); return true;
}

#include "SafePatch.inc"

extern "C" bool __cdecl PoolReadable(const void* pool) {
    bool ok = Writable(pool, 16);
    if (!ok) Log("GTA audio-adjacent pool guard blocked invalid ECX=%p at 0x0040FB80", pool);
    return ok;
}

__declspec(naked) static void PoolGuard() {
    __asm {
        pushfd
        pushad
        mov ebp, esp
        sub esp, 528
        and esp, 0FFFFFFF0h
        fxsave [esp]
        push ecx
        call PoolReadable
        add esp, 4
        fxrstor [esp]
        mov esp, ebp
        test al, al
        jnz valid
        popad
        popfd
        xor eax, eax
        ret
    valid:
        popad
        popfd
        jmp dword ptr [poolTrampoline]
    }
}

static bool PreparePoolGuard(Hook& hook) {
    HMODULE exe=GetModuleHandleA(nullptr);
    // The crash report names 0x0040FB80. GTA contains many allocator templates with
    // the same prefix, so this guard intentionally binds to that RVA and then verifies
    // a longer body. Pattern-only selection would risk guarding the wrong pool.
    BYTE* at=reinterpret_cast<BYTE*>(exe)+0xFB80;
    const BYTE sig[]={0x8B,0x51,0x08,0x56,0x32,0xC0,0x57,0x8B,0x79,0x0C,0x47,0x8B,0xF7,0x3B,0xF2,0x89,0x79,0x0C,0x75,0x0D,0x84,0xC0,0xC7,0x41,0x0C,0,0,0,0};
    if(!Readable(at,sizeof(sig)) || memcmp(at,sig,sizeof(sig))){ Log("GTA 0x0040FB80 body does not match the supported build; pool guard skipped"); return false; }
    poolTrampoline=VirtualAlloc(nullptr,16,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE); if(!poolTrampoline) return false;
    memcpy(poolTrampoline,at,6); BYTE* t=static_cast<BYTE*>(poolTrampoline); t[6]=0xE9; int32_t back=static_cast<int32_t>((at+6)-(t+11)); memcpy(t+7,&back,4); FlushInstructionCache(GetCurrentProcess(),t,11);
    hook.name="GTA 0x0040FB80 pool guard"; hook.address=at; hook.length=6; memcpy(hook.original,at,6); hook.patched[0]=0xE9; int32_t rel=static_cast<int32_t>(reinterpret_cast<BYTE*>(&PoolGuard)-(at+5)); memcpy(hook.patched+1,&rel,4); hook.patched[5]=0x90; return true;
}

static FARPROC Proc(HMODULE m, const char* n) { return m ? GetProcAddress(m,n) : nullptr; }
#include "ActorGuard.inc"
#include "IncidentGuards.inc"
#include "PrepareGuards.inc"

static DWORD WINAPI Bootstrap(void*) {
    Log("Valkyrie Crashfix 2.3.0-test starting (original RadioEx fix discovery and v1: konradzimorski)");
    HMODULE pe=nullptr, cleo=nullptr, bass=nullptr;
    for(int i=0;i<240 && !InterlockedCompareExchange(&shuttingDown,0,0);++i) {
        pe=GetModuleHandleA("PECore.asi"); cleo=GetModuleHandleA("CLEO.asi"); bass=GetModuleHandleA("bass.dll");
        if(pe&&cleo&&bass) break; Sleep(250);
    }
    if(!pe||!cleo||!bass){ Log("Modules unavailable: PECore=%p CLEO=%p BASS=%p",pe,cleo,bass); return 0; }
    getStream=reinterpret_cast<GetStreamFn>(Proc(cleo,"_CLEO_GetInternalAudioStream@8"));
    getPosition=reinterpret_cast<GetPosFn>(Proc(bass,"BASS_ChannelGetPosition")); getLength=reinterpret_cast<GetLenFn>(Proc(bass,"BASS_ChannelGetLength"));
    setPosition=reinterpret_cast<SetPosFn>(Proc(bass,"BASS_ChannelSetPosition")); bytesToSeconds=reinterpret_cast<BytesToSecondsFn>(Proc(bass,"BASS_ChannelBytes2Seconds"));
    secondsToBytes=reinterpret_cast<SecondsToBytesFn>(Proc(bass,"BASS_ChannelSeconds2Bytes")); getError=reinterpret_cast<ErrorFn>(Proc(bass,"BASS_ErrorGetCode"));
    if(!getStream||!getPosition||!getLength||!setPosition||!bytesToSeconds||!secondsToBytes){ Log("Required CLEO/BASS API missing"); return 0; }
    BYTE* text=nullptr; size_t textSize=0; if(!TextSection(pe,text,textSize)){Log("PECore .text unavailable");return 0;}
    static const BYTE a[]={0x68,0x10,0x32,0,0,0xE8,0xFF,0xC5,0xFF,0xFF,0xD9,0x45,0xFC};
    static const BYTE b[]={0x68,0x10,0x32,0,0,0xE8,0xB4,0x90,0xFF,0xFF,0xF3,0x0F,0x10,0x45,0xB8};
    static const BYTE c[]={0x68,0x12,0x32,0,0,0xE8,0xE2,0xA0,0xFF,0xFF,0xF3,0x0F,0x10,0x45,0xC8};
    static const BYTE d[]={0x68,0x11,0x32,0,0,0xE8,0x38,0x10,0xFC,0xFF,0x83,0xC4,0x0C};
    // The rel32 bytes are deliberately wildcarded: harmless PECore relinks may move both
    // caller and callee while leaving the surrounding RadioEx operation unchanged.
    Pattern patterns[]={ {a,"xxxxxx????xxx",sizeof(a),5,"3210-A",(void*)&Hook3210A}, {b,"xxxxxx????xxxxx",sizeof(b),5,"3210-B",(void*)&Hook3210B}, {c,"xxxxxx????xxxxx",sizeof(c),5,"3212",(void*)&Hook3212}, {d,"xxxxxx????xxx",sizeof(d),5,"3211",(void*)&Hook3211} };
    std::vector<Hook> hooks; hooks.reserve(64);
    BYTE* originalTargets[4]{};
    for(unsigned i=0;i<4;i++){ auto& p=patterns[i]; BYTE* at=FindUnique(text,textSize,p); if(!at){Log("%s signature missing or ambiguous; no changes made",p.name);return 0;} int32_t rel=0; memcpy(&rel,at+1,4); originalTargets[i]=at+5+rel; if(originalTargets[i]<text || originalTargets[i]>=text+textSize){Log("%s original target is outside PECore .text; no changes made",p.name);return 0;} Hook h{}; if(!MakeCall(h,at,p.hook,p.name)){Log("%s validation failed; no changes made",p.name);return 0;} hooks.push_back(h); }
    if(originalTargets[0]!=originalTargets[1] || originalTargets[0]!=originalTargets[2]){Log("RadioEx getter targets disagree; no changes made");return 0;}
    Log("Validated PECore targets: getters=%p setter=%p",originalTargets[0],originalTargets[3]);
    Hook pool{}; if(PreparePoolGuard(pool)) hooks.push_back(pool); else Log("Pool guard not installed; RadioEx fixes can still operate");
    if(WaitForModPatches()) {
        PrepareAuditedGuards(hooks);
        if(!PrepareActorGuard(hooks)){actorChecks.clear();Log("Special actor guard SKIPPED: unsupported loader layout");}
        PrepareIncidentGuards(hooks);
    }
    if(!CommitHooks(hooks)){Log("Hook transaction not installed; any completed writes were rolled back");return 0;}
    Log("Installed %u verified hooks with peer threads paused",static_cast<unsigned>(hooks.size()));
    for(auto& h:hooks) Log("  %s at %p",h.name,h.address);
    if(patchProtectionFailure) Log("ERROR: a page protection restore failed; code remains installed");
    MonitorGuards();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){ selfModule=module; HMODULE pinned=nullptr; if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(module),&pinned)) return FALSE; InitializeCriticalSection(&logLock); DisableThreadLibraryCalls(module); HANDLE t=CreateThread(nullptr,0,Bootstrap,nullptr,0,nullptr); if(t) CloseHandle(t); }
    else if(reason==DLL_PROCESS_DETACH){ InterlockedExchange(&shuttingDown,1); /* Windows may already be tearing modules down; installed call sites are process-local. */ }
    return TRUE;
}
