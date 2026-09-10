static void TestIncidents(){
    BYTE* p=static_cast<BYTE*>(VirtualAlloc(nullptr,0x3000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));assert(p);
    IncidentRegisters r{};r.eax=DWORD(p);r.ecx=DWORD(p);
    assert(IncidentProbe(1,&r));assert(IncidentProbe(2,&r));
    for(unsigned count=0;count<=6;++count){p[0x30C]=BYTE(count);assert(IncidentProbe(3,&r));}
    for(unsigned count:{7u,16u,255u}){p[0x30C]=BYTE(count);assert(!IncidentProbe(3,&r));}
    DWORD protect=0;assert(VirtualProtect(p,0x1000,PAGE_READONLY,&protect));
    assert(IncidentProbe(1,&r));assert(!IncidentProbe(2,&r));
    assert(VirtualProtect(p,0x1000,PAGE_NOACCESS,&protect));
    assert(!IncidentProbe(1,&r));assert(!IncidentProbe(2,&r));assert(!IncidentProbe(3,&r));
    assert(VirtualProtect(p,0x1000,PAGE_READWRITE,&protect));
    for(DWORD invalid:{0ul,0x5Cul,0xFFFFul,0xFFFFFFF0ul}){
        r.eax=invalid;r.ecx=invalid;
        for(unsigned site=1;site<=3;++site)assert(!IncidentProbe(site,&r));
    }
    DWORD args[]={0,DWORD(p),DWORD(p+0x1000)};r.savedEsp=DWORD(args)-4;
    assert(IncidentProbe(4,&r));
    args[1]=args[2];assert(!IncidentProbe(4,&r)); // self-parenting must not mutate lists
    args[1]=0;assert(!IncidentProbe(4,&r));args[1]=DWORD(p);args[2]=0;assert(!IncidentProbe(4,&r));
    args[2]=DWORD(p+0x1000);assert(VirtualProtect(p+0x1000,0x1000,PAGE_READONLY,&protect));
    assert(!IncidentProbe(4,&r));assert(VirtualProtect(p+0x1000,0x1000,PAGE_READWRITE,&protect));
    r.savedEsp=0xFFFFFFF8;assert(!IncidentProbe(4,&r));
    BYTE code[]={0x90,0x90,0xC3};IncidentSignature sig{DWORD(code),"\x90\x90\xC3",3};
    IncidentSpec spec{1,"fixture",DWORD(code),nullptr,&sig,1};assert(IncidentMatches(spec));
    code[2]=0xCC;assert(!IncidentMatches(spec));
    VirtualFree(p,0,MEM_RELEASE);
    r.eax=DWORD(p);r.ecx=DWORD(p);assert(!IncidentProbe(1,&r));assert(!IncidentProbe(2,&r));
    puts("PASS: incident pointer/permission/overflow checks, six-slot extras boundary, frame inputs, released memory and signature mismatch rejection.");
}
