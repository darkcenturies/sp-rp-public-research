#include "AuditedGuards.inc"

static bool MatchBytes(const BYTE* at,const char* expected,const char* mask,size_t length){
    if(!Readable(at,length)) return false;
    for(size_t i=0;i<length;++i) if((!mask || mask[i]=='x') && at[i]!=BYTE(expected[i])) return false;
    return true;
}
static bool RevalidateAuditedGuards(){
    for(const auto& s:guardSpecs) if(g_resume[s.id]){
        if(!MatchBytes(reinterpret_cast<BYTE*>(s.address),s.expected,s.mask,s.expectedSize))return false;
        for(size_t i=0;i<s.exitCount;++i){const auto& e=s.exits[i];if(!MatchBytes(reinterpret_cast<BYTE*>(e.address),e.bytes,nullptr,e.size))return false;}
    }
    return RevalidateActorGuard() && RevalidateIncidentGuards();
}
static bool WaitForModPatches(){
    // These installed ASIs load after Crashfix in this game. Do not race their
    // initial patches. If absent on disk, they are not mandatory dependencies.
    char root[MAX_PATH]{};GetModuleFileNameA(GetModuleHandleA(nullptr),root,MAX_PATH);
    char* slash=strrchr(root,'\\');if(!slash)return false;slash[1]=0;
    const char* lateMods[]={"modloader.asi","skygfx.asi"};
    bool required[2]{};
    for(int i=0;i<2;++i){char path[MAX_PATH]{};strcpy_s(path,root);strcat_s(path,lateMods[i]);required[i]=GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;}
    DWORD previous=0;int stable=0;
    for(int attempt=0;attempt<150;++attempt){
        bool ready=true;for(int i=0;i<2;++i)if(required[i]&&!GetModuleHandleA(lateMods[i]))ready=false;
        DWORD fingerprint=2166136261u;
        for(const auto& s:guardSpecs){
            auto at=reinterpret_cast<BYTE*>(s.address);
            if(!Readable(at,s.expectedSize)){ready=false;break;}
            for(size_t i=0;i<s.expectedSize;++i)fingerprint=(fingerprint^at[i])*16777619u;
            for(size_t i=0;i<s.exitCount;++i){const auto& e=s.exits[i];auto exitAt=reinterpret_cast<BYTE*>(e.address);if(!Readable(exitAt,e.size)){ready=false;break;}for(size_t j=0;j<e.size;++j)fingerprint=(fingerprint^exitAt[j])*16777619u;}
        }
        stable=ready&&fingerprint==previous?stable+1:0;previous=fingerprint;
        if(stable>=10)return true;
        Sleep(100);
    }
    Log("Audited guard installation deferred: mod patches did not settle in 15 seconds");return false;
}
static void PrepareAuditedGuards(std::vector<Hook>& hooks){
    if(reinterpret_cast<DWORD>(GetModuleHandleA(nullptr))!=0x400000){Log("Audited GTA guards skipped: executable base differs");return;}
    for(const auto& s:guardSpecs){
        BYTE* at=reinterpret_cast<BYTE*>(s.address);
        bool match=MatchBytes(at,s.expected,s.mask,s.expectedSize);
        for(size_t i=0;match && i<s.exitCount;++i) {
            const auto& e=s.exits[i];match=MatchBytes(reinterpret_cast<BYTE*>(e.address),e.bytes,nullptr,e.size);
        }
        if(!match){Log("Guard %02d SKIPPED (modified/unsupported instructions): %s at %08lX",s.id,s.name,s.address);continue;}
        if(s.id==5){
            memcpy(&g_modelTable,at+3,4);
            if(!Readable(reinterpret_cast<void*>(g_modelTable),4)){Log("Guard 05 SKIPPED: live model table unreadable");continue;}
            Log("Guard 05 captured live model table %08lX",g_modelTable);
        }
        // Guard 16 executes its original CALL itself, before checking the result.
        // Other guards replay their branch-free original instructions verbatim.
        BYTE* replay=static_cast<BYTE*>(VirtualAlloc(nullptr,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!replay){Log("Guard %02d SKIPPED: trampoline allocation failed",s.id);continue;}
        size_t replaySize=s.size;
        if(s.id==16){DWORD displacement;memcpy(&displacement,at+1,4);g_call16=reinterpret_cast<void*>(DWORD(at+5)+displacement);replaySize=0;}
        memcpy(replay,at,replaySize);replay[replaySize]=0xE9;
        DWORD rel=DWORD(at+s.size)-DWORD(replay+replaySize+5);memcpy(replay+replaySize+1,&rel,4);
        DWORD old=0;
        if(!VirtualProtect(replay,32,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),replay,32)){
            VirtualFree(replay,0,MEM_RELEASE);Log("Guard %02d SKIPPED: trampoline protection/cache failure",s.id);continue;
        }
        g_resume[s.id]=replay;
        Hook h{};h.name=s.name;h.address=at;h.length=s.size;
        memcpy(h.original,at,s.size);memset(h.patched,0x90,s.size);h.patched[0]=0xE9;
        rel=DWORD(s.handler)-DWORD(at+5);memcpy(h.patched+1,&rel,4);hooks.push_back(h);
        Log("Guard %02d PREPARED: %s",s.id,s.name);
    }
    Log("Guard 13 RETIRED: fabricated animation fallback has no safe caller contract");
    Log("Guard 15 RETIRED: timed-out streaming IO cannot be treated as completed");
    Log("Guard 30 replaces frame guessing with null-clump protection; valid missing-frame results remain NULL");
}

static void MonitorGuards(){
    LONG previous[31]{};
    bool changed[64]{};
    for(;;){
        Sleep(2000);
        MonitorIncidents();
        for(const auto& s:guardSpecs){
            LONG now=InterlockedCompareExchange(&g_blocked[s.id],0,0);
            if(now!=previous[s.id]){Log("Guard %02d blocked total=%lu: %s",s.id,DWORD(now),s.name);previous[s.id]=now;}
        }
        for(size_t i=0;i<installed.size();++i){
            const auto& h=installed[i];bool differs=!Readable(h.address,h.length) || memcmp(h.address,h.patched,h.length)!=0;
            if(differs && !changed[i]) Log("Hook changed by another writer: %s at %p; leaving its bytes untouched",h.name,h.address);
            changed[i]=differs;
        }
    }
}
