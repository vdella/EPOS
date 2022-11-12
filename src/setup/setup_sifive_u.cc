// EPOS SiFive-U (RISC-V) SETUP

#include <architecture.h>
#include <machine.h>
#include <utility/elf.h>
#include <utility/string.h>

using namespace EPOS::S;
typedef unsigned long Reg;

// timer handler
extern "C" [[gnu::interrupt, gnu::aligned(8)]] void _mmode_forward()
{
    Reg id = CPU::mcause();
    if ((id & CLINT::INT_MASK) == CLINT::IRQ_MAC_TIMER)
    {
        Timer::reset();
        CPU::sie(CPU::STI);
    }
    Reg interrupt_id = 1 << ((id & CLINT::INT_MASK) - 2);
    if (CPU::int_enabled() && (CPU::sie() & (interrupt_id)))
        CPU::mip(interrupt_id);
}

extern "C"
{
    void _start();

    void _int_entry();

    // SETUP entry point is in .init (and not in .text), so it will be linked first and will be the first function after the ELF header in the image
    void _entry() __attribute__((used, naked, section(".init")));
    void _setup();

    // LD eliminates this variable while performing garbage collection, that's why the used attribute.
    char __boot_time_system_info[sizeof(EPOS::S::System_Info)] __attribute__((used)) = "<System_Info placeholder>"; // actual System_Info will be added by mkbi!
}

__BEGIN_SYS

extern OStream kout, kerr;

class Setup
{
private:
    // Physical memory map
    static const unsigned long RAM_BASE = Memory_Map::RAM_BASE;
    static const unsigned long RAM_TOP = Memory_Map::RAM_TOP;
    static const unsigned long APP_LOW = Memory_Map::APP_LOW;
    static const unsigned long APP_HIGH = Memory_Map::APP_HIGH;
    static const unsigned long APP_CODE = Memory_Map::APP_CODE;
    static const unsigned long APP_DATA = Memory_Map::APP_DATA;
    static const unsigned long MIO_BASE = Memory_Map::MIO_BASE;
    static const unsigned long MIO_TOP = Memory_Map::MIO_TOP;
    static const unsigned long FREE_BASE = Memory_Map::FREE_BASE;
    static const unsigned long FREE_TOP = Memory_Map::FREE_TOP;
    static const unsigned long PHY_MEM = Memory_Map::PHY_MEM;
    static const unsigned long SETUP = Memory_Map::SETUP;
    static const unsigned long BOOT_STACK = Memory_Map::BOOT_STACK;
    static const unsigned long PAGE_TABLES = Memory_Map::PAGE_TABLES;

    static const unsigned long SYS = Memory_Map::SYS;
    static const unsigned long SYS_CODE = Memory_Map::SYS_CODE;
    static const unsigned long SYS_INFO = Memory_Map::SYS_INFO;
    static const unsigned long SYS_PT = Memory_Map::SYS_PT;
    static const unsigned long SYS_PD1 = Memory_Map::SYS_PD1;
    static const unsigned long SYS_PD2 = Memory_Map::SYS_PD2;
    static const unsigned long SYS_DATA = Memory_Map::SYS_DATA;
    static const unsigned long SYS_STACK = Memory_Map::SYS_STACK;
    static const unsigned long SYS_HEAP = Memory_Map::SYS_HEAP;
    static const unsigned long SYS_HIGH = Memory_Map::SYS_HIGH;



    static const unsigned int PT_ENTRIES = MMU::PT_ENTRIES;
    static const unsigned int PD_ENTRIES = PT_ENTRIES;

    // Architecture Imports
    typedef CPU::Reg Reg;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;
    typedef MMU::RV64_Flags RV64_Flags;
    typedef MMU::Page_Table Page_Table;
    typedef MMU::Page_Directory Page_Directory;

public:
    Setup();

private:
    void say_hi();
    void init_mmu();
    void mmu_init();
    void enable_paging();
    void call_next();

private:
    System_Info *si;
};

