static BYTE actorTestModel[5][32],actorTestStream[5][20],actorTestExt[5][72],actorTestDir[16],actorTestEntry[64];
static BYTE* actorTestTable[5];
static BYTE* actorTestStreamPtr=&actorTestStream[0][0];
static BYTE* actorTestExtPtr=&actorTestExt[0][0];
static void* actorTestDirPtr=actorTestDir;
static void** actorTestDirIndirect=&actorTestDirPtr;
static int actorCalls,actorFindCalls,actorNesting;
static bool actorDirectoryHit;
static const char* actorExpectedName;
static DWORD __cdecl FakeActorKey(const char* name){return !_stricmp(name,"psycho")?123:456;}
static void* __fastcall FakeActorFind(void*,void*,const char*){++actorFindCalls;return actorDirectoryHit?actorTestEntry:nullptr;}
static void __cdecl FakeActorOriginal(int id,const char* name,int flags){
    ++actorCalls;assert(id==0 && name==actorExpectedName && flags==12);
    // Simulate the destructive rename: resolution must already be complete.
    ActorWrite32(actorTestModel[0]+4,123);
    BYTE* entry=static_cast<BYTE*>(ActorLookup(actorTestDir,nullptr,name));assert(entry);
    assert(ActorRead32(entry)==(0x55000000|0x2345));
    assert(ActorRead32(entry+12)==0x12345678);
    WORD fields[4];memcpy(fields,entry+4,8);
    assert(fields[0]==7 && fields[1]==8 && fields[2]==9 && fields[3]==10);
    assert(actorTestStream[0][0]==0xA5 && actorTestExt[0][0]==0xA5);
    if(!actorNesting){
        ++actorNesting;ActorContext* outer=actorContext;ActorRequest(id,name,flags);assert(actorContext==outer);--actorNesting;
    }
}
static void TestActors(){
    actorModels=actorTestTable;actorModelCount=5;actorStreaming=&actorTestStreamPtr;actorExtended=&actorTestExtPtr;
    actorDirectory=&actorTestDirIndirect;actorKey=FakeActorKey;actorFind=reinterpret_cast<ActorFindFn>(&FakeActorFind);actorOriginal=FakeActorOriginal;
    for(int i=0;i<5;++i){actorTestTable[i]=actorTestModel[i];ActorWrite32(actorTestModel[i],0x87654321);}
    ActorWrite32(actorTestModel[4]+4,123);
    memset(actorTestStream[0],0xA5,20);memset(actorTestExt[0],0xA5,72);
    actorTestStream[4][7]=0x55;ActorWrite32(actorTestStream[4]+8,0x2345);ActorWrite32(actorTestStream[4]+12,7);
    ActorWrite32(actorTestExt[4]+16,8);ActorWrite32(actorTestExt[4]+20,9);ActorWrite32(actorTestExt[4]+24,10);ActorWrite32(actorTestExt[4]+28,0x12345678);
    actorExpectedName="psycho";actorDirectoryHit=false;
    ActorRequest(0,actorExpectedName,12);assert(actorCalls==2 && actorContext==nullptr);
    assert(ActorMetadata(4,actorTestEntry));
    FILE* fixture=nullptr;assert(!fopen_s(&fixture,"actor-metadata-fixture.bin","wb"));
    assert(fwrite(actorTestEntry,1,64,fixture)==64);assert(fwrite(actorTestStream[4],1,20,fixture)==20);assert(fwrite(actorTestExt[4],1,72,fixture)==72);fclose(fixture);
    actorDirectoryHit=true;ActorRequest(0,actorExpectedName,12);assert(actorCalls==4 && actorContext==nullptr);
    actorDirectoryHit=false;
    BYTE before[32];memcpy(before,actorTestModel[0],32);
    ActorRequest(-1,"psycho",12);ActorRequest(5,"psycho",12);ActorRequest(0,nullptr,12);ActorRequest(0,"",12);ActorRequest(0,"absent",12);
    char longName[48];memset(longName,'x',48);ActorRequest(0,longName,12);
    assert(actorCalls==4 && !memcmp(before,actorTestModel[0],32));
    ActorWrite32(actorTestModel[4],0x11111111);ActorRequest(0,"psycho",12);assert(actorCalls==4);ActorWrite32(actorTestModel[4],0x87654321);
    ActorWrite32(actorTestExt[4]+20,0);ActorRequest(0,"psycho",12);assert(actorCalls==4);
    ActorWrite32(actorTestExt[4]+20,0x10000);ActorRequest(0,"psycho",12);assert(actorCalls==4);ActorWrite32(actorTestExt[4]+20,9);
    // An already-addressable destination remains valid without a directory hit.
    memcpy(actorTestStream[0],actorTestStream[4],20);memcpy(actorTestExt[0],actorTestExt[4],72);
    ActorContext ctx;int source=-1;int callsBefore=actorFindCalls;
    assert(ActorResolve(0,"psycho",ctx,source) && source==0 && actorFindCalls==callsBefore);
    actorDirectoryHit=true;assert(ActorLookup(actorTestDir,nullptr,"unrelated")==actorTestEntry);
    actorContext=nullptr;actorChecks.clear();
    puts("PASS: special actor registered-model fallback, directory hit, existing-slot fast path, nested context, unrelated lookup, invalid IDs/names, missing files, class mismatch and metadata bounds; rejection precedes loader effects.");
}
