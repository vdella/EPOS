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
    System_Info * si = System::info();
    unsigned sys_data_end = si->lm.sys_data + si->lm.sys_data_size + 1;
    // free(Memory_Map::RAM_BASE, pages(Memory_Map::RAM_TOP + 1 - Traits<Machine>::STACK_SIZE - Memory_Map::RAM_BASE));

    // Worst-Fit guarantees this will work
    free(align_page(sys_data_end), pages(Memory_Map::MMODE_F - align_page(sys_data_end))); // [align_page(&_end), 0x87bf9000]
    free(Memory_Map::RAM_TOP + 1 - Traits<Machine>::STACK_SIZE * Traits<Machine>::CPUS, pages(Traits<Machine>::STACK_SIZE * Traits<Machine>::CPUS));

    // Free init/setup memory
    free(Memory_Map::RAM_BASE, pages(Memory_Map::SYS - Memory_Map::RAM_BASE));

}

__END_SYS
