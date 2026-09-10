// Guards derived from the actual PE executable and Discord report fault sites.
// No exception swallowing: valid instructions are replayed verbatim. Invalid
// inputs take narrowly defined no-op/empty-result paths before the bad access.
struct IncidentRegisters {
    DWORD edi,esi,ebp,savedEsp,ebx,edx,ecx,eax,flags;
};
static_assert(sizeof(IncidentRegisters)==36,"PUSHFD/PUSHAD layout");
static volatile LONG incidentBlocked[7]{};
static DWORD incidentContinue[7]={0,0x47EA4B,0x6F5E63,0x4C9697,0x7F0B06,0x6C45B6,0x6C7C56};

static bool IncidentField(DWORD object,DWORD offset,size_t bytes,bool write){
    if(object<0x10000 || object>MAXDWORD-offset)return false;
    auto p=reinterpret_cast<const void*>(object+offset);
    return write ? Writable(p,bytes) : Readable(p,bytes);
}
extern "C" __declspec(noinline) int __cdecl IncidentProbe(unsigned site,const IncidentRegisters* r){
    bool valid=false;
    switch(site){
    case 1: // GET_CHAR_HEALTH: absent actor has no health; script gets zero.
        valid=IncidentField(r->eax,0x540,4,false);break;
    case 2: // CTrain::SetTrainCruiseSpeed: absent train cannot receive a write.
        valid=IncidentField(r->ecx,0x3D0,1,true);break;
    case 3: // CVehicleModelInfo::CreateInstance: extras are optional (six slots).
        valid=IncidentField(r->eax,0x30C,1,false) &&
              *reinterpret_cast<const BYTE*>(r->eax+0x30C)<=6;break;
    case 4: { // RwFrameAddChild: validate before detaching child from old parent.
        // savedEsp is the stack position after PUSHFD, before PUSHAD.
        DWORD parent=0,child=0;
        if(r->savedEsp<=MAXDWORD-16 && Readable(reinterpret_cast<void*>(r->savedEsp+8),8)){
            memcpy(&parent,reinterpret_cast<void*>(r->savedEsp+8),4);
            memcpy(&child,reinterpret_cast<void*>(r->savedEsp+12),4);
            valid=parent!=child && IncidentField(parent,0,0xA4,true) && IncidentField(child,0,0xA4,true);
        }
        break;
    }
    default:return 0;
    }
    if(!valid)InterlockedIncrement(&incidentBlocked[site]);
    return valid?1:0;
}

// FXSAVE/FXRSTOR isolates C++/VirtualQuery from game x87 and SSE state. All
// general registers and EFLAGS are restored before replay or fallback.
__declspec(naked) static void IncidentHealth(){
    __asm {
        pushfd
        pushad
        mov ebp,esp
        sub esp,528
        and esp,0FFFFFFF0h
        fxsave [esp]
        push ebp
        push 1
        call IncidentProbe
        add esp,8
        fxrstor [esp]
        mov esp,ebp
        test eax,eax
        jz missing
        popad
        popfd
        fld dword ptr [eax+540h]
        jmp dword ptr [incidentContinue+4]
    missing:
        popad
        popfd
        fldz
        jmp dword ptr [incidentContinue+4]
    }
}
__declspec(naked) static void IncidentTrain(){
    __asm {
        pushfd
        pushad
        mov ebp,esp
        sub esp,528
        and esp,0FFFFFFF0h
        fxsave [esp]
        push ebp
        push 2
        call IncidentProbe
        add esp,8
        fxrstor [esp]
        mov esp,ebp
        test eax,eax
        jz missing
        popad
        popfd
        mov byte ptr [ecx+3D0h],al
        jmp dword ptr [incidentContinue+8]
    missing:
        popad
        popfd
        jmp dword ptr [incidentContinue+8]
    }
}
__declspec(naked) static void IncidentExtras(){
    __asm {
        pushfd
        pushad
        mov ebp,esp
        sub esp,528
        and esp,0FFFFFFF0h
        fxsave [esp]
        push ebp
        push 3
        call IncidentProbe
        add esp,8
        fxrstor [esp]
        mov esp,ebp
        test eax,eax
        jz missing
        popad
        popfd
        mov cl,byte ptr [eax+30Ch]
        jmp dword ptr [incidentContinue+12]
    missing:
        popad
        popfd
        mov cl,0
        jmp dword ptr [incidentContinue+12]
    }
}
__declspec(naked) static void IncidentFrame(){
    __asm {
        pushfd
        pushad
        mov ebp,esp
        sub esp,528
        and esp,0FFFFFFF0h
        fxsave [esp]
        push ebp
        push 4
        call IncidentProbe
        add esp,8
        fxrstor [esp]
        mov esp,ebp
        test eax,eax
        jz missing
        popad
        popfd
        mov ecx,dword ptr [esp+8]
        push esi
        push edi
        jmp dword ptr [incidentContinue+16]
    missing:
        popad
        popfd
        mov eax,0
        ret
    }
}

