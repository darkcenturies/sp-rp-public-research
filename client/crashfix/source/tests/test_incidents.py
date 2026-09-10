"""Run existing guard regressions plus actual compiled incident wrapper tests.
The C++ validation helpers run in native_tests; here a destructive probe stub
tests register/FPU isolation and the exact game instructions/fallback paths.
"""
import runpy,pathlib
g=runpy.run_path(str(pathlib.Path(__file__).with_name('test_guards.py')))
globals().update({k:v for k,v in g.items() if not k.startswith('__')})
from unicorn.x86_const import UC_X86_REG_FP0,UC_X86_REG_MXCSR
sites=[(1,'IncidentHealth',0x47ea45,0x47ea4b),(2,'IncidentTrain',0x6f5e5d,0x6f5e63),
       (3,'IncidentExtras',0x4c9691,0x4c9697),(4,'IncidentFrame',0x7f0b00,0x7f0b06)]
fixture=next(x for x in specs if x['id']==20)
def make(site,seed,accepted):
    u=setup(fixture,seed)
    u.reg_write(UC_X86_REG_EAX,P);u.reg_write(UC_X86_REG_ECX,P+0x1000)
    u.mem_write(P+0x540,struct.pack('<f',73.5));u.mem_write(P+0x30c,bytes([seed%7]))
    w32(u,SP+4,P);w32(u,SP+8,P+0x1000)
    # Establish a nonempty x87 stack to detect helper pollution/lost values.
    scratch=STOP+0x100;u.mem_write(scratch,b'\xd9\xe8\xd9\xeb');u.emu_start(scratch,scratch+4)
    # Probe can trash caller-saved integer, flags, SSE and x87 state.
    probe=b'\x0f\x57\xc0\x0f\x57\xc9\xd9\xe8\xb9'+u32(0xabcdef01)+b'\xba'+u32(0xfedcba98)+b'\x83\xc2\x01\xb8'+u32(accepted)+b'\xc3'
    u.mem_write(symbols['IncidentProbe'],probe)
    return u
def state(u,site):
    start=SP-8 if site==4 else SP-4 if site==5 else SP
    return ([u.reg_read(r) for r in regs]+[u.reg_read(UC_X86_REG_EFLAGS)],
            bytes(u.mem_read(start,SP+64-start)),bytes(u.mem_read(P,0x2000)),
            [u.reg_read(UC_X86_REG_XMM0+i) for i in range(8)],
            [u.reg_read(r) for r in (UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,UC_X86_REG_MXCSR)],
            [u.reg_read(UC_X86_REG_FP0+i) for i in range(8)])
normal=0
for site,name,entry,end in sites:
    for seed in range(28):
        a=make(site,seed,1);b=make(site,seed,1)
        run(a,entry,{end});run(b,symbols[name],{end})
        assert state(a,site)==state(b,site),(name,seed,'valid state differs')
        normal+=1
print(f'PASS: {normal} incident valid-path comparisons including x87 register contents, SSE/MXCSR, flags, stack and target memory.')
for site,name,entry,end in sites:
    a=make(site,1,0);b=make(site,1,0)
    if site in [1,3]:a.reg_write(UC_X86_REG_EAX,0);b.reg_write(UC_X86_REG_EAX,0)
    if site==2:a.reg_write(UC_X86_REG_ECX,0);b.reg_write(UC_X86_REG_ECX,0)
    # Expected fallback machine code is independent of implementation.
    expected={1:b'\xd9\xee',2:b'\x90',3:b'\xb1\x00',4:b'\xb8\0\0\0\0\xc3'}[site]
    a.mem_write(entry,expected);stop=STOP if site==4 else entry+len(expected)
    run(a,entry,{stop});run(b,symbols[name],{STOP if site==4 else end})
    # Invalid frame input returns before the two stolen PUSH instructions.
    # Stack space below entry ESP is scratch, not an output of that path.
    assert state(a,0 if site==4 else site)==state(b,0 if site==4 else site),(name,'fallback state differs')
