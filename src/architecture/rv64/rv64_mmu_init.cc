#include <architecture/mmu.h>
#include <system.h>

// extern "C" char _edata;
// extern "C" char __bss_start;
// extern "C" char _end;

__BEGIN_SYS

void MMU::init()
{
    db<Init, MMU>(INF) << "MMU::init()" << endl;
    // db<Init, MMU>(WRN) << "MMU::init::dat.e=" << &_edata << ",bss.b=" << &__bss_start << ",bss.e=" << &_end << endl;

    free(Memory_Map::RAM_BASE, pages(Memory_Map::RAM_TOP + 1 - Traits<Machine>::STACK_SIZE - Memory_Map::RAM_BASE));

}

__END_SYS
