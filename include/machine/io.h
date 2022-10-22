// EPOS IO Mediator Common Package

#ifndef __io_h
#define __io_h

#include <system/config.h>

__BEGIN_SYS

class IO_Common
{
protected:
    IO_Common() {}

public:
    static void writel(unsigned int val, volatile void *addr);
    static unsigned int readl(const volatile void *addr);
};

__END_SYS

#endif

#if defined(__IO_H) && !defined(__io_common_only__)
#include __IO_H
#endif
