"""Execute the built x86 ASI guards in Unicorn; compare real GTA stolen bytes.
No DLL is injected or run in the game. Requires pefile, capstone, unicorn.
Usage: python test_guards.py [game-exe]
"""
import json, pathlib, re, struct, sys, random
import pefile, capstone
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import *
HERE=pathlib.Path(__file__).resolve().parent
DIST=HERE.parents[1]
game=pefile.PE(sys.argv[1] if len(sys.argv)>1 else r'C:\Games\Project Eagle\gta_pe.exe')
plugin=pefile.PE(str(DIST/'Valkyrie Crashfix.asi'))
specs=json.loads((HERE/'audit.json').read_text())
symbols={name:int(addr,16) for name,addr in re.findall(r'\?([^@ ]+)@@\S+\s+([0-9a-fA-F]{8})', (DIST/'Valkyrie Crashfix.map').read_text())}
symbols.update({name:int(addr,16) for name,addr in re.findall(r'\s_([A-Za-z]\w*)\s+([0-9a-fA-F]{8})', (DIST/'Valkyrie Crashfix.map').read_text())})
BASE=plugin.OPTIONAL_HEADER.ImageBase; GAME=0x400000
HEAP=0x02400000; STACK=0x02200000; SP=STACK+0x8000; TABLE=0x03000000; STOP=0x03100000; TEB=0x02000000; GDT=0x02100000
P=HEAP+0x18000
regs=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP]
names=['eax','ebx','ecx','edx','esi','edi','ebp','esp']
rmap=dict(zip(names,regs))
def u32(v): return struct.pack('<I',v&0xffffffff)
def w32(u,a,v): u.mem_write(a,u32(v))
def r32(u,a): return struct.unpack('<I',u.mem_read(a,4))[0]
def descriptor(base,limit,access=0x93,flags=0xC):
    return struct.pack('<HHBBBB',limit&65535,base&65535,(base>>16)&255,access,((limit>>16)&15)|(flags<<4),(base>>24)&255)
