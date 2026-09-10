"""Generate reviewable MSVC x86 guards and exact GTA build signatures."""
import json, pathlib, pefile, sys, hashlib
here=pathlib.Path(__file__).resolve().parent
dest=here.parent
game=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else r'C:\Games\Project Eagle\gta_pe.exe')
if hashlib.sha256(game.read_bytes()).hexdigest()!='f115b49c5ac3a8fcedb00ee216730b2a6f573afe4908b2ac328a9cc2b9d0dcb3':
    raise SystemExit('Unsupported EXE: do not regenerate trusted signatures from an unreviewed binary')
p=pefile.PE(str(game))
sites=json.loads((here/'legacy-sites.json').read_text())
spec=[]
def add(n,title,checks,fail,targets=(),address=None,size=None,notes='',following=16):
    s=next(x for x in sites if x['name']==('CClumpModelInfo_GetFrameFromId' if n==30 else f'CrashFix_Misc{n}')).copy()
    s.update(id=n,title=title,checks=checks.strip(),fail=fail.strip(),targets=list(targets),notes=notes,following=following)
    if address is not None: s['address']=address
    if size is not None: s['size']=size
    spec.append(s)
def target(a): return f'push 0{a:X}h\nret'  # synthetic tail transfer; no register clobber
add(1,'Missing vehicle specular material data', '''
cmp esi, 10000h
jb invalid
push eax
mov eax, dword ptr ds:[8D12CCh]
test eax, eax
js bad_scratch
add eax, esi
jc bad_scratch
mov eax, [eax]
cmp eax, 10000h
jb bad_scratch
pop eax
jmp valid
bad_scratch:
pop eax
jmp invalid
''',target(0x5d9b09),[0x5d9b09],address=0x5d9a7a,size=8,following=5,notes='0x8D12CC contains a signed plugin OFFSET, not a pointer. Accepts normal small offsets; checks the material, unregistered offset, address overflow and resulting specular-data pointer. Preserves the SkyGFX decision.')
add(2,'Missing vehicle collision data','''
cmp eax, 10000h
jb invalid
push eax
mov eax, [eax+2Ch]
cmp eax, 10000h
jb bad_scratch
cmp dword ptr [eax+10h], 10000h
jb bad_scratch
pop eax
jmp valid
bad_scratch:
pop eax
jmp invalid
''',target(0x6b3775),[0x6b3775],notes='No EBX clobber from suspension-line probe.')
add(3,'Missing car-door vehicle or handling','''
cmp ecx, 10000h
jb invalid
cmp dword ptr [ecx+384h], 10000h
jb invalid
''','''
mov ecx, [esp+4]
mov dword ptr [ecx], 0
ret 8
''',notes='Sets output group to default and uses callee cleanup RET 8, not a CDECL abort.')
add(4,'Sound service division faults','''
test ecx, ecx
jz invalid
cmp ecx, -1
jne valid
cmp eax, 80000000h
je invalid
''',target(0x4f0b07),[0x4f0b07],notes='Handles zero divisor and signed INT_MIN/-1 overflow.')
add(5,'Relocated vehicle model table','''
push edi
mov edi, g_modelTable
mov edi, [edi+ecx*4]
cmp edi, 10000h
jb bad_scratch
cmp dword ptr [edi+5Ch], 10000h
jb bad_scratch
pop edi
jmp valid
bad_scratch:
pop edi
jmp invalid
''','pop edi\n'+target(0x5dfcc4),[0x5dfcc4],notes='Captures the live table operand, not 0xA9B0C8. Missing model/vehicle structure uses existing SEH epilogue.')
add(6,'Null animation association','cmp ecx, 10000h\njb invalid',target(0x4d1a44),[0x4d1a44])
add(7,'Null collision model','cmp ecx, 10000h\njb invalid',target(0x417bff),[0x417bff])
add(8,'Null 2D-effect object','cmp ecx, 10000h\njb invalid',target(0x734871),[0x734871])
add(9,'Null projectile','cmp esi, 10000h\njb invalid','mov eax, [esp+7Ch]\nxor edi, edi\n'+target(0x73983a),[0x73983a],notes='Reloads the projectile loop index before advancing; avoids stale EAX.')
add(10,'Null transform vector','cmp ecx, 10000h\njb invalid','''
mov eax, [esp+1Ch]
mov dword ptr [eax], 0
mov dword ptr [eax+4], 0
mov dword ptr [eax+8], 0
'''+target(0x533539),[0x533539],notes='Returns initialized zero vector through the original output pointer.')
add(11,'Null animation association (second path)','cmp ecx, 10000h\njb invalid',target(0x4d2e03),[0x4d2e03])
add(12,'Null animation block','cmp edi, 10000h\njb invalid',target(0x4d4222),[0x4d4222])
# 13 cannot safely manufacture an animation association for callers.
add(14,'Uninitialized audio event volumes','cmp dword ptr ds:[0BD00F8h], 10000h\njb invalid','''
mov eax, [esp]
mov fs:[0], eax
add esp, 0Ch
ret 0Ch
''',notes='Restores the SEH chain established before this hook; old early return left FS:[0] dangling.')
# 15 intentionally retired: an IO timeout is not evidence that IO completed.
add(16,'Missing first animation association','cmp eax, 10000h\njb invalid','add esp, 4\n'+target(0x5e5eba),[0x5e5eba,0x5e5805],address=0x5e5810,size=5,following=0,notes='Calls the original association getter once, then checks its result BEFORE the existing Fastman92 extension stub. Real epilogue restores EDI/ESI and FS:[0].')
add(17,'Null RenderWare frame plugin owner','cmp eax, 10000h\njb invalid',target(0x7f1236),[0x7f1236])
add(18,'Missing wheel frame','cmp ebp, 10000h\njb invalid','''
mov eax, [esp+10h]
mov dword ptr [eax], 0
mov dword ptr [eax+4], 0
mov dword ptr [eax+8], 0
pop esi
pop ebp
ret 0Ch
''')
add(20,'Null placeable or matrix','''
cmp ecx, 10000h
jb invalid
cmp dword ptr [ecx+14h], 10000h
jb invalid
''','ret',notes='Checks the matrix member as well as the object.')
add(21,'Null fall-out callback task','''
cmp dword ptr [esp+0Ch], 10000h
jb invalid
''','ret',notes='Checks actual task argument; no unpopulated association-lifetime registry.')
add(22,'Missing animation node source','cmp edx, 10000h\njb invalid','''
mov ebx, [esi+10h]
mov dword ptr [ebx+ecx+10h], 0
mov dword ptr [ebx+ecx+14h], 0
'''+target(0x4cef19),[0x4cef19],notes='Zeros only this node and returns to the real loop; no duplicated loop or changed valid behavior.')
add(23,'Door offset index bounds','cmp dword ptr [esp+0Ch], 7\njae invalid','mov dword ptr [esp+8], 0\njmp dword ptr [g_resume+23*4]',notes='The array has 7 vectors, not 16. Unsigned comparison rejects negative indices too.')
add(24,'Null RenderWare child frame','cmp ebp, 10000h\njb invalid','xor ebp, ebp\nxor eax, eax\n'+target(0x7f0dce),[0x7f0dce])
add(25,'Car-door callback without vehicle','''
cmp esi, 10000h
jb invalid
cmp dword ptr [esi+10h], 10000h
jb invalid
''','pop esi\npop ecx\nret',notes='Preserves EAX on valid path; original LEA/PUSH replayed exactly.')
add(26,'Invalid shot pedestrian list','cmp ebx, 10000h\njb invalid','xor edi, edi\ndec ebp\ntest edi, edi\n'+target(0x739fa6),[0x739fa6])
add(27,'Dying-in-car task without vehicle','''
cmp dword ptr [edi+58Ch], 10000h
jb invalid
''',target(0x63782a),[0x63782a],address=0x637804,size=8,notes='Hooks the actual vehicle dereference after the original ped-state branch; preserves valid flags and ECX.')
add(28,'Garage door without dummy','''
cmp esi, 10000h
jb invalid
cmp dword ptr [esi+170h], 10000h
jb invalid
''',target(0x44a650),[0x44a650])
add(29,'Sound-bank slot bounds','''
push eax
movsx eax, word ptr [esp+10h]
test eax, eax
jl bad_scratch
cmp ax, word ptr [ecx+0Ch]
jae bad_scratch
cmp dword ptr [ecx], 10000h
jb bad_scratch
pop eax
jmp valid
bad_scratch:
pop eax
jmp invalid
''',target(0x4e0227),[0x4e0227],notes='Checks the signed slot against the live loader slot count at +0x0C and checks the slot array. Does not misclassify every low byte FF as -1.')
add(30,'Null clump frame lookup','cmp dword ptr [esp+8], 10000h\njb invalid','xor eax, eax\nret',notes='Retains normal null-result semantics; removes recursive nearest-frame guessing.')
lines=['// Generated by tests/generate_guards.py. Reviewed instruction contracts in audit.json.',
       'void* g_resume[31]{};', 'volatile LONG g_blocked[31]{};', 'DWORD g_modelTable=0;', 'void* g_call16=nullptr;']
