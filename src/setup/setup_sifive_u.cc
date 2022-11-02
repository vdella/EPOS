// EPOS SiFive-U (RISC-V) SETUP

#include <architecture.h>
#include <machine.h>
#include <utility/elf.h>
#include <utility/string.h>

using namespace EPOS::S;
typedef unsigned long Reg;

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

__BEGIN_SYS

extern OStream kout, kerr;

class Setup
{
private:
    // Physical memory map
    static const unsigned long RAM_BASE = Memory_Map::RAM_BASE;
    static const unsigned long RAM_TOP = Memory_Map::RAM_TOP;
    static const unsigned long MIO_BASE = Memory_Map::MIO_BASE;
    static const unsigned long MIO_TOP = Memory_Map::MIO_TOP;
    static const unsigned long FREE_BASE = Memory_Map::FREE_BASE;
    static const unsigned long FREE_TOP = Memory_Map::FREE_TOP;
    static const unsigned long SETUP = Memory_Map::SETUP;
    static const unsigned long BOOT_STACK = Memory_Map::BOOT_STACK;

    static const unsigned int PT_ENTRIES = MMU::PT_ENTRIES;
    static const unsigned int PD_ENTRIES = MMU::PD_ENTRIES;

    // Architecture Imports
    typedef CPU::Reg Reg;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;
    typedef MMU::Page_Directory Page_Directory;
    typedef MMU::Page_Table Page_Table;
    typedef MMU::RV64_Flags RV64Flags;

public:
    Setup();

private:
    void say_hi();
    void call_next();
    void init_mmu();

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

    init_mmu();

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
    kout << "  Memory:       " << (RAM_TOP + 1 - RAM_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(RAM_BASE) << ":" << reinterpret_cast<void *>(RAM_TOP) << "]" << endl;
    kout << "  User memory:  " << (FREE_TOP - FREE_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(FREE_BASE) << ":" << reinterpret_cast<void *>(FREE_TOP) << "]" << endl;
    kout << "  I/O space:    " << (MIO_TOP + 1 - MIO_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(MIO_BASE) << ":" << reinterpret_cast<void *>(MIO_TOP) << "]" << endl;
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

void Setup::init_mmu()
{
    kout << "Initializing MMU..." << endl;

    kout << "Creating master!" << endl;
    Page_Directory * _master = new ((void*) Memory_Map::PAGE_TABLES) Page_Directory();
    kout << "Created master!" << endl;

    // Total de entradas de pd2 + pd1
    unsigned pd_entradas = MMU::page_tables(MMU::pages(Traits<Machine>::RAM_TOP + 1 - Traits<Machine>::RAM_BASE));
    kout << "PD entradas: " << pd_entradas << endl;

    kout << "Assigning auxiliary variables." << endl;
    // unsigned int lvl2 = pd_entradas / PD_ENTRIES;
    unsigned int lvl1 = PD_ENTRIES;
    unsigned int PAGE = 4 * 1024;
    Phy_Addr addr = Memory_Map::PAGE_TABLES;

    kout << "Remapping master!" << endl;
    kout << "PAGE_TABLES = " << addr << endl;

    _master->remap(addr, RV64Flags::V, 0, 1);

    addr += PAGE;
    kout << addr << endl;
    kout << "Remapped master!" << endl;

    for (unsigned i = 0; i < 512; i++) 
    {
        Page_Directory * pd = new ((void*) addr) Page_Directory();
        _master[i] = *pd;
        (&(_master[i]))->remap(addr, RV64Flags::V);
        addr += PD_ENTRIES * PAGE;
    }

    kout << "Assigned lvl2 - first iterative loop!" << endl;

    kout << "Addr = " << addr << endl;
    kout << "lvl1 = " << lvl1 << endl;

    for (unsigned i = 0; i < 512; i++)
    {
        kout << "i = " << i << endl;
        for (unsigned j = 0; j < lvl1 * lvl1; j++)
        {
            kout << "j = " << j << endl;

            kout << "Getting page tables!" << endl;
            Page_Table * pt = new ((void*) addr) Page_Table();

            kout << "Remapping" << endl;
            kout << "Addr = " << addr << endl;
            pt->remap(addr, RV64Flags::SYS);
            kout << "Addr = " << addr << endl;
            
            kout << "Referencing" << endl;
            (&(_master[i]))[j] = *pt;

            kout << "Summing address" << endl;
            addr += PT_ENTRIES * PAGE;
            kout << "Addr = " << addr << endl;
        }
    }

    kout << "Almost flushing!" << endl;

    MMU::flush_tlb();
    kout << "Chegamos no final" << endl;
    kout << endl;
}

void Setup::call_next()
{
    db<Setup>(INF) << "SETUP ends here!" << endl;

    // Call the next stage
    CPU::satp((1UL << 63) | (Memory_Map::PAGE_TABLES >> 12));
    kout << "Passou SATP" << endl;
    CPU::sstatus(CPU::SPP_S);
    kout << "Passou SSTATUS" << endl;
    CPU::sepc(CPU::Reg(&_start));
    kout << "Passou SEPC" << endl;
    CPU::sret();

    // SETUP is now part of the free memory and this point should never be reached, but, just in case ... :-)
    db<Setup>(ERR) << "OS failed to init!" << endl;
}

__END_SYS

using namespace EPOS::S;

void _entry() // machine mode
{
    // Desabilita as interrupções
    CPU::mies(CPU::MSI | CPU::MTI | CPU::MEI);
    CPU::int_disable();

    if (CPU::mhartid() == 0) // SiFive-U requires 2 cores, so we disable core 1 here
        CPU::halt();

    // Guarantee that paging is off before going to S-mode.
    CPU::satp(0);
    Machine::clear_bss();

    // Rever
    CPU::sp(Memory_Map::RAM_TOP + 1 - sizeof(long));

    CPU::pmpcfg0(0x1f);
    CPU::pmpaddr0((1UL << 55) - 1);

    // // Salva o hartid para poder usar em s-mode
    // Reg core = CPU::mhartid();
    // CPU::tp(core);

    // Delegar as instrucoes e as excecoes
    CPU::mideleg(CPU::SSI | CPU::STI | CPU::SEI);
    CPU::medeleg(0xffff);

    // Escreve as interrupcoes que vao para s-mode e o handler
    CLINT::mtvec(CLINT::DIRECT, CPU::Reg(&_mmode_forward));

    // Coloca o _setup no pc e entra no modo supervisor
    CPU::mstatus(CPU::MPP_S | CPU::MPIE);
    CPU::mepc((CPU::Reg)&_setup);
    CPU::mret();
}

void _setup() // supervisor mode
{
    kerr << endl;
    kout << endl;

    CPU::sie(CPU::SSI | CPU::STI | CPU::SEI);
    CPU::sstatus(CPU::SPP_S);
    CLINT::stvec(CLINT::DIRECT, CPU::Reg(&_int_entry));

    Setup setup;
}
