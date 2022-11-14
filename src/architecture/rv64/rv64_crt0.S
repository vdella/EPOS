// EPOS RISC-V Program Starter

        .file "rv64_crt0.S"

        // Entry point called by SETUP for processors
        .section .text
        .align  8
        .global _start
        .type   _start, function
_start:
        // Temporary stack(s) for INIT are created and configure by SETUP

        // Call _init
        call      _init

        // Call main
        .align  8
        .globl  __epos_app_entry
        .type   __epos_app_entry, function
__epos_app_entry:
        call      main
        ld        a0, 0(sp)         // save main's return value to be used by exit()
        call      _fini
        #!ECALL:
halt:
        j halt
        # call      _exit