// Conservative visual gate: this exact binary initializes two searchlight
// records (0x4C stride, cursor C1C998..C1CA30). Some reference headers declare
// four; we do not rely on that larger capacity. Limit both enqueue and render
// to the two initialized records. Excess lights are omitted, not allocated.
__declspec(naked) static void IncidentLightAdd(){
    __asm {
        pushfd
        cmp dword ptr ds:[0C1C96Ch],2
        jae full
        popfd
        mov edx,dword ptr ds:[0C1C96Ch]
        jmp dword ptr [incidentContinue+20]
    full:
        lock inc dword ptr [incidentBlocked+20]
        popfd
        ret
    }
}
__declspec(naked) static void IncidentLightRender(){
    __asm {
        pushfd
        cmp dword ptr ds:[0C1C96Ch],2
        jbe valid
        mov dword ptr ds:[0C1C96Ch],2
        lock inc dword ptr [incidentBlocked+24]
    valid:
        popfd
        mov eax,dword ptr ds:[0C1C96Ch]
        push ebx
        jmp dword ptr [incidentContinue+24]
    }
}

struct IncidentSignature {DWORD address;const char* bytes;size_t size;};
static const IncidentSignature healthSignatures[]={
    {0x47EA45,"\xD9\x80\x40\x05\0\0",6},
    {0x47EA4B,"\xE8\xF0\x30\x3A\0\x6A\x01\x8B\xCE\xA3\x78\x3C\xA4\0\xE8\x12\x59\xFE\xFF",19},
};
static const IncidentSignature trainSignatures[]={
    {0x6F5E50,"\xD9\x44\x24\x08\xE8\xE7\xBC\x12\0\x8B\x4C\x24\x04\x88\x81\xD0\x03\0\0\xC3",20},
};
static const IncidentSignature extrasSignatures[]={
    {0x4C9691,"\x8A\x88\x0C\x03\0\0\x84\xC9\x89\x7C\x24\x0C\x0F\x84\x0E\x01\0\0",18},
    {0x4C97B1,"\xC6\x05\x78\xE4\xB4\0\xFF\xC6\x05\x79\xE4\xB4\0\xFF\x6A\0\x68\x10\x94\x4C\0\x57",22},
};
static const IncidentSignature frameSignatures[]={
    {0x7F0B00,"\x8B\x4C\x24\x08\x56\x57\x8B\x41\x04",9},
};
static const IncidentSignature lightSignatures[]={
    {0x6C45B0,"\x8B\x15\x6C\xC9\xC1\0\x8B\x4C\x24\x04",10},
    {0x6C45C4,"\x6B\xC0\x4C\x89\x88\x90\xC9\xC1\0",9},
    {0x6C462A,"\x89\x15\x6C\xC9\xC1\0\xC3",7},
    {0x6C7C50,"\xA1\x6C\xC9\xC1\0\x53\x32\xDB\x85\xC0\x56\x0F\x86\xA2\0\0\0",17},
    {0x6C458A,"\x83\xC0\x4C\x3D\x30\xCA\xC1\0\x7C\xDF",10},
};
struct IncidentSpec {unsigned id;const char* name;DWORD address;void* target;const IncidentSignature* signatures;size_t count;};
static const IncidentSpec incidentSpecs[]={
    {1,"VCF-045 missing actor health (0226)",0x47EA45,IncidentHealth,healthSignatures,2},
    {2,"VCF-005/010 missing train cruise-speed target",0x6F5E5D,IncidentTrain,trainSignatures,1},
    {3,"VCF-023/029 missing vehicle extra structure",0x4C9691,IncidentExtras,extrasSignatures,2},
    {4,"VCF-002 invalid frame attach inputs",0x7F0B00,IncidentFrame,frameSignatures,1},
    {5,"VCF-008 two-searchlight enqueue gate",0x6C45B0,IncidentLightAdd,lightSignatures,5},
    {6,"VCF-008 two-searchlight render bound",0x6C7C50,IncidentLightRender,lightSignatures,5},
};
static bool incidentPrepared[7]{};
static bool IncidentMatches(const IncidentSpec& spec){
    for(size_t i=0;i<spec.count;++i){const auto& s=spec.signatures[i];
        if(!Readable(reinterpret_cast<void*>(s.address),s.size) || memcmp(reinterpret_cast<void*>(s.address),s.bytes,s.size))return false;
    }
    return true;
}
static bool RevalidateIncidentGuards(){
    for(const auto& s:incidentSpecs)if(incidentPrepared[s.id]&&!IncidentMatches(s))return false;
    return true;
}
static void PrepareIncidentGuards(std::vector<Hook>& hooks){
    if(DWORD(GetModuleHandleA(nullptr))!=0x400000){Log("Incident guards skipped: unsupported executable base");return;}
    for(const auto& s:incidentSpecs){
        if(!IncidentMatches(s)){Log("Incident guard SKIPPED: %s (modified/unsupported instructions)",s.name);continue;}
        Hook h{};h.name=s.name;h.address=reinterpret_cast<BYTE*>(s.address);h.length=6;
        memcpy(h.original,h.address,6);memset(h.patched,0x90,6);h.patched[0]=0xE9;
        int32_t rel=DWORD(s.target)-(s.address+5);memcpy(h.patched+1,&rel,4);
        hooks.push_back(h);incidentPrepared[s.id]=true;Log("Incident guard PREPARED: %s at %08lX",s.name,s.address);
    }
}
static void MonitorIncidents(){
    static LONG previous[7]{};
    for(const auto& s:incidentSpecs){LONG now=InterlockedCompareExchange(&incidentBlocked[s.id],0,0);
        if(now!=previous[s.id]){Log("Incident guard blocked total=%lu: %s",DWORD(now),s.name);previous[s.id]=now;}
    }
}
