#include <architecture/mmu.h>
#include <system.h>

__BEGIN_SYS

void MMU::init()
{

    db<Init, MMU>(INF) << "MMU::init()" << endl;
    System_Info * si = System::info();
    static const unsigned int HEAP_SIZE = Traits<System>::HEAP_SIZE;
    db<Init, MMU>(INF) << "heap size = " << HEAP_SIZE << endl;


    MMU::master(reinterpret_cast<Page_Directory *>(CPU::pdp()));

    free(si->pmm.free1_top - HEAP_SIZE, pages(HEAP_SIZE) + 2566);
    free(si->pmm.free1_base, pages(si->pmm.free1_top - HEAP_SIZE));

    // free(si->pmm.free1_top - 10000, 10000);
    // free(si->pmm.free1_base, pages(si->pmm.free1_top - 10000));

}

__END_SYS
