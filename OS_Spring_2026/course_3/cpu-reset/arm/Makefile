CC = aarch64-linux-gnu-gcc
LD = aarch64-linux-gnu-ld
OBJCOPY = aarch64-linux-gnu-objcopy
QEMU = qemu-system-aarch64

kernel.bin: start.S
	$(CC) -c $< -o start.o -march=armv8-a -nostdlib -fno-asynchronous-unwind-tables
	$(LD) start.o -o kernel.elf -T arm.ld
	$(OBJCOPY) -O binary kernel.elf $@

run: kernel.bin
	$(QEMU) -machine virt -cpu cortex-a57 -semihosting -device loader,file=kernel.bin,addr=0x0 -nographic -d in_asm,cpu,exec -smp 1 -D /dev/stdout

clean:
	rm -f start.o kernel.elf kernel.bin

.PHONY: run clean
