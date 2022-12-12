#include <architecture/mmu.h>
#include <system.h>

__BEGIN_SYS

void MMU::init()
{

    db<Init, MMU>(INF) << "MMU::init()" << endl;
    System_Info * si = System::info();

    MMU::master(reinterpret_cast<Page_Directory *>(CPU::pdp()));

    free(si->pmm.free1_top - 10000 * sizeof(Page), 10000);
    free(si->pmm.free1_base, pages(si->pmm.free1_top) - 10000);
}

__END_SYS
