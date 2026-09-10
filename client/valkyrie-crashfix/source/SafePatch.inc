// Freeze existing peer threads during code writes. No logging, allocation or CRT
// locks while peers are stopped. A peer inside any overwritten bytes aborts.
struct FrozenThreads {
    HANDLE handles[256]{};
    size_t count=0, stopped=0;
    bool Freeze(const std::vector<Hook>& hooks) {
        HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
        if(snap==INVALID_HANDLE_VALUE) return false;
        THREADENTRY32 entry{}; entry.dwSize=sizeof(entry);
        bool ok=Thread32First(snap,&entry)!=FALSE;
        if(ok) do {
            if(entry.th32OwnerProcessID!=GetCurrentProcessId() || entry.th32ThreadID==GetCurrentThreadId()) continue;
            if(count==256){ok=false;break;}
            HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
            if(!h){ok=false;break;}
            handles[count++]=h;
        } while(Thread32Next(snap,&entry));
        CloseHandle(snap);
        if(!ok) return false;
        for(size_t i=0;i<count;++i){
            if(SuspendThread(handles[i])==DWORD(-1)) return false;
            ++stopped;
            CONTEXT ctx{};ctx.ContextFlags=CONTEXT_CONTROL;
            if(!GetThreadContext(handles[i],&ctx)) return false;
            for(const auto& h:hooks) {
                DWORD begin=reinterpret_cast<DWORD>(h.address);
                if(ctx.Eip>=begin && ctx.Eip<begin+h.length) return false;
            }
        }
        return true;
    }
    ~FrozenThreads(){
        for(size_t i=0;i<stopped;++i) ResumeThread(handles[i]);
        for(size_t i=0;i<count;++i) CloseHandle(handles[i]);
    }
};

static bool RevalidateAuditedGuards();

#ifdef VALKYRIE_TEST
static int testFailAfterWrite=-1;
#endif
static bool CommitHooks(std::vector<Hook>& hooks) {
    if(hooks.empty() || hooks.size()>64) return false;
    // Allocate tracking state before stopping any peer thread.
    installed=hooks;
    bool success=false;
    {
        FrozenThreads frozen;
        if(!frozen.Freeze(hooks)){ installed.clear();return false; }
        if(!RevalidateAuditedGuards()){installed.clear();return false;}
        // Recheck originals while peers are stopped: never overwrite another mod.
        for(const auto& h:hooks)
            if(memcmp(h.address,h.original,h.length)){installed.clear();return false;}
        DWORD protection[64]{};
        size_t madeWritable=0;
        for(;madeWritable<hooks.size();++madeWritable){
            auto& h=hooks[madeWritable];
            if(!VirtualProtect(h.address,h.length,PAGE_EXECUTE_READWRITE,&protection[madeWritable])) break;
        }
        size_t written=0;
        if(madeWritable==hooks.size()) {
            for(size_t i=0;i<hooks.size();++i){
                auto& h=hooks[i];
                memcpy(h.address,h.patched,h.length);
                written=i+1; // Includes the current write if verification fails.
                if(!FlushInstructionCache(GetCurrentProcess(),h.address,h.length) || memcmp(h.address,h.patched,h.length)) break;
#ifdef VALKYRIE_TEST
                if(testFailAfterWrite==int(i)) break;
#endif
                if(i+1==hooks.size()) success=true;
            }
        }
        if(!success) for(size_t i=0;i<written;++i){
            auto& h=hooks[i];memcpy(h.address,h.original,h.length);
            FlushInstructionCache(GetCurrentProcess(),h.address,h.length);
        }
        // Reverse order is required when multiple hooks share one memory page.
        for(size_t i=madeWritable;i>0;--i){
            DWORD ignored=0;auto& h=hooks[i-1];
            if(!VirtualProtect(h.address,h.length,protection[i-1],&ignored)) {
                // Keep the code and trampolines alive; do not claim rollback.
                patchProtectionFailure=true;
            }
        }
    }
    if(!success) installed.clear();
    return success;
}
