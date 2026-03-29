NASM = nasm
QEMU = qemu-system-x86_64

boot.bin: boot.asm
	$(NASM) $< -f bin -o $@

run: boot.bin
	$(QEMU) -drive format=raw,file=$< -nographic -d in_asm,cpu,exec -D /dev/stdout

clean:
	rm -f boot.bin

.PHONY: run clean
