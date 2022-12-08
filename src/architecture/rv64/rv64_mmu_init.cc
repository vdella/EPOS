#include <architecture/mmu.h>
#include <system.h>

__BEGIN_SYS

void MMU::init()
{
    typedef MMU::Phy_Addr Phy_Addr;

    db<Init, MMU>(INF) << "MMU::init()" << endl;
    System_Info * si = System::info();

    db<Init, MMU>(INF) << "Sys Data: " << (Phy_Addr) si->lm.sys_data << endl;
    db<Init, MMU>(INF) << "Sys Data Size: " << (Phy_Addr) si->lm.sys_data_size << endl;

    unsigned long sys_data_end = si->lm.sys_data + si->lm.sys_data_size + 1;

    db<Init, MMU>(INF) << "Sys Data End: " << (Phy_Addr) sys_data_end << endl;
    db<Init, MMU>(INF) << "Memory Map Sys Data End: " << (Phy_Addr) (Memory_Map::SYS_DATA + si->lm.sys_data_size + 1) << endl;

    MMU::master(reinterpret_cast<Page_Directory *>(CPU::pdp()));

    free(si->pmm.free1_base, pages(si->pmm.free1_top));

}

__END_SYS