Setup::Setup()
{
    Display::init();
    kout << endl;
    kerr << endl;

    si = reinterpret_cast<System_Info *>(&__boot_time_system_info);
    if (si->bm.n_cpus > Traits<Machine>::CPUS)
        si->bm.n_cpus = Traits<Machine>::CPUS;

    db<Setup>(TRC) << "Setup(si=" << reinterpret_cast<void *>(si) << ",sp=" << CPU::sp() << ")" << endl;
    db<Setup>(INF) << "Setup:si=" << *si << endl;

    // Print basic facts about this EPOS instance
    say_hi();
    //build page tables
    // init_mmu();

    enable_paging();

    // SETUP ends here, so let's transfer control to the next stage (INIT or APP)
    call_next();
}

void Setup::say_hi()
{
    db<Setup>(TRC) << "Setup::say_hi()" << endl;
    db<Setup>(INF) << "System_Info=" << *si << endl;

    if (si->bm.application_offset == -1U)
        db<Setup>(ERR) << "No APPLICATION in boot image, you don't need EPOS!" << endl;

    kout << "This is EPOS!\n"
         << endl;
    kout << "Setting up this machine as follows: " << endl;
    kout << "  Mode:         " << ((Traits<Build>::MODE == Traits<Build>::LIBRARY) ? "library" : (Traits<Build>::MODE == Traits<Build>::BUILTIN) ? "built-in"
                                                                                                                                                 : "kernel")
         << endl;
    kout << "  Processor:    " << Traits<Machine>::CPUS << " x RV" << Traits<CPU>::WORD_SIZE << " at " << Traits<CPU>::CLOCK / 1000000 << " MHz (BUS clock = " << Traits<CPU>::CLOCK / 1000000 << " MHz)" << endl;
    kout << "  Machine:      SiFive-U" << endl;
    kout << "  Memory:       " << (RAM_TOP + 1 - RAM_BASE) / (1024 * 1024) << " MB [" << reinterpret_cast<void *>(RAM_BASE) << ":" << reinterpret_cast<void *>(RAM_TOP) << "]" << endl;
    kout << "  User memory:  " << (FREE_TOP - FREE_BASE) / (1024 * 1024) << " MB [" << reinterpret_cast<void *>(FREE_BASE) << ":" << reinterpret_cast<void *>(FREE_TOP) << "]" << endl;
    kout << "  I/O space:    " << (MIO_TOP + 1 - MIO_BASE) / (1024 * 1024) << " MB [" << reinterpret_cast<void *>(MIO_BASE) << ":" << reinterpret_cast<void *>(MIO_TOP) << "]" << endl;
    kout << "  Node Id:      ";
    if (si->bm.node_id != -1)
        kout << si->bm.node_id << " (" << Traits<Build>::NODES << ")" << endl;
    else
        kout << "will get from the network!" << endl;
    kout << "  Position:     ";
    if (si->bm.space_x != -1)
        kout << "(" << si->bm.space_x << "," << si->bm.space_y << "," << si->bm.space_z << ")" << endl;
    else
        kout << "will get from the network!" << endl;
    if (si->bm.extras_offset != -1UL)
        kout << "  Extras:       " << si->lm.app_extra_size << " bytes" << endl;

    kout << endl;
}

