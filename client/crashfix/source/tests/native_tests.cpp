#define VALKYRIE_TEST
#include "../ValkyrieCrashfix.cpp"
#include <cassert>
#include "actor_tests.inc"
#include "incident_tests.inc"

static DWORD WINAPI Idle(LPVOID){return 0;}
static DWORD fakeHandle=17;
static QWORD fakeLength=1000,fakePosition=250,lastSeek=0;
static bool conversionFails=false,seekFails=false;
static DWORD __stdcall FakeStream(int,int){return fakeHandle;}
static QWORD __stdcall FakeLength(DWORD,DWORD){return fakeLength;}
static QWORD __stdcall FakePosition(DWORD,DWORD){return fakePosition;}
static double __stdcall FakeSeconds(DWORD,QWORD b){return double(b)/100.;}
static QWORD __stdcall FakeBytes(DWORD,double s){return conversionFails?~QWORD(0):QWORD(s*100.);}
static BOOL __stdcall FakeSeek(DWORD,QWORD pos,DWORD){lastSeek=pos;return !seekFails;}
int main(){
    selfModule=GetModuleHandleA(nullptr);InitializeCriticalSection(&logLock);
    BYTE* memory=static_cast<BYTE*>(VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    assert(memory);memset(memory,0x90,8192);
    std::vector<Hook> hooks;
    for(size_t offset:{size_t(0),size_t(16),size_t(4096)}){
        Hook h{};h.name="test";h.address=memory+offset;h.length=5;
        memset(h.original,0x90,5);memset(h.patched,0xCC,5);hooks.push_back(h);
    }
    DWORD ignored;assert(VirtualProtect(memory,8192,PAGE_EXECUTE_READ,&ignored));
    auto verifyOriginal=[&](){
        for(auto& h:hooks) assert(memcmp(h.address,h.original,5)==0);
        for(size_t offset:{size_t(0),size_t(4096)}){MEMORY_BASIC_INFORMATION m{};assert(VirtualQuery(memory+offset,&m,sizeof(m)));assert(m.Protect==PAGE_EXECUTE_READ);}
    };
    for(int i=0;i<3;++i){testFailAfterWrite=i;assert(!CommitHooks(hooks));assert(installed.empty());verifyOriginal();}
    puts("PASS: rollback includes failed current write at all 3 positions; shared-page protections restored.");
    testFailAfterWrite=-1;
    assert(VirtualProtect(memory,5,PAGE_EXECUTE_READWRITE,&ignored));memory[0]=0xC3;
    assert(VirtualProtect(memory,5,PAGE_EXECUTE_READ,&ignored));
    assert(!CommitHooks(hooks));assert(memory[0]==0xC3);
    assert(VirtualProtect(memory,5,PAGE_EXECUTE_READWRITE,&ignored));memory[0]=0x90;
    assert(VirtualProtect(memory,5,PAGE_EXECUTE_READ,&ignored));
    puts("PASS: competing patch is rejected without overwriting its bytes.");
    HANDLE thread=CreateThread(nullptr,0,Idle,nullptr,CREATE_SUSPENDED,nullptr);assert(thread);
    CONTEXT original{};original.ContextFlags=CONTEXT_CONTROL;assert(GetThreadContext(thread,&original));
    CONTEXT inside=original;inside.Eip=DWORD(memory);assert(SetThreadContext(thread,&inside));
    assert(!CommitHooks(hooks));verifyOriginal();
    assert(SetThreadContext(thread,&original));assert(ResumeThread(thread)==1);
    assert(WaitForSingleObject(thread,2000)==WAIT_OBJECT_0);CloseHandle(thread);
    puts("PASS: peer EIP in overwrite range aborts; pre-existing suspend count preserved.");
    assert(CommitHooks(hooks));assert(installed.size()==3);
    for(auto& h:hooks)assert(memcmp(h.address,h.patched,5)==0);
    assert(!patchProtectionFailure);
    puts("PASS: successful transaction installs every patch and restores protection.");
    // Pattern guard must reject a changed continuation as well as entry bytes.
    BYTE signature[]={0x90,0x90,0x90};assert(MatchBytes(signature,"\x90\x90\x90",nullptr,3));
    signature[2]=0xCC;assert(!MatchBytes(signature,"\x90\x90\x90",nullptr,3));
    assert(!Readable(nullptr,4));assert(!Readable(reinterpret_cast<void*>(0x5c),4));
    VirtualFree(memory,0,MEM_RELEASE);
    getStream=FakeStream;getLength=FakeLength;getPosition=FakePosition;
    bytesToSeconds=FakeSeconds;secondsToBytes=FakeBytes;setPosition=FakeSeek;
    float out=99;
    assert(Hook3210A(0x3210,5,&out)==1 && out==2.5f);
    assert(Hook3212(0x3212,5,&out)==1 && out==10.f);
    fakePosition=1200;assert(Hook3210B(0x3210,5,&out)==1 && out==10.f);
    fakePosition=~QWORD(0);assert(!Hook3210A(0x3210,5,&out) && out==0.f);
    fakeHandle=0;assert(!Hook3212(0x3212,5,&out) && out==0.f);fakeHandle=17;
    fakeLength=~QWORD(0);assert(!Hook3212(0x3212,5,&out));
    fakeLength=0;assert(!Hook3212(0x3212,5,&out));fakeLength=1000;
    assert(!Hook3210A(0x3210,5,nullptr));
    assert(Hook3211(0x3211,5,-5.f) && lastSeek==0);
    assert(Hook3211(0x3211,5,999.f) && lastSeek==1000);
    assert(!Hook3211(0x3211,5,NAN));assert(!Hook3211(0x3211,5,INFINITY));
    conversionFails=true;assert(!Hook3211(0x3211,5,1.f));conversionFails=false;
    seekFails=true;assert(!Hook3211(0x3211,5,1.f));
    BYTE* pool=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));assert(pool);
    assert(PoolReadable(pool));assert(VirtualProtect(pool,4096,PAGE_READONLY,&ignored));assert(!PoolReadable(pool));
    VirtualFree(pool,0,MEM_RELEASE);
    puts("PASS: RadioEx outputs, missing streams, BASS errors, clamping, NaN/infinity and read-only pool rejection.");
    TestActors();
    TestIncidents();
    puts("All native installer tests passed.");return 0;
}
