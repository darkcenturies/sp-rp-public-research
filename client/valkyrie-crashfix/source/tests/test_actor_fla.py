"""Run FLA's real metadata copier against the compiled resolver's native fixture.
Usage: python test_actor_fla.py path-to-FLA.asi path-to-actor-metadata-fixture.bin
"""
import sys,struct,pathlib,pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBP
p=pefile.PE(sys.argv[1]);base=p.OPTIONAL_HEADER.ImageBase
fixture=pathlib.Path(sys.argv[2]).read_bytes();assert len(fixture)==156
u=Uc(UC_ARCH_X86,UC_MODE_32);data=p.get_memory_mapped_image()
u.mem_map(base,(len(data)+4095)&~4095);u.mem_write(base,data)
u.mem_map(0x2000000,0x10000)
stream=0x2000000;extended=0x2001000;entry=0x2002000;stack=0x2008000;stop=0x2009000;id=2
def w32(a,v):u.mem_write(a,struct.pack('<I',v))
w32(base+0x1aae74,stream);w32(base+0x1aae80,extended)
u.mem_write(stream,b'\xa5'*100);u.mem_write(extended,b'\xa5'*360);u.mem_write(entry,fixture[:64])
w32(stack,stop);w32(stack+4,id);w32(stack+8,entry)
u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,0x1234);u.reg_write(UC_X86_REG_EBP,0x5678)
u.emu_start(base+0xaa0f0,stop,count=100)
s=bytearray(b'\xa5'*20);e=bytearray(b'\xa5'*72)
s[7:16]=fixture[64+7:64+16];e[16:32]=fixture[84+16:84+32]
assert u.mem_read(stream+id*20,20)==s
assert u.mem_read(extended+id*72,72)==e
assert u.reg_read(UC_X86_REG_ESP)==stack+4
assert u.reg_read(UC_X86_REG_ESI)==0x1234 and u.reg_read(UC_X86_REG_EBP)==0x5678
print('PASS: actual FLA +AA0F0 copies the compiled actor resolver metadata exactly, preserving other streaming fields and cdecl stack/nonvolatile registers.')