void Setup::enable_paging()
{

    unsigned int PAGE_SIZE = 4 * 1024;
    unsigned int PT_ENTRIES = MMU::PT_ENTRIES;
    Phy_Addr bytes = 10UL * 1024 * 1024 * 1024;
    kout << "Total Bytes " << bytes;
    unsigned long pages = MMU::pages(bytes);

    kout << "Total Pages: " << pages << endl;

    unsigned total_pts = MMU::page_tables(pages);
    kout << "Total Page Tables: " << total_pts << endl;


    unsigned int PD_ENTRIES_LVL_2 = total_pts / PT_ENTRIES;
    unsigned int PD_ENTRIES_LVL_1 = PT_ENTRIES;
    unsigned int PT_ENTRIES_LVL_0 = PT_ENTRIES;

    Phy_Addr PD2_ADDR = PAGE_TABLES;
    Page_Directory * master = new ((void *)PD2_ADDR) Page_Directory();
    kout << "Master Base Address: " << PD2_ADDR << endl;
    PD2_ADDR += PAGE_SIZE;

    master->remap(PD2_ADDR, RV64_Flags::V, 0, PD_ENTRIES_LVL_2);

    Phy_Addr PD1_ADDR = PD2_ADDR + PT_ENTRIES * PAGE_SIZE;
    Phy_Addr PT0_ADDR = PD1_ADDR;

    for (unsigned long i = 0; i < PD_ENTRIES_LVL_2; i++)
    {
        Page_Directory *pd_lv1 = new ((void *)PD2_ADDR) Page_Directory();
        PD2_ADDR += PAGE_SIZE;

        pd_lv1->remap(PD1_ADDR, RV64_Flags::V, 0, PD_ENTRIES_LVL_1);
        PD1_ADDR += PD_ENTRIES_LVL_1 * PAGE_SIZE;
    }

    PD1_ADDR = 0;
    for (unsigned long i = 0; i < PD_ENTRIES_LVL_2; i++)
    {
        for (unsigned long j = 0; j < PD_ENTRIES_LVL_1; j++)
        {
            Page_Table *pt_lv0 = new ((void *)PT0_ADDR) Page_Table();
            PT0_ADDR += PAGE_SIZE;
            pt_lv0->remap(PD1_ADDR, RV64_Flags::SYS, 0, PT_ENTRIES_LVL_0);
            PD1_ADDR += PD_ENTRIES_LVL_1 * PAGE_SIZE;
        }
    }
    kout << "Page Directory LVL1 Address" << PD1_ADDR << endl;

    kout << "Page Directory End Address" << PD2_ADDR << endl;

    db<Setup>(INF) << "Set SATP" << endl;
    // Set SATP and enable paging
    CPU::satp((1UL << 63) | (reinterpret_cast<unsigned long>(master) >> 12));

    db<Setup>(INF) << "Flush TLB" << endl;
    // Flush TLB to ensure we've got the right memory organization
    MMU::flush_tlb();
}

