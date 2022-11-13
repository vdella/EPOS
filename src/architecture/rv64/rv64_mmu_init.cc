#include <architecture/mmu.h>
#include <system.h>

// extern "C" char _edata;
// extern "C" char __bss_start;
// extern "C" char _end;

__BEGIN_SYS

void MMU::init()
{
    db<Init, MMU>(INF) << "MMU::init()" << endl;

    free(Memory_Map::RAM_BASE, pages(Memory_Map::RAM_TOP + 1 - Traits<Machine>::STACK_SIZE - Memory_Map::RAM_BASE));

}

__END_SYS
