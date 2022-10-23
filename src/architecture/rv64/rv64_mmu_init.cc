#include <architecture/mmu.h>
#include <system.h>

extern "C" char _edata;
extern "C" char __bss_start;
extern "C" char _end;

__BEGIN_SYS

void MMU::init()
{
    db<Init, MMU>(TRC) << "MMU::init()" << endl;

    db<Init, MMU>(INF) << "MMU::init::dat.e=" << &_edata << ",bss.b=" << &__bss_start << ",bss.e=" << &_end << endl;

    // free(align_page(&_end), pages(Memory_Map::RAM_TOP + 1 - Traits<Machine>::STACK_SIZE - align_page(&_end)));
    //free(Memory_Map::RAM_TOP + 1 - Traits<Machine>::STACK_SIZE * Traits<Machine>::CPUS, pages(Traits<Machine>::STACK_SIZE * Traits<Machine>::CPUS));

    free(Memory_Map::RAM_BASE, pages(Memory_Map::RAM_TOP + 1 - Traits<Machine>::STACK_SIZE - Memory_Map::RAM_BASE));
    // free(System::info()->pmm.free1_base, pages(System::info()->pmm.free1_top - System::info()->pmm.free1_base));
}

__END_SYS
