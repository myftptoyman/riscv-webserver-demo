# Bare-metal startup code for RISC-V lwIP web server

.section .text.init
.global _start
.global _trap_entry

_start:
    # Disable interrupts
    csrw mie, zero

    # Set up stack pointer
    la sp, __stack_top

    # Clear BSS section
    la t0, __bss_start
    la t1, __bss_end
1:  bgeu t0, t1, 2f
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b
2:

    # Set up trap vector (direct mode)
    la t0, _trap_entry
    csrw mtvec, t0

    # Enable machine external interrupts (MEIE) in mie
    li t0, (1 << 11)
    csrs mie, t0

    # Jump to C main
    call main

    # If main returns, halt
halt:
    wfi
    j halt

# Trap entry point (aligned to 4 bytes)
.align 4
_trap_entry:
    # Save all caller-saved registers
    addi sp, sp, -256
    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)
    sd t2, 24(sp)
    sd t3, 32(sp)
    sd t4, 40(sp)
    sd t5, 48(sp)
    sd t6, 56(sp)
    sd a0, 64(sp)
    sd a1, 72(sp)
    sd a2, 80(sp)
    sd a3, 88(sp)
    sd a4, 96(sp)
    sd a5, 104(sp)
    sd a6, 112(sp)
    sd a7, 120(sp)

    # Call C trap handler
    call trap_handler

    # Restore registers
    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    ld t2, 24(sp)
    ld t3, 32(sp)
    ld t4, 40(sp)
    ld t5, 48(sp)
    ld t6, 56(sp)
    ld a0, 64(sp)
    ld a1, 72(sp)
    ld a2, 80(sp)
    ld a3, 88(sp)
    ld a4, 96(sp)
    ld a5, 104(sp)
    ld a6, 112(sp)
    ld a7, 120(sp)
    addi sp, sp, 256

    mret
