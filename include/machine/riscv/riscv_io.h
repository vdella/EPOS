#ifndef __riscv_io_h
#define __riscv_io_h

#include <architecture/cpu.h>
#include <machine/io.h>

#define __arch_getl(a)			(*(volatile unsigned int *)(Memory_Map::OTP_BASE + a))
#define __arch_putl(v, a)		(*(volatile unsigned int *)(Memory_Map::OTP_BASE + a) = (v))

/* These barriers need to enforce ordering on both devices or memory. */
#define mb()		ASM("fence iorw, iorw")
#define rmb()		ASM("fence ir, ir")
#define wmb()		ASM("fence ow, ow")

__BEGIN_SYS

class IO: private IO_Common
{

public:
    IO() {}

    static void writel(unsigned int val, unsigned int addr)
    {
        wmb();
        __arch_putl(val, addr);
    }

    static unsigned int readl(const unsigned int addr)
    {
        unsigned int val = __arch_getl(addr);
        rmb();
        return val;
    }
};

__END_SYS

#endif