def setup(s,seed=0):
    rng=random.Random(seed);u=Uc(UC_ARCH_X86,UC_MODE_32)
    for base,data in [(GAME,game.get_memory_mapped_image()),(BASE,plugin.get_memory_mapped_image())]:
        u.mem_map(base,(len(data)+4095)&~4095);u.mem_write(base,data)
    for a,n in [(HEAP,0x40000),(STACK,0x10000),(TABLE,0x10000),(STOP,4096),(TEB,4096),(GDT,4096)]:u.mem_map(a,n)
    u.mem_write(HEAP,u32(P)*(0x40000//4));u.mem_write(STACK,u32(P)*(0x10000//4));w32(u,SP,STOP)
    for i,r in enumerate(regs[:-1]):u.reg_write(r,HEAP+0x1000*(i+1))
    u.reg_write(UC_X86_REG_ESP,SP);u.reg_write(UC_X86_REG_EFLAGS,0x202|rng.choice([0,1,4,0x40,0x80,0x800,0x8c5]))
    for i in range(8):u.reg_write(UC_X86_REG_XMM0+i,rng.getrandbits(128))
    u.reg_write(UC_X86_REG_FPCW,0x37f)
    # Windows x86 FS:[0] exception chain, with a real 32-bit segment descriptor.
    u.mem_write(GDT+8,descriptor(0,0xfffff));u.mem_write(GDT+16,descriptor(TEB,0xfffff))
    u.reg_write(UC_X86_REG_GDTR,(0,GDT,0xff,0));u.reg_write(UC_X86_REG_FS,16)
    u.reg_write(UC_X86_REG_SS,8);u.reg_write(UC_X86_REG_DS,8);u.reg_write(UC_X86_REG_ES,8)
    w32(u,TEB,SP)
    n=s['id']
    if n==1:
        # 0x8D12CC is a small plugin OFFSET, ESI is the RpMaterial pointer.
        # 2.2's inverted fixture missed the healthy-material false positive.
        offset=0x20+4*(seed%6);w32(u,0x8d12cc,offset)
        w32(u,u.reg_read(UC_X86_REG_ESI)+offset,P)
    if n==4:u.reg_write(UC_X86_REG_EAX,0x1234567+seed);u.reg_write(UC_X86_REG_ECX,7)
    if n==5:
        u.reg_write(UC_X86_REG_ECX,405);w32(u,TABLE+405*4,P);w32(u,symbols['g_modelTable'],TABLE)
    if n==14:w32(u,0xbd00f8,P)
    if n==8:u.reg_write(UC_X86_REG_EDX,0x20)
    if n==22:u.reg_write(UC_X86_REG_EAX,2);u.reg_write(UC_X86_REG_ECX,0x30)
    if n==26:u.reg_write(UC_X86_REG_EBP,3)
    if n==23:w32(u,SP+8,seed%7)
    if n==29:w32(u,SP+8,seed%45)
    if n==16:
        # A deterministic callee with register/flag changes and a call counter.
        # The wrapper must call it exactly once and preserve all returned state.
        getter=0x4d15e0;w32(u,P+0x300,P);w32(u,P+0x304,0)
        u.mem_write(getter,b'\xff\x05'+u32(P+0x304)+b'\xa1'+u32(P+0x300)+b'\xba\x67\x45\x23\x01\x83\xc2\x01\xc3')
        w32(u,symbols['g_call16'],getter)
    original=bytes.fromhex(s['original'])
    if n==5:original=original[:3]+u32(TABLE)
    replay=BASE+0x300000+n*0x1000
    replay_bytes=b'' if n==16 else original
    u.mem_map(replay,4096);u.mem_write(replay,replay_bytes+b'\xe9'+u32(s['address']+s['size']-(replay+len(replay_bytes)+5)))
    w32(u,symbols['g_resume']+n*4,replay)
    u.mem_write(s['address'],original)
    return u
def run(u,entry,stops):
    seen=[]
    def check(uc,a,size,data):
        if a in stops: seen.append(a);uc.emu_stop()
    h=u.hook_add(UC_HOOK_CODE,check)
    try: u.emu_start(entry,0,count=500)
    except Exception as e: raise RuntimeError(f'entry={entry:08X} fault EIP={u.reg_read(UC_X86_REG_EIP):08X}, regs={[hex(u.reg_read(r)) for r in regs]}') from e
    u.hook_del(h)
    assert seen, f'failed to reach expected exit; EIP={u.reg_read(UC_X86_REG_EIP):08X}'
    return seen[-1]
def snapshot(u,guard_id):
    sp=u.reg_read(UC_X86_REG_ESP)
    # SUB ESP reserves uninitialized locals; their old contents are not an ABI
    # promise. Guard 25's stolen PUSH, however, creates a meaningful new slot.
    start=SP-4 if guard_id==25 else SP
    return ([u.reg_read(r) for r in regs]+[u.reg_read(UC_X86_REG_EFLAGS)],
            bytes(u.mem_read(start,SP+0x100-start)),
            [u.reg_read(UC_X86_REG_XMM0+i) for i in range(8)],u.reg_read(UC_X86_REG_FPCW))
count=0
for s in specs:
    n=s['id'];original=bytes.fromhex(s['original'])
    ins=list(capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32).disasm(original,s['address']))
    assert sum(x.size for x in ins)==s['size'],f'{n}: split instruction'
    if n!=16:assert all(x.mnemonic not in ['call','jmp'] and not x.mnemonic.startswith('j') for x in ins),f'{n}: non-relocatable replay'
    else:assert len(ins)==1 and ins[0].mnemonic=='call'
    for seed in range(12):
        u=setup(s,seed);v=setup(s,seed)
        stop=s['address']+s['size']
        run(u,s['address'],{stop});run(v,symbols[f'Guard{n:02}'],{stop})
        assert snapshot(u,n)==snapshot(v,n),f'valid path differs: guard {n}, seed {seed}: {snapshot(u,n)[0]} != {snapshot(v,n)[0]}'
        assert r32(v,symbols['g_blocked']+n*4)==0
        if n==16:assert r32(u,P+0x304)==1 and r32(v,P+0x304)==1
        count+=1
print(f'PASS: {count} valid-path comparisons (GPRs, flags, active stack, XMM, x87 control); whole-instruction replay verified.')

# Invalid cases: each extra dereference and each decision branch is exercised.
cases={
1:[('material',lambda u:u.reg_write(UC_X86_REG_ESI,0)),
('unregistered offset',lambda u:w32(u,0x8d12cc,0xffffffff)),
('address overflow',lambda u:(u.reg_write(UC_X86_REG_ESI,0xfffffff0),w32(u,0x8d12cc,0x20))),
('specular entry',lambda u:w32(u,u.reg_read(UC_X86_REG_ESI)+r32(u,0x8d12cc),0))],
2:[('colmodel',lambda u:u.reg_write(UC_X86_REG_EAX,0)),('coldata',lambda u:w32(u,u.reg_read(UC_X86_REG_EAX)+0x2c,0)),('suspension',lambda u:w32(u,P+0x10,0))],
3:[('vehicle',lambda u:u.reg_write(UC_X86_REG_ECX,0)),('handling',lambda u:w32(u,u.reg_read(UC_X86_REG_ECX)+0x384,0))],
4:[('zero',lambda u:u.reg_write(UC_X86_REG_ECX,0)),('overflow',lambda u:(u.reg_write(UC_X86_REG_ECX,0xffffffff),u.reg_write(UC_X86_REG_EAX,0x80000000)))],
5:[('model',lambda u:w32(u,TABLE+405*4,0)),('structure',lambda u:w32(u,P+0x5c,0))],
6:[('null',lambda u:u.reg_write(UC_X86_REG_ECX,0))],7:[('null',lambda u:u.reg_write(UC_X86_REG_ECX,0))],8:[('null',lambda u:u.reg_write(UC_X86_REG_ECX,0))],
9:[('null',lambda u:u.reg_write(UC_X86_REG_ESI,0))],10:[('null',lambda u:u.reg_write(UC_X86_REG_ECX,0))],11:[('null',lambda u:u.reg_write(UC_X86_REG_ECX,0))],12:[('null',lambda u:u.reg_write(UC_X86_REG_EDI,0))],
14:[('volumes',lambda u:w32(u,0xbd00f8,0))],16:[('association',lambda u:w32(u,P+0x300,0))],17:[('null',lambda u:u.reg_write(UC_X86_REG_EAX,0))],18:[('null',lambda u:u.reg_write(UC_X86_REG_EBP,0))],
20:[('object',lambda u:u.reg_write(UC_X86_REG_ECX,0)),('matrix',lambda u:w32(u,u.reg_read(UC_X86_REG_ECX)+0x14,0))],21:[('task',lambda u:w32(u,SP+8,0))],22:[('nodes',lambda u:u.reg_write(UC_X86_REG_EDX,0))],
23:[(str(x),lambda u,x=x:w32(u,SP+8,x)) for x in [7,15,16,0xffffffff]],24:[('null',lambda u:u.reg_write(UC_X86_REG_EBP,0))],
25:[('task',lambda u:u.reg_write(UC_X86_REG_ESI,0)),('vehicle',lambda u:w32(u,u.reg_read(UC_X86_REG_ESI)+0x10,0))],
26:[('list',lambda u:u.reg_write(UC_X86_REG_EBX,0x130))],27:[('vehicle',lambda u:w32(u,u.reg_read(UC_X86_REG_EDI)+0x58c,0))],
28:[('object',lambda u:u.reg_write(UC_X86_REG_ESI,0)),('dummy',lambda u:w32(u,u.reg_read(UC_X86_REG_ESI)+0x170,0))],
29:[(str(x),lambda u,x=x:w32(u,SP+8,x)) for x in [0xffff,0xfffe,0x8000]]+[
('slot count',lambda u:(w32(u,SP+8,45),u.mem_write(u.reg_read(UC_X86_REG_ECX)+0xc,struct.pack('<H',45)))),
('slot array',lambda u:w32(u,u.reg_read(UC_X86_REG_ECX),0))],30:[('clump',lambda u:w32(u,SP+4,0))]}
invalid_count=0
for s in specs:
    n=s['id']
    for label,mutate in cases[n]:
        u=setup(s);mutate(u)
        stops=set(s['targets'])|{STOP}
        # Execute the real epilogue for SEH cases instead of stopping before it.
        if n==5:
            w32(u,SP,0xdead0001);w32(u,SP+4,0xdead0002);w32(u,SP+0xc0,0x12345678);w32(u,SP+0xcc,STOP);stops={STOP}
        if n==14:w32(u,SP,0x12345678);w32(u,SP+0xc,STOP);stops={STOP}
        if n==16:
            w32(u,SP+4,0xdead0001);w32(u,SP+8,0xdead0002);w32(u,SP+0x54,0x12345678);w32(u,SP+0x60,STOP);stops={STOP}
        if n==18:w32(u,SP+8,STOP)
        if n==25:w32(u,SP+8,STOP)
        if n==22:
            # Destination indexing in the game is ECX byte offset, not a pointer.
            u.reg_write(UC_X86_REG_ECX,0x18)
        if n==23:stops={s['address']+s['size']}
        run(u,symbols[f'Guard{n:02}'],stops)
        assert r32(u,symbols['g_blocked']+n*4)==1,(n,label,'counter')
        if n in [5,14,16]:assert r32(u,TEB)==0x12345678,(n,'SEH chain')
        if n==5:
            assert u.reg_read(UC_X86_REG_ESP)==SP+0xd0
            assert u.reg_read(UC_X86_REG_EDI)==0xdead0001 and u.reg_read(UC_X86_REG_ESI)==0xdead0002
        if n==14:assert u.reg_read(UC_X86_REG_ESP)==SP+0x1c
        if n==16:
            assert u.reg_read(UC_X86_REG_ESP)==SP+0x64
            assert u.reg_read(UC_X86_REG_EDI)==0xdead0001 and u.reg_read(UC_X86_REG_ESI)==0xdead0002
        if n==3:assert u.reg_read(UC_X86_REG_ESP)==SP+12 and r32(u,P)==0
        if n==18:assert u.reg_read(UC_X86_REG_ESP)==SP+24
        if n==25:assert u.reg_read(UC_X86_REG_ESP)==SP+12
        if n==23:assert r32(u,SP+8)==0
        if n==22:assert r32(u,P+0x28)==0 and r32(u,P+0x2c)==0
        invalid_count+=1
print(f'PASS: {invalid_count} invalid-path cases, including real SEH epilogues, argument cleanup, output initialization, counters and bounds.')

# Regression: slots with low byte 0xFF are not -1; all valid door indices accepted.
s=next(x for x in specs if x['id']==29)
for slot in [0xff,0x1ff,0x7fff]:
    u=setup(s);w32(u,SP+8,slot);run(u,symbols['Guard29'],{s['address']+s['size']});assert r32(u,symbols['g_blocked']+29*4)==0
print('PASS: 16-bit bank-slot regression: 0x00FF / 0x01FF / 0x7FFF are not misclassified as negative.')

# Pool helper is an actual C++ call and may clobber XMM/x87. Verify the wrapper
# restores that state using a deliberately destructive mock helper.
s=next(x for x in specs if x['id']==20)
for accepted in [0,1]:
    u=setup(s);v=setup(s)
    replay=BASE+0x400000;v.mem_map(replay,4096)
    original=game.get_data(0x40fb80-GAME,6)
    v.mem_write(replay,original+b'\xe9'+u32(0x40fb86-(replay+11)))
    w32(v,symbols['poolTrampoline'],replay)
    v.mem_write(symbols['PoolReadable'],b'\x0f\x57\xc0\xd9\xe8\xb8'+u32(accepted)+b'\xc3')
    before=[v.reg_read(UC_X86_REG_XMM0+i) for i in range(8)]
    fp_before=(v.reg_read(UC_X86_REG_FPSW),v.reg_read(UC_X86_REG_FPTAG),v.reg_read(UC_X86_REG_FPCW))
    run(v,symbols['PoolGuard'],{0x40fb86 if accepted else STOP})
    assert [v.reg_read(UC_X86_REG_XMM0+i) for i in range(8)]==before
    assert (v.reg_read(UC_X86_REG_FPSW),v.reg_read(UC_X86_REG_FPTAG),v.reg_read(UC_X86_REG_FPCW))==fp_before
    if accepted:
        run(u,0x40fb80,{0x40fb86});assert snapshot(u,25)==snapshot(v,25)
    else:assert v.reg_read(UC_X86_REG_EAX)==0 and v.reg_read(UC_X86_REG_ESP)==SP+4
print('PASS: pool wrapper preserves XMM and x87 across helper calls; valid replay and null return verified.')
print('All compiled guard tests passed. This is isolated machine-code validation, not gameplay certification.')
