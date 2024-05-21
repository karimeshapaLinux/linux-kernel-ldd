# Guest running in x86- 16bit mode (real mode).
# Don't complicate things -> protected 32, 64 bits modes.

.globl _start

# 16bit mode
.code16

_start:
  mov %ax, 0
loop:
	# out value, port
	out %ax, $0x10
	inc %ax
	jmp loop