print('PASS: missing-actor zero-health result, train no-write, zero optional extras and pre-mutation frame rejection.')

# Exercise real downstream game branches (rather than only stolen bytes).
u=make(3,0,0);u.reg_write(UC_X86_REG_EAX,0)
run(u,symbols['IncidentExtras'],{0x4c97bf})
assert u.mem_read(0xb4e478,2)==b'\xff\xff'
assert r32(u,SP+0xc)==u.reg_read(UC_X86_REG_EDI)
u=make(2,0,0);u.reg_write(UC_X86_REG_ECX,0)
run(u,symbols['IncidentTrain'],{STOP});assert u.reg_read(UC_X86_REG_ESP)==SP+4
print('PASS: actual vehicle no-extras branch resets both selection IDs; actual train RET preserves CDECL stack cleanup.')

light_count=0xc1c96c
for name,entry,end in [('IncidentLightAdd',0x6c45b0,0x6c45b6),('IncidentLightRender',0x6c7c50,0x6c7c56)]:
    for n in [0,1]:
        a=make(0,1,1);b=make(0,1,1);w32(a,light_count,n);w32(b,light_count,n)
        run(a,entry,{end});run(b,symbols[name],{end})
        # Render's stolen PUSH EBX produces a defined stack output.
        stack_site=5 if name=='IncidentLightRender' else 0
        assert state(a,stack_site)==state(b,stack_site),(name,n,'normal path')
for n in [2,3,4,255,0x7fffffff,0xffffffff]:
    u=make(0,1,1);w32(u,light_count,n);old=bytes(u.mem_read(0xc1c990,0x200));before=u.reg_read(UC_X86_REG_EFLAGS)
    run(u,symbols['IncidentLightAdd'],{STOP})
    assert r32(u,light_count)==n and bytes(u.mem_read(0xc1c990,0x200))==old
    assert u.reg_read(UC_X86_REG_ESP)==SP+4 and u.reg_read(UC_X86_REG_EFLAGS)==before
for n in [2,3,4,255,0x7fffffff,0xffffffff]:
    u=make(0,1,1);w32(u,light_count,n)
    run(u,symbols['IncidentLightRender'],{0x6c7c56})
    assert r32(u,light_count)==2 and u.reg_read(UC_X86_REG_EAX)==2
    assert u.reg_read(UC_X86_REG_ESP)==SP-4 and r32(u,SP-4)==u.reg_read(UC_X86_REG_EBX)
# Repeated enqueue stress executes the original full add function for slots 0/1.
u=make(0,1,1);w32(u,light_count,0)
u.mem_write(0xc1c990+2*0x4c,b'\xa5'*0x100)
for i in range(1000):
    u.reg_write(UC_X86_REG_ESP,SP)
    run(u,symbols['IncidentLightAdd'],{STOP})
assert r32(u,light_count)==2 and bytes(u.mem_read(0xc1c990+2*0x4c,0x100))==b'\xa5'*0x100
print('PASS: searchlight valid replay, unsigned enqueue/render bounds and 1,000 repeated enqueues with intact overflow canary.')

# Every signature must match this executable, every six-byte patch is whole instructions.
src=(HERE.parent/'IncidentGuards.inc').read_text()
import ast
for block in re.findall(r'static const IncidentSignature \w+\[\]=\{(.*?)\};',src,re.S):
    for address,literal,size in re.findall(r'\{(0x[0-9a-fA-F]+),("(?:\\.|[^"\\])*"),(\d+)\}',block):
        expected=ast.literal_eval(literal).encode('latin1');assert len(expected)==int(size)
        assert game.get_data(int(address,16)-GAME,int(size))==expected,(address,'signature does not match executable')
for site,name,entry,end in sites+[(5,'IncidentLightAdd',0x6c45b0,0x6c45b6),(6,'IncidentLightRender',0x6c7c50,0x6c7c56)]:
    original=game.get_data(entry-GAME,6)
    ins=list(capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32).disasm(original,entry))
    assert sum(i.size for i in ins)==6,(name,'split instruction')
print('All incident wrapper tests passed. No game process was modified.')
