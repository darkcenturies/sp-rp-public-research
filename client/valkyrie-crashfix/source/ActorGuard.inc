// RequestSpecialModel preflight and a paired, scoped FLA directory lookup.
// Never enter the destructive loader until its file metadata has been resolved.
#include "ActorSignatures.inc"
using ActorRequestFn = void (__cdecl*)(int,const char*,int);
using ActorFindFn = void* (__thiscall*)(void*,const char*);
using ActorKeyFn = DWORD (__cdecl*)(const char*);
static ActorRequestFn actorOriginal;
static ActorFindFn actorFind;
static ActorKeyFn actorKey;
static BYTE** actorModels;
static DWORD actorModelCount;
static BYTE** actorStreaming;
static BYTE** actorExtended;
static void*** actorDirectory;
struct ActorContext { BYTE entry[64]{}; const char* name=nullptr; void* directory=nullptr; };
static __declspec(thread) ActorContext* actorContext;
struct ActorCheck { const BYTE* at; BYTE bytes[128]; size_t size; };
static std::vector<ActorCheck> actorChecks;

static DWORD ActorRead32(const void* p){DWORD value;memcpy(&value,p,4);return value;}
static void ActorWrite32(void* p,DWORD value){memcpy(p,&value,4);}
static bool ActorName(const char* name){
    for(size_t i=0;i<48;++i){if(!Readable(name+i,1))return false;if(!name[i])return i!=0;}
    return false;
}
static bool ActorModel(int id,BYTE*& model){
    if(id<0 || DWORD(id)>=actorModelCount || !Readable(actorModels+id,4))return false;
    model=actorModels[id];return Readable(model,0x20);
}
// Inverse of the verified FLA +AA0F0 metadata copier. Do not copy list links,
// load state, references or flags between models. Refuse unrepresentable fields.
static bool ActorMetadata(int id,BYTE* entry){
    if(id<0 || DWORD(id)>=actorModelCount || !Readable(actorStreaming,4) || !Readable(actorExtended,4))return false;
    BYTE* base=*actorStreaming;BYTE* ext=*actorExtended;
    if(!base || !ext || DWORD(base)>MAXDWORD-DWORD(id)*20-20 || DWORD(ext)>MAXDWORD-DWORD(id)*72-72)return false;
    BYTE* s=base+size_t(id)*20;BYTE* e=ext+size_t(id)*72;
    if(!Readable(s,20) || !Readable(e,72))return false;
    DWORD offset=ActorRead32(s+8),size=ActorRead32(s+12);
    DWORD a=ActorRead32(e+16),b=ActorRead32(e+20),c=ActorRead32(e+24);
    if(offset>0xffffff || !b || size>0xffff || a>0xffff || b>0xffff || c>0xffff)return false;
    ActorWrite32(entry,offset|(DWORD(s[7])<<24));
    WORD words[]={WORD(size),WORD(a),WORD(b),WORD(c)};memcpy(entry+4,words,8);
    ActorWrite32(entry+12,ActorRead32(e+28));return true;
}
static bool ActorResolve(int id,const char* name,ActorContext& context,int& source){
    BYTE* destination=nullptr;
    if(!ActorName(name) || !ActorModel(id,destination) || !Readable(actorDirectory,4))return false;
    void** indirect=*actorDirectory;
    if(!Readable(indirect,4) || !Readable(*indirect,16))return false;
    context.name=name;context.directory=*indirect;
    DWORD key=actorKey(name);
    // Preserve the game's existing same-name, already-addressable fast path.
    if(ActorRead32(destination+4)==key && ActorMetadata(id,context.entry)){source=id;return true;}
    auto directory=static_cast<BYTE*>(context.directory);
    DWORD count=ActorRead32(directory+8),capacity=ActorRead32(directory+4);
    auto entries=reinterpret_cast<BYTE*>(ActorRead32(directory));
    if(count>capacity || count>1000000 || (count && !Readable(entries,size_t(count)*64)))return false;
    if(void* entry=actorFind(context.directory,name)){
        if(!Readable(entry,64))return false;
        memcpy(context.entry,entry,64);source=-1;return true;
    }
    // Eagle can register a trainer's special actor as an ordinary ped. Such
    // models are absent from the extra-objects directory but have real streaming
    // metadata. Only use the same model class; never substitute a random actor.
    if(!Readable(actorModels,size_t(actorModelCount)*4))return false;
    for(DWORD i=0;i<actorModelCount;++i){
        BYTE* model=actorModels[i];
        if(i==DWORD(id) || !Readable(model,0x20) || ActorRead32(model+4)!=key || ActorRead32(model)!=ActorRead32(destination))continue;
        if(ActorMetadata(int(i),context.entry)){source=int(i);return true;}
    }
    return false;
}
static void* __fastcall ActorLookup(void* directory,void*,const char* name){
    // The original loader uses this exact name pointer. A nested request has its
    // own context; unrelated calls retain their original lookup behavior.
    if(actorContext && actorContext->name==name && actorContext->directory==directory)return actorContext->entry;
    return actorFind(directory,name);
}
static void __cdecl ActorRequest(int id,const char* name,int flags){
    ActorContext context;int source=-1;
    if(!ActorResolve(id,name,context,source)){
        Log("Special actor request rejected before model changes: slot=%d name=%s",id,ActorName(name)?name:"<invalid>");return;
    }
    if(source>=0 && source!=id)Log("Special actor resolved from registered model: name=%s source=%d slot=%d",name,source,id);
    ActorContext* previous=actorContext;actorContext=&context;
    actorOriginal(id,name,flags);
    actorContext=previous;
}
static bool ActorSignature(const BYTE* at,const char* bytes,const char* mask,size_t size){
    if(!Readable(at,size))return false;
    for(size_t i=0;i<size;++i)if((!mask || mask[i]=='x') && at[i]!=BYTE(bytes[i]))return false;
    ActorCheck check{};check.at=at;check.size=size;memcpy(check.bytes,at,size);actorChecks.push_back(check);return true;
}
static bool RevalidateActorGuard(){
    for(const auto& check:actorChecks)if(!Readable(check.at,check.size) || memcmp(check.at,check.bytes,check.size))return false;
    return true;
}
static bool PrepareActorGuard(std::vector<Hook>& hooks){
    auto fla=reinterpret_cast<BYTE*>(GetModuleHandleA("$fastman92limitAdjuster.asi"));
    if(!fla || DWORD(GetModuleHandleA(nullptr))!=0x400000)return false;
    actorChecks.clear();
    if(!ActorSignature(fla+0xaa0f0,actor_copyBytes,actor_copyMask,sizeof(actor_copyBytes)-1)
       || !ActorSignature(fla+0xaa5b0,actor_lookupBytes,actor_lookupMask,sizeof(actor_lookupBytes)-1)
       || !ActorSignature(fla+0xaa590,actor_fastBytes,actor_fastMask,sizeof(actor_fastBytes)-1)
       || !ActorSignature(fla+0xa8650,actor_findBytes,actor_findMask,sizeof(actor_findBytes)-1)
       || !ActorSignature(reinterpret_cast<BYTE*>(0x409d10),"\x83\xec\x08\x53\x8b\x5c\x24\x10\x55\x8b\x2c\x9d\0\0\0\0\x56\x8b\x75\x04\x57\x8b\x7c\x24\x20\x57\xe8\x01\x32\x13\0","xxxxxxxxxxxx????xxxxxxxxxxxxxxx",31)
       || !ActorSignature(reinterpret_cast<BYTE*>(0x4c5956),"\x81\xf9\0\0\0\0","xx????",6))return false;
    auto jumpMatches=[&](DWORD at,BYTE* target){
        auto code=reinterpret_cast<BYTE*>(at);
        if(!Readable(code,5) || code[0]!=0xe9 || code+5+int32_t(ActorRead32(code+1))!=target)return false;
        return ActorSignature(code,reinterpret_cast<const char*>(code),nullptr,5);
    };
    if(!jumpMatches(0x409f65,fla+0xaa5b0) || !jumpMatches(0x409d39,fla+0xaa590)
       || !jumpMatches(0x409f9f,reinterpret_cast<BYTE*>(0x409fd3)))return false;
    // Validate every wildcarded relocation against this module's actual base.
    if(ActorRead32(fla+0xaa0f4)!=DWORD(fla+0x1aae74)
       || ActorRead32(fla+0xaa103)!=DWORD(fla+0x1aae80)
       || ActorRead32(fla+0xaa597)!=DWORD(fla+0x1aae80)
       || ActorRead32(fla+0xaa5b3)!=DWORD(fla+0x1b68f0))return false;
    actorModels=reinterpret_cast<BYTE**>(ActorRead32(reinterpret_cast<void*>(0x409d1c)));
    actorModelCount=ActorRead32(reinterpret_cast<void*>(0x4c5958));
    if(!actorModelCount || actorModelCount>1000000 || !Readable(actorModels,size_t(actorModelCount)*4))return false;
    actorStreaming=reinterpret_cast<BYTE**>(fla+0x1aae74);
    actorExtended=reinterpret_cast<BYTE**>(fla+0x1aae80);
    actorDirectory=reinterpret_cast<void***>(fla+0x1b68f0);
    actorFind=reinterpret_cast<ActorFindFn>(fla+0xa8650);
    actorKey=reinterpret_cast<ActorKeyFn>(0x53cf30);
    BYTE* replay=static_cast<BYTE*>(VirtualAlloc(nullptr,16,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!replay)return false;
    memcpy(replay,reinterpret_cast<void*>(0x409d10),8);replay[8]=0xe9;
    ActorWrite32(replay+9,0x409d18-DWORD(replay+13));DWORD old;
    if(!VirtualProtect(replay,16,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),replay,16)){VirtualFree(replay,0,MEM_RELEASE);return false;}
    actorOriginal=reinterpret_cast<ActorRequestFn>(replay);
    Hook entry{};entry.name="Special actor preflight at 00409D10";entry.address=reinterpret_cast<BYTE*>(0x409d10);entry.length=8;
    memcpy(entry.original,entry.address,8);memset(entry.patched,0x90,8);entry.patched[0]=0xe9;ActorWrite32(entry.patched+1,DWORD(&ActorRequest)-0x409d15);
    Hook lookup{};
    if(!MakeCall(lookup,fla+0xaa5b9,reinterpret_cast<void*>(&ActorLookup),"FLA special actor resolved metadata"))return false;
    hooks.push_back(entry);hooks.push_back(lookup);
    Log("Special actor guard prepared: live model table=%p count=%lu",actorModels,actorModelCount);return true;
}