void Setup::mmu_init()
{
    unsigned int PAGE_SIZE = 4 * 1024;
    // Get the physical address for the SYSTEM Page Table
    // PT_Entry * sys_pt = reinterpret_cast<PT_Entry *>(si->pmm.sys_pt);

    Page_Table * sys_pt = reinterpret_cast<Page_Table*>((void*)SYS_PT);

    // Clear the System Page Table
    // memset(sys_pt, 0, sizeof(Page));

    // System Info
    unsigned int from = MMU::page(SYS_INFO);
    sys_pt->remap(SYS_INFO, RV64_Flags::SYS, from, from+1);
    // sys_pt[MMU::page(SYS_INFO)] = MMU::phy2pte(si->pmm.sys_info, Flags::SYS);

    // Set an entry to this page table, so the system can access it later
    from = MMU::page(SYS_PT);
    sys_pt->remap(SYS_PT, RV64_Flags::SYS, from, from+1);
    // sys_pt[MMU::page(SYS_PT)] = MMU::phy2pte(si->pmm.sys_pt, Flags::SYS);

    // System Page Directory
    from = MMU::page(SYS_PD1);
    sys_pt->remap(SYS_PD1, RV64_Flags::SYS, from, from+1);
    // sys_pt[MMU::page(SYS_PD)] = MMU::phy2pte(si->pmm.sys_pd, Flags::SYS);
    //
    // unsigned int i;
    // PT_Entry aux;

    // SYSTEM code
    unsigned int sys_code_size = MMU::pages(SYS_INFO - SYS_CODE);
    sys_pt->remap(SYS_CODE, RV64_Flags::SYS, 0, sys_code_size);
    // for(i = 0, aux = si->pmm.sys_code; i < MMU::pages(si->lm.sys_code_size); i++, aux = aux + sizeof(Page))
    //     sys_pt[MMU::page(SYS_CODE) + i] = MMU::phy2pte(aux, Flags::SYS);

    // SYSTEM data
    unsigned int sys_data_size = MMU::pages(SYS_STACK - SYS_DATA);
    sys_pt->remap(SYS_DATA, RV64_Flags::SYS, 0, sys_data_size);
    // for(i = 0, aux = si->pmm.sys_data; i < MMU::pages(si->lm.sys_data_size); i++, aux = aux + sizeof(Page))
    //     sys_pt[MMU::page(SYS_DATA) + i] = MMU::phy2pte(aux, Flags::SYS);

    // SYSTEM stack (used only during init and for the ukernel model)
    unsigned int sys_stack_size = MMU::pages(SYS_HEAP - SYS_STACK);
    sys_pt->remap(SYS_STACK, RV64_Flags::SYS, 0, sys_stack_size);
    // for(i = 0, aux = si->pmm.sys_stack; i < MMU::pages(si->lm.sys_stack_size); i++, aux = aux + sizeof(Page))
    //     sys_pt[MMU::page(SYS_STACK) + i] = MMU::phy2pte(aux, Flags::SYS);


//APP
// Get the physical address for the first APPLICATION Page Tables
    // PT_Entry * app_code_pt = reinterpret_cast<PT_Entry *>(si->pmm.app_code_pts);
    // PT_Entry * app_data_pt = reinterpret_cast<PT_Entry *>(si->pmm.app_data_pts);

    Page_Table * app_code_pt = reinterpret_cast<Page_Table *>((void*)APP_CODE);
    Page_Table * app_data_pt = reinterpret_cast<Page_Table *>((void*)APP_DATA);

    unsigned long app_code_size = APP_DATA - APP_CODE;
    app_code_pt->remap(APP_CODE, RV64_Flags::APP, 0, MMU::pages(app_code_size));

    unsigned long app_data_size = APP_HIGH - APP_DATA;
    app_data_pt->remap(APP_DATA, RV64_Flags::APP, 0, MMU::pages(app_data_size));



    // // Clear the first APPLICATION Page Tables
    // memset(app_code_pt, 0, MMU::page_tables(MMU::pages(si->lm.app_code_size)) * sizeof(Page));
    // memset(app_data_pt, 0,  MMU::page_tables(MMU::pages(si->lm.app_data_size)) * sizeof(Page));

    // unsigned int i;
    // PT_Entry aux;
    //
    // // APPLICATION code
    // for(i = 0, aux = si->pmm.app_code; i < MMU::pages(si->lm.app_code_size); i++, aux = aux + sizeof(Page))
    //     app_code_pt[MMU::page(si->lm.app_code) + i] = MMU::phy2pte(aux, Flags::APP);
    //
    // // APPLICATION data (contains stack, heap and extra)
    // for(i = 0, aux = si->pmm.app_data; i < MMU::pages(si->lm.app_data_size); i++, aux = aux + sizeof(Page))
    //     app_data_pt[MMU::page(si->lm.app_data) + i] = MMU::phy2pte(aux, Flags::APP);

//SYS PD
// Get the physical address for the System Page Directory
    // PT_Entry * sys_pd = reinterpret_cast<PT_Entry *>(si->pmm.sys_pd);
    Page_Directory * sys_pd2 = reinterpret_cast<Page_Directory *>((void*)SYS_PD2);
    Page_Directory * sys_pd1 = reinterpret_cast<Page_Directory *>((void*)SYS_PD1);

    from = MMU::page(SYS_PD1);
    sys_pd2->remap(SYS_PD1, RV64_Flags::V, from, from+1);




    // Clear the System Page Directory
    // memset(sys_pd, 0, sizeof(Page));
    //
    // // Calculate the number of page tables needed to map the physical memory
    // unsigned int mem_size = MMU::pages(si->bm.mem_top - si->bm.mem_base);
    // int n_pts = MMU::page_tables(mem_size);

    unsigned int mem_size = MMU::pages(RAM_TOP+1 - RAM_BASE);
    unsigned int n_pts = MMU::page_tables(mem_size);
    // Map the whole physical memory into the page tables pointed by phy_mem_pts
    Page_Table * pts = reinterpret_cast<Page_Table *>((void*)PHY_MEM);
    pts->remap(RAM_BASE, RV64_Flags::SYS, 0, mem_size);

    //
    // for(unsigned int i = 0; i < mem_size; i++)
    //     pts[i] = MMU::phy2pte((si->bm.mem_base + i * sizeof(Page)), Flags::SYS);

    // Attach all physical memory starting at PHY_MEM
    // assert((MMU::directory_lvl_1(MMU::align_directory_lvl_1(PHY_MEM)) + n_pts) < (MMU::PD_ENTRIES - 4)); // check if it would overwrite the OS

    // for(unsigned int i = MMU::directory_lvl_1(MMU::align_directory_lvl_1(PHY_MEM)), j = 0; i < MMU::directory_lvl_1(MMU::align_directory_lvl_1(PHY_MEM)) + n_pts; i++, j++)
    //     sys_pd[i] = MMU::phy2pde(si->pmm.phy_mem_pts + j * sizeof(Page));

    from = MMU::directory_lvl_1(MMU::align_directory(PHY_MEM));
    sys_pd1->remap(PHY_MEM, RV64_Flags::V, from, from + n_pts);

    // Attach the portion of the physical memory used by Setup at SETUP
    // sys_pd[MMU::directory_lvl_1(SETUP)] =  MMU::phy2pde(si->pmm.phy_mem_pts); NOT IMPLEMENTED YET

    // Attach the portion of the physical memory used by int_m2s at MEM_TOP
    // sys_pd[MMU::directory_lvl_1(MEM_TOP)] =  MMU::phy2pde(si->pmm.phy_mem_pts + (n_pts - 1) * sizeof(Page));

    from = MMU::directory_lvl_1(RAM_TOP);
    sys_pd1->remap(PHY_MEM + (n_pts - 1) * PAGE_SIZE, RV64_Flags::V, from, from+1);

    // Attach all physical memory starting at MEM_BASE
    // assert((MMU::directory_lvl_1(MMU::align_directory_lvl_1(MEM_BASE)) + n_pts) < (MMU::PD_ENTRIES - 4)); // check if it would overwrite the OS
    // for(unsigned int i = MMU::directory_lvl_1(MMU::align_directory_lvl_1(MEM_BASE)), j = 0; i < MMU::directory(MMU::align_directory(MEM_BASE)) + n_pts; i++, j++)
    //     sys_pd[i] = MMU::phy2pde((si->pmm.phy_mem_pts + j * sizeof(Page)));

    from = MMU::directory_lvl_1(MMU::align_directory(RAM_BASE));
    sys_pd1->remap(PHY_MEM, RV64_Flags::V, from, from + n_pts);

    // Calculate the number of page tables needed to map the IO address space
    unsigned int io_size = MMU::pages(MIO_TOP - MIO_BASE);
    n_pts = MMU::page_tables(io_size);

    Page_Table * io_pts = reinterpret_cast<Page_Table *>((void*)MIO_BASE);
    io_pts->remap(MIO_BASE, RV64_Flags::MIO, 0, io_size);

    // Map IO address space into the page tables pointed by io_pts
    // pts = reinterpret_cast<PT_Entry *>(si->pmm.io_pts);
    // for(unsigned int i = 0; i < io_size; i++)
    //     pts[i] = MMU::phy2pte((si->bm.mio_base + i * sizeof(Page)), Flags::IO);



    // Attach devices' memory at Memory_Map::IO
    // assert((MMU::directory(MMU::align_directory(IO)) + n_pts) < (MMU::PD_ENTRIES - 3)); // check if it would overwrite the OS

    from = MMU::directory_lvl_1(MMU::align_directory(MIO_TOP));
    sys_pd1->remap(MIO_BASE, RV64_Flags::V, from, from + n_pts);

    // for(unsigned int i = MMU::directory(MMU::align_directory(IO)), j = 0; i < MMU::directory(MMU::align_directory(IO)) + n_pts; i++, j++)
    //     sys_pd[i] = MMU::phy2pde((si->pmm.io_pts + j * sizeof(Page)));

    from = MMU::directory_lvl_1(SYS);
    sys_pd1->remap(SYS_PT, RV64_Flags::V, from, from+1);

    // Attach the OS (i.e. sys_pt)
    // sys_pd[MMU::directory_lvl_1(SYS)] = MMU::phy2pde(si->pmm.sys_pt);

    n_pts = MMU::page_tables(MMU::pages(app_code_size));
    from = MMU::directory_lvl_1(MMU::align_directory(APP_CODE));
    sys_pd1->remap(APP_CODE, RV64_Flags::V, from, from + n_pts);
    // Attach the first APPLICATION CODE (i.e. app_code_pt)
    // n_pts = MMU::page_tables(MMU::pages(si->lm.app_code_size));
    // for(unsigned int i = MMU::directory_lvl_1(MMU::align_directory_lvl_1(si->lm.app_code)), j = 0; i < MMU::directory_lvl_1(MMU::align_directory_lvl_1(si->lm.app_code)) + n_pts; i++, j++)
    //     sys_pd[i] = MMU::phy2pde(si->pmm.app_code_pts + j * sizeof(Page));

    n_pts = MMU::page_tables(MMU::pages(app_data_size));
    from = MMU::directory_lvl_1(MMU::align_directory(APP_DATA));
    sys_pd1->remap(APP_DATA, RV64_Flags::V, from, from + n_pts);
    // Attach the first APPLICATION DATA (i.e. app_data_pt, containing heap, stack and extra)
    // n_pts = MMU::page_tables(MMU::pages(si->lm.app_data_size));
    // for(unsigned int i = MMU::directory_lvl_1(MMU::align_directory_lvl_1(si->lm.app_data)), j = 0; i < MMU::directory_lvl_1(MMU::align_directory_lvl_1(si->lm.app_data)) + n_pts; i++, j++)
    //     sys_pd[i] = MMU::phy2pde(si->pmm.app_data_pts + j * sizeof(Page));

    CPU::satp((1UL << 63) | (reinterpret_cast<unsigned long>(sys_pd2) >> 12));

    db<Setup>(INF) << "Flush TLB" << endl;
    // Flush TLB to ensure we've got the right memory organization
    MMU::flush_tlb();
}