for s in spec:
    n=s['id']; lines += [f'// {s["title"]}. {s["notes"]}',f'__declspec(naked) void Guard{n:02d}() {{', '    __asm {']
    if n==16:lines += ['        call dword ptr [g_call16]']
    lines += ['        pushfd']
    lines += ['        '+x for x in s['checks'].splitlines()]
    lines += ['    valid:', '        popfd',f'        jmp dword ptr [g_resume+{n}*4]', '    invalid:',f'        lock inc dword ptr [g_blocked+{n}*4]','        popfd']
    lines += ['        '+x for x in s['fail'].splitlines()]
    lines += ['    }','}']
lines += ['struct GuardCheck { DWORD address; const char* bytes; size_t size; };',
          'struct GuardSpec { int id; const char* name; DWORD address; size_t size; void* handler; const char* expected; const char* mask; size_t expectedSize; const GuardCheck* exits; size_t exitCount; };']
def lit(b): return '"'+''.join(f'\\x{x:02X}' for x in b)+'"'
for s in spec:
    n=s['id']; a=s['address']; size=s['size']
    s['original']=p.get_data(a-0x400000,size).hex()
    s['expected']=p.get_data(a-0x400000,size+s['following']).hex(); s['mask']='x'*(size+s['following'])
    if n==5: s['mask']=s['mask'][:3]+'????'+s['mask'][7:]
    s['exitChecks']=[{'address':t,'bytes':p.get_data(t-0x400000,16).hex()} for t in s['targets']]
    if s['targets']:
        lines += [f'static const GuardCheck checks{n}[]={{']+[f'    {{0x{t["address"]:X},{lit(bytes.fromhex(t["bytes"]))},16}},' for t in s['exitChecks']]+['};']
lines += ['static const GuardSpec guardSpecs[]={']
for s in spec:
    n=s['id']; lines += [f'    {{{n},"{s["title"]}",0x{s["address"]:X},{s["size"]},(void*)&Guard{n:02d},{lit(bytes.fromhex(s["expected"]))},"{s["mask"]}",{len(s["mask"])},{f"checks{n}" if s["targets"] else "nullptr"},{len(s["targets"])}}},']
lines += ['};']
(dest/'AuditedGuards.inc').write_text('\n'.join(lines)+'\n')
(dest/'tests/audit.json').write_text(json.dumps(spec,indent=2))
print(f'Generated {len(spec)} individually checked guards; 13 and 15 retired.')
