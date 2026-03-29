CC = riscv64-linux-gnu-gcc
LD = riscv64-linux-gnu-ld
OBJCOPY = riscv64-linux-gnu-objcopy
QEMU = qemu-system-riscv64

kernel.bin: start.S
	$(CC) -c $< -o start.o -march=rv64gc -mabi=lp64 -nostdlib -fno-asynchronous-unwind-tables
	$(LD) start.o -o kernel.elf -T riscv.ld
	$(OBJCOPY) -O binary kernel.elf $@

run: kernel.bin
	$(QEMU) -machine virt -bios none -device loader,file=kernel.bin,addr=0x1000 -nographic -d in_asm,cpu,exec -smp 1 -D /dev/stdout

clean:
	rm -f start.o kernel.elf kernel.bin

.PHONY: run clean