void Setup::init_mmu()
{

    unsigned int PAGE_SIZE = 4 * 1024;
    unsigned int PT_ENTRIES = MMU::PT_ENTRIES;
    unsigned long pages = MMU::pages(RAM_TOP + 1);

    kout << "Total Pages: " << pages << endl;

    unsigned total_pts = MMU::page_tables(pages);
    kout << "Total Page Tables: " << total_pts << endl;


    unsigned int PD_ENTRIES_LVL_2 = total_pts / PT_ENTRIES;
    unsigned int PD_ENTRIES_LVL_1 = PT_ENTRIES;
    unsigned int PT_ENTRIES_LVL_0 = PT_ENTRIES;

    Phy_Addr PD2_ADDR = PAGE_TABLES;
    Page_Directory * master = new ((void *)PD2_ADDR) Page_Directory();
    kout << "Master Base Address: " << PD2_ADDR << endl;
    PD2_ADDR += PAGE_SIZE;

    master->remap(PD2_ADDR, RV64_Flags::V, 0, PD_ENTRIES_LVL_2);

    Phy_Addr PD1_ADDR = PD2_ADDR + PT_ENTRIES * PAGE_SIZE;
    Phy_Addr PT0_ADDR = PD1_ADDR;

    for (unsigned long i = 0; i < PD_ENTRIES_LVL_2; i++)
    {
        Page_Directory *pd_lv1 = new ((void *)PD2_ADDR) Page_Directory();
        PD2_ADDR += PAGE_SIZE;

        pd_lv1->remap(PD1_ADDR, RV64_Flags::V, 0, PD_ENTRIES_LVL_1);
        PD1_ADDR += PD_ENTRIES_LVL_1 * PAGE_SIZE;
    }

    PD1_ADDR = 0;
    for (unsigned long i = 0; i < PD_ENTRIES_LVL_2; i++)
    {
        for (unsigned long j = 0; j < PD_ENTRIES_LVL_1; j++)
        {
            Page_Table *pt_lv0 = new ((void *)PT0_ADDR) Page_Table();
            PT0_ADDR += PAGE_SIZE;
            pt_lv0->remap(PD1_ADDR, RV64_Flags::SYS, 0, PT_ENTRIES_LVL_0);
            PD1_ADDR += PD_ENTRIES_LVL_1 * PAGE_SIZE;
        }
    }
    kout << "Page Directory LVL1 Address" << PD1_ADDR << endl;

    kout << "Page Directory End Address" << PD2_ADDR << endl;

    db<Setup>(INF) << "Set SATP" << endl;
    // Set SATP and enable paging
    CPU::satp((1UL << 63) | (reinterpret_cast<unsigned long>(master) >> 12));

    db<Setup>(INF) << "Flush TLB" << endl;
    // Flush TLB to ensure we've got the right memory organization
    MMU::flush_tlb();
}

