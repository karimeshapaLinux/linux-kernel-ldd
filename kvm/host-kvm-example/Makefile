AS = as
CC = gcc
LD = ld
CFLAGS = -32
LDFLAGS = -m elf_i386 --oformat binary -N -e _start -Ttext 0x10000

host-kvm-simple: host_kvm_simple.c
	$(CC) $^ -o $@

guest.o: guest.S
	$(AS) $(CFLAGS) $^ -o $@

guest: guest.o
	$(LD) $(LDFLAGS) $^ -o $@

clean:
	rm -rf guest.o guest host-kvm-simple