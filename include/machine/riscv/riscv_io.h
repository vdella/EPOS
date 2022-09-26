#ifndef __riscv_io_h
#define __riscv_io_h

#include <architecture/cpu.h>
#include <machine/io.h>

#define __arch_getl(a)			(*(volatile unsigned int *)(a))
#define __arch_putl(v, a)		(*(volatile unsigned int *)(a) = (v))

#define RISCV_FENCE(p, s)       __asm__ __volatile__ ("fence " #p "," #s : : : "memory")

/* These barriers need to enforce ordering on both devices or memory. */
#define mb()		RISCV_FENCE(iorw, iorw)
#define rmb()		RISCV_FENCE(ir, ir)
#define wmb()		RISCV_FENCE(ow, ow)

#define __iormb()	            rmb()
#define __iowmb()	            wmb()

__BEGIN_SYS

class IO: private IO_Common
{

public:
    IO() {}

    static void writel(unsigned int val, volatile void *addr)
    {
        __iowmb();
        __arch_putl(val, addr);
    }

    static unsigned int readl(const volatile void *addr)
    {
        unsigned int val = __arch_getl(addr);
        __iormb();
        return val;
    }
};

__END_SYS

#endif