void Setup::call_next()
{
    db<Setup>(INF) << "SETUP almost ready!" << endl;

    CPU::sie(CPU::SSI | CPU::STI | CPU::SEI);
    CPU::sstatus(CPU::SPP_S);

    CPU::sepc(CPU::Reg(&_start));
    CLINT::stvec(CLINT::DIRECT, CPU::Reg(&_int_entry));

    CPU::sret();
    db<Setup>(ERR) << "OS failed to init!" << endl;
}

__END_SYS

using namespace EPOS::S;

void _entry() // machine mode
{
    // SiFive-U core 0 doesn't have MMU
    if (CPU::mhartid() == 0)
        CPU::halt();

    // ensure that sapt is 0
    CPU::satp(0);
    Machine::clear_bss();

    // need to check?
    // set the stack pointer, thus creating a stack for SETUP
    CPU::sp(Memory_Map::BOOT_STACK + Traits<Machine>::STACK_SIZE - sizeof(long));

    // Set up the Physical Memory Protection registers correctly
    // A = NAPOT, X, R, W
    CPU::pmpcfg0(0x1f);
    // All memory
    CPU::pmpaddr0((1UL << 55) - 1);

    // Delegate all traps to supervisor
    // Timer will not be delegated due to architecture reasons.
    CPU::mideleg(CPU::SSI | CPU::STI | CPU::SEI);
    CPU::medeleg(0xffff);

    CPU::mies(CPU::MSI | CPU::MTI | CPU::MEI);              // enable interrupts generation by CLINT
    CPU::mint_disable();                                    // (mstatus) disable interrupts (they will be reenabled at Init_End)
    CLINT::mtvec(CLINT::DIRECT, CPU::Reg(&_mmode_forward)); // setup a preliminary machine mode interrupt handler pointing it to _mmode_forward

    // MPP_S = change to supervirsor
    // MPIE = otherwise we won't ever receive interrupts
    CPU::mstatus(CPU::MPP_S | CPU::MPIE);
    CPU::mepc(CPU::Reg(&_setup)); // entry = _setup
    CPU::mret();                  // enter supervisor mode at setup (mepc) with interrupts enabled (mstatus.mpie = true)
}

void _setup() // supervisor mode
{
    kerr << endl;
    kout << endl;

    Setup setup;
}
