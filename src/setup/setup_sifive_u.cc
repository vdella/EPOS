// EPOS SiFive-U (RISC-V) SETUP

// If multitasking is enabled, configure the machine in supervisor mode and activate paging. Otherwise, keep the machine in machine mode.

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
    void _int_m2s() __attribute((naked, aligned(4)));

    // SETUP entry point is in .init (and not in .text), so it will be linked first and will be the first function after the ELF header in the image
    void _entry() __attribute__((used, naked, section(".init")));
    void _setup();
    // void _print(const char * s) { Display::puts(s); }
    void _panic();

    // LD eliminates this variable while performing garbage collection, that's why the used attribute.
    char __boot_time_system_info[sizeof(EPOS::S::System_Info)] __attribute__((used)) = "<System_Info placeholder>"; // actual System_Info will be added by mkbi!
}

__BEGIN_SYS
char *bi;
extern OStream kout, kerr;

class Setup
{
private:
    // System Traits
    static const bool multitask = Traits<System>::multitask;

    // Physical memory map
    static const unsigned long RAM_BASE         = Memory_Map::RAM_BASE;
    static const unsigned long RAM_TOP          = Memory_Map::RAM_TOP;
    static const unsigned long MIO_BASE         = Memory_Map::MIO_BASE;
    static const unsigned long MIO_TOP          = Memory_Map::MIO_TOP;
    static const unsigned long FREE_BASE        = Memory_Map::FREE_BASE; // only used in LIBRARY mode
    static const unsigned long FREE_TOP         = Memory_Map::FREE_TOP;  // only used in LIBRARY mode
    static const unsigned long IMAGE            = Memory_Map::IMAGE;
    static const unsigned long SETUP            = Memory_Map::SETUP;
    static const unsigned long BOOT_STACK       = Memory_Map::BOOT_STACK;
    static const unsigned long INT_M2S          = Memory_Map::INT_M2S;

    // Logical memory map
    static const unsigned long APP_LOW          = Memory_Map::APP_LOW;
    static const unsigned long APP_HIGH         = Memory_Map::APP_HIGH;
    static const unsigned long PHY_MEM          = Memory_Map::PHY_MEM;
    static const unsigned long IO               = Memory_Map::IO;
    static const unsigned long SYS              = Memory_Map::SYS;
    static const unsigned long SYS_INFO         = Memory_Map::SYS_INFO;
    static const unsigned long SYS_PT           = Memory_Map::SYS_PT;
    static const unsigned long SYS_PD           = Memory_Map::SYS_PD;
    static const unsigned long SYS_CODE         = Memory_Map::SYS_CODE;
    static const unsigned long SYS_DATA         = Memory_Map::SYS_DATA;
    static const unsigned long SYS_STACK        = Memory_Map::SYS_STACK;
    static const unsigned long SYS_HEAP         = Memory_Map::SYS_HEAP;
    static const unsigned long SYS_HIGH         = Memory_Map::SYS_HIGH;
    static const unsigned long APP_CODE         = Memory_Map::APP_CODE;
    static const unsigned long APP_DATA         = Memory_Map::APP_DATA;

    // Architecture Imports
    typedef CPU::Reg Reg;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;
    typedef MMU::Page Page;
    typedef MMU::Page_Flags Flags;
    typedef MMU::Page_Table Page_Table;
    typedef MMU::Page_Directory Page_Directory;
    typedef MMU::PT_Entry PT_Entry;
    typedef MMU::PD_Entry PD_Entry;

public:
    Setup();

private:
    void build_lm();
    void build_pmm();

    void say_hi();

    void setup_m2s();
    void setup_sys_pt();
    void setup_app_pt();
    void setup_sys_pd();
    void enable_paging();

    void load_parts();
    void adjust_perms();
    void call_next();

private:
    char * bi;
    System_Info * si;

    static volatile bool paging_ready;
};

volatile bool Setup::paging_ready = false;

Setup::Setup()
{
    Display::init();
    kout << endl;
    kerr << endl;

    bi = reinterpret_cast<char *>(IMAGE);
    si = reinterpret_cast<System_Info *>(&__boot_time_system_info);
    if(si->bm.n_cpus > Traits<Machine>::CPUS)
        si->bm.n_cpus = Traits<Machine>::CPUS;

    db<Setup>(TRC) << "Setup(bi=" << reinterpret_cast<void *>(bi) << ",sp=" << CPU::sp() << ")" << endl;
    db<Setup>(INF) << "Setup:si=" << *si << endl;

    if(multitask) {

        // Build the memory model
        build_lm();
        build_pmm();

        // Print basic facts about this EPOS instance
        if(!si->lm.has_app)
            db<Setup>(ERR) << "No APPLICATION in boot image, you don't need EPOS!" << endl;
        if(!si->lm.has_sys)
            db<Setup>(INF) << "No SYSTEM in boot image, assuming EPOS is a library!" << endl;
        say_hi();

        // Configure the memory model defined above
        setup_sys_pt();
        setup_app_pt();
        setup_sys_pd();

        // Relocate the machine to supervisor interrupt forwarder
        setup_m2s();

        // Enable paging
        enable_paging();

        // Load EPOS parts (e.g. INIT, SYSTEM, APPLICATION)
        load_parts();

        // Adjust APPLICATION permissions
        // FIXME: ld is putting the data segments (.data, .sdata, .bss, etc) inside the code segment even if we specify --nmagic, so, for a while, we can't fine tune perms.
        // adjust_perms();

    } else { // library mode

        // Print basic facts about this EPOS instance
        say_hi();

    }

    // build page tables
    init_mmu();

    build_lm();

    load_parts();

    // enable_paging();

    // SETUP ends here, so let's transfer control to the next stage (INIT or APP)
    call_next();
}


void Setup::build_lm()
{
    db<Setup>(TRC) << "Setup::build_lm()" << endl;

    // Get boot image structure
    si->lm.has_stp = (si->bm.setup_offset != -1u);
    si->lm.has_ini = (si->bm.init_offset != -1u);
    si->lm.has_sys = (si->bm.system_offset != -1u);
    si->lm.has_app = (si->bm.application_offset != -1u);
    si->lm.has_ext = (si->bm.extras_offset != -1u);

    // Check SETUP integrity and get the size of its segments
    si->lm.stp_entry = 0;
    si->lm.stp_segments = 0;
    si->lm.stp_code = ~0U;
    si->lm.stp_code_size = 0;
    si->lm.stp_data = ~0U;
    si->lm.stp_data_size = 0;
    if(si->lm.has_stp) {
        ELF * stp_elf = reinterpret_cast<ELF *>(&bi[si->bm.setup_offset]);
        if(!stp_elf->valid())
            db<Setup>(ERR) << "SETUP ELF image is corrupted!" << endl;
        si->lm.stp_entry = stp_elf->entry();
        int i = 0;
        for(; (i < stp_elf->segments()) && (stp_elf->segment_type(i) != PT_LOAD); i++);
        si->lm.stp_code = stp_elf->segment_address(i);
        si->lm.stp_code_size = stp_elf->segment_size(i);
        si->lm.stp_segments = 1;
        for(i++; i < stp_elf->segments(); i++) {
            if(stp_elf->segment_type(i) != PT_LOAD)
                continue;
            if(stp_elf->segment_address(i) < si->lm.stp_data)
                si->lm.stp_data = stp_elf->segment_address(i);
            si->lm.stp_data_size += stp_elf->segment_size(i);
            si->lm.stp_segments++;
        }
    }

    // Check INIT integrity and get the size of its segments
    si->lm.ini_entry = 0;
    si->lm.ini_segments = 0;
    si->lm.ini_code = ~0U;
    si->lm.ini_code_size = 0;
    si->lm.ini_data = ~0U;
    si->lm.ini_data_size = 0;
    if(si->lm.has_ini) {
        ELF * ini_elf = reinterpret_cast<ELF *>(&bi[si->bm.init_offset]);
        if(!ini_elf->valid())
            db<Setup>(ERR) << "INIT ELF image is corrupted!" << endl;
        si->lm.ini_entry = ini_elf->entry();
        int i = 0;
        for(; (i < ini_elf->segments()) && (ini_elf->segment_type(i) != PT_LOAD); i++);
        si->lm.ini_code = ini_elf->segment_address(i);
        si->lm.ini_code_size = ini_elf->segment_size(i);
        si->lm.ini_segments = 1;
        for(i++; i < ini_elf->segments(); i++) {
            if(ini_elf->segment_type(i) != PT_LOAD)
                continue;
            if(ini_elf->segment_address(i) < si->lm.ini_data)
                si->lm.ini_data = ini_elf->segment_address(i);
            si->lm.ini_data_size += ini_elf->segment_size(i);
            si->lm.ini_segments++;
        }
    }

    // Check SYSTEM integrity and get the size of its segments
    si->lm.sys_entry = 0;
    si->lm.sys_segments = 0;
    si->lm.sys_code = 0;
    si->lm.sys_code_size = 0;
    si->lm.sys_data = ~0U;
    si->lm.sys_data_size = 0;
    si->lm.sys_stack = SYS_STACK;
    si->lm.sys_stack_size = Traits<System>::STACK_SIZE;
    if(si->lm.has_sys) {
        ELF * sys_elf = reinterpret_cast<ELF *>(&bi[si->bm.system_offset]);
        if(!sys_elf->valid())
            db<Setup>(ERR) << "OS ELF image is corrupted!" << endl;
        si->lm.sys_entry = sys_elf->entry();
        for(int i = 0; i < sys_elf->segments(); i++) {
            if((sys_elf->segment_size(i) == 0) || (sys_elf->segment_type(i) != PT_LOAD))
                continue;
            if((sys_elf->segment_address(i) < SYS) || (sys_elf->segment_address(i) > SYS_HIGH)) {
                db<Setup>(WRN) << "Ignoring OS ELF segment " << i << " at " << hex << sys_elf->segment_address(i) << "!"<< endl;
                continue;
            }
            if(sys_elf->segment_address(i) < SYS_DATA) { // CODE
                if(si->lm.sys_code_size == 0) {
                    si->lm.sys_code_size = sys_elf->segment_size(i);
                    si->lm.sys_code = sys_elf->segment_address(i);
                } else if(sys_elf->segment_address(i) < si->lm.sys_code) {
                    si->lm.sys_code_size = si->lm.sys_code - sys_elf->segment_address(i) + sys_elf->segment_size(i);
                    si->lm.sys_code = sys_elf->segment_address(i);
                } else if(sys_elf->segment_address(i) > (si->lm.sys_code + si->lm.sys_code_size)) {
                    si->lm.sys_code_size = sys_elf->segment_address(i) - si->lm.sys_code;
                } else
                    si->lm.sys_code_size += sys_elf->segment_size(i);
            } else { // DATA
                if(sys_elf->segment_address(i) < si->lm.sys_data)
                    si->lm.sys_data = sys_elf->segment_address(i);
                si->lm.sys_data_size += sys_elf->segment_size(i);
            }
            si->lm.sys_segments++;
        }
        if(si->lm.sys_code != MMU::align_directory(si->lm.sys_code))
            db<Setup>(ERR) << "Unaligned OS code segment:" << hex << si->lm.sys_code << endl;
        if(si->lm.sys_data == ~0U) {
            db<Setup>(WRN) << "OS ELF image has no data segment!" << endl;
            si->lm.sys_data = MMU::align_page(APP_DATA);
        }
        if(si->lm.sys_code != SYS_CODE)
            db<Setup>(ERR) << "OS code segment address (" << reinterpret_cast<void *>(si->lm.sys_code) << ") does not match the machine's memory map (" << reinterpret_cast<void *>(SYS_CODE) << ")!" << endl;
        if(si->lm.sys_code + si->lm.sys_code_size > si->lm.sys_data)
            db<Setup>(ERR) << "OS code segment is too large!" << endl;
        if(si->lm.sys_data != SYS_DATA)
            db<Setup>(ERR) << "OS data segment address (" << reinterpret_cast<void *>(si->lm.sys_data) << ") does not match the machine's memory map (" << reinterpret_cast<void *>(SYS_DATA) << ")!" << endl;
        if(si->lm.sys_data + si->lm.sys_data_size > si->lm.sys_stack)
            db<Setup>(ERR) << "OS data segment is too large!" << endl;
        if(MMU::page_tables(MMU::pages(si->lm.sys_stack_size)) > 1)
            db<Setup>(ERR) << "OS stack segment is too large!" << endl;
    }

    // Check APPLICATION integrity and get the size of its segments
    si->lm.app_entry = 0;
    si->lm.app_segments = 0;
    si->lm.app_code = ~0U;
    si->lm.app_code_size = 0;
    si->lm.app_data = ~0U;
    si->lm.app_data_size = 0;
    si->lm.app_extra = ~0U;
    si->lm.app_extra_size = 0;
    if(si->lm.has_app) {
        ELF * app_elf = reinterpret_cast<ELF *>(&bi[si->bm.application_offset]);
        if(!app_elf->valid())
            db<Setup>(ERR) << "APP ELF image is corrupted!" << endl;
        si->lm.app_entry = app_elf->entry();
        for(int i = 0; i < app_elf->segments(); i++) {
            if((app_elf->segment_size(i) == 0) || (app_elf->segment_type(i) != PT_LOAD))
                continue;
            if((app_elf->segment_address(i) < APP_LOW) || (app_elf->segment_address(i) > APP_HIGH)) {
                db<Setup>(WRN) << "Ignoring APP ELF segment " << i << " at " << hex << app_elf->segment_address(i) << "!"<< endl;
                continue;
            }
            if(app_elf->segment_address(i) < APP_DATA) { // CODE
                if(si->lm.app_code_size == 0) {
                    si->lm.app_code_size = app_elf->segment_size(i);
                    si->lm.app_code = app_elf->segment_address(i);
                } else if(app_elf->segment_address(i) < si->lm.app_code) {
                    si->lm.app_code_size = si->lm.app_code - app_elf->segment_address(i) + app_elf->segment_size(i);
                    si->lm.app_code = app_elf->segment_address(i);
                } else if(app_elf->segment_address(i) > (si->lm.app_code + si->lm.app_code_size)) {
                    si->lm.app_code_size = app_elf->segment_address(i) - si->lm.app_code;
                } else
                    si->lm.app_code_size += app_elf->segment_size(i);
            } else { // DATA
                if(app_elf->segment_address(i) < si->lm.app_data)
                    si->lm.app_data = app_elf->segment_address(i);
                si->lm.app_data_size += app_elf->segment_size(i);
            }
            si->lm.app_segments++;
        }
        if(si->lm.app_code != MMU::align_directory(si->lm.app_code))
            db<Setup>(ERR) << "Unaligned APP code segment:" << hex << si->lm.app_code << endl;
        if(si->lm.app_data == ~0U) {
            db<Setup>(WRN) << "APP ELF image has no data segment!" << endl;
            si->lm.app_data = MMU::align_page(APP_DATA);
        }
        if(Traits<System>::multiheap) { // Application heap in data segment
            si->lm.app_data_size = MMU::align_page(si->lm.app_data_size);
            si->lm.app_stack = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_data_size += MMU::align_page(Traits<Application>::STACK_SIZE);
            si->lm.app_heap = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_data_size += MMU::align_page(Traits<Application>::HEAP_SIZE);
        }
        if(si->lm.has_ext) { // Check for EXTRA data in the boot image
            si->lm.app_extra = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_extra_size = si->bm.img_size - si->bm.extras_offset;
            if(Traits<System>::multiheap)
                si->lm.app_extra_size = MMU::align_page(si->lm.app_extra_size);
            si->lm.app_data_size += si->lm.app_extra_size;
        }
    }
}


void Setup::build_pmm()
{
    // Allocate (reserve) memory for all entities we have to setup.
    // We'll start at the highest address to make possible a memory model
    // on which the application's logical and physical address spaces match.

    Phy_Addr top_page = MMU::pages(BOOT_STACK);

    db<Setup>(TRC) << "Setup::build_pmm() [top=" << top_page << "]" << endl;

    // Machine to Supervisor code (1 x sizeof(Page), not listed in the PMM)
    top_page -= 1;

    // System Page Directory (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_pd = top_page * sizeof(Page);

    // System Page Table (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_pt = top_page * sizeof(Page);

    // Page tables to map the whole physical memory
    // = NP/NPTE_PT * sizeof(Page)
    //   NP = size of physical memory in pages
    //   NPTE_PT = number of page table entries per page table
    top_page -= MMU::page_tables(MMU::pages(si->bm.mem_top - si->bm.mem_base));
    si->pmm.phy_mem_pts = top_page * sizeof(Page);

    // Page tables to map the IO address space
    // = NP/NPTE_PT * sizeof(Page)
    //   NP = size of I/O address space in pages
    //   NPTE_PT = number of page table entries per page table
    top_page -= MMU::page_tables(MMU::pages(si->bm.mio_top - si->bm.mio_base));
    si->pmm.io_pts = top_page * sizeof(Page);

    // Page tables to map the first APPLICATION code segment
    top_page -= MMU::page_tables(MMU::pages(si->lm.app_code_size));
    si->pmm.app_code_pts = top_page * sizeof(Page);

    // Page tables to map the first APPLICATION data segment (which contains heap, stack and extra)
    top_page -= MMU::page_tables(MMU::pages(si->lm.app_data_size));
    si->pmm.app_data_pts = top_page * sizeof(Page);

    // System Info (1 x sizeof(Page))
    if(SYS_INFO != Traits<Machine>::NOT_USED)
        top_page -= 1;
    si->pmm.sys_info = top_page * sizeof(Page);

    // SYSTEM code segment
    top_page -= MMU::pages(si->lm.sys_code_size);
    si->pmm.sys_code = top_page * sizeof(Page);

    // SYSTEM data segment
    top_page -= MMU::pages(si->lm.sys_data_size);
    si->pmm.sys_data = top_page * sizeof(Page);

    // SYSTEM stack segment
    top_page -= MMU::pages(si->lm.sys_stack_size);
    si->pmm.sys_stack = top_page * sizeof(Page);

    // The memory allocated so far will "disappear" from the system as we set usr_mem_top as follows:
    si->pmm.usr_mem_base = si->bm.mem_base;
    si->pmm.usr_mem_top = top_page * sizeof(Page);

    // APPLICATION code segment
    top_page -= MMU::pages(si->lm.app_code_size);
    si->pmm.app_code = top_page * sizeof(Page);

    // APPLICATION data segment (contains stack, heap and extra)
    top_page -= MMU::pages(si->lm.app_data_size);
    si->pmm.app_data = top_page * sizeof(Page);

    // Free chunks (passed to MMU::init)
    si->pmm.free1_base = si->bm.mem_base;
    si->pmm.free1_top = top_page * sizeof(Page);

    // Test if we didn't overlap SETUP and the boot image
    if(si->pmm.usr_mem_top <= si->lm.stp_code + si->lm.stp_code_size + si->lm.stp_data_size)
        db<Setup>(ERR) << "SETUP would have been overwritten!" << endl;
}


void Setup::say_hi()
{
    db<Setup>(TRC) << "Setup::say_hi()" << endl;
    db<Setup>(INF) << "System_Info=" << *si << endl;

    kout << "This is EPOS!\n" << endl;
    kout << "Setting up this machine as follows: " << endl;
    kout << "  Mode:         " << ((Traits<Build>::MODE == Traits<Build>::LIBRARY) ? "library" : (Traits<Build>::MODE == Traits<Build>::BUILTIN) ? "built-in"
                                                                                                                                                 : "kernel")
         << endl;
    kout << "  Processor:    " << Traits<Machine>::CPUS << " x RV" << Traits<CPU>::WORD_SIZE << " at " << Traits<CPU>::CLOCK / 1000000 << " MHz (BUS clock = " << Traits<CPU>::CLOCK / 1000000 << " MHz)" << endl;
    kout << "  Machine:      SiFive-U" << endl;
#ifdef __library__
    kout << "  Memory:       " << (RAM_TOP + 1 - RAM_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(RAM_BASE) << ":" << reinterpret_cast<void *>(RAM_TOP) << "]" << endl;
    kout << "  User memory:  " << (FREE_TOP - FREE_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(FREE_BASE) << ":" << reinterpret_cast<void *>(FREE_TOP) << "]" << endl;
    kout << "  I/O space:    " << (MIO_TOP + 1 - MIO_BASE) / 1024 << " KB [" << reinterpret_cast<void *>(MIO_BASE) << ":" << reinterpret_cast<void *>(MIO_TOP) << "]" << endl;
#else
    kout << "  Memory:       " << (si->bm.mem_top - si->bm.mem_base) / 1024 << " KB [" << reinterpret_cast<void *>(si->bm.mem_base) << ":" << reinterpret_cast<void *>(si->bm.mem_top) << "]" << endl;
    kout << "  User memory:  " << (si->pmm.usr_mem_top - si->pmm.usr_mem_base) / 1024 << " KB [" << reinterpret_cast<void *>(si->pmm.usr_mem_base) << ":" << reinterpret_cast<void *>(si->pmm.usr_mem_top) << "]" << endl;
    kout << "  I/O space:    " << (si->bm.mio_top - si->bm.mio_base) / 1024 << " KB [" << reinterpret_cast<void *>(si->bm.mio_base) << ":" << reinterpret_cast<void *>(si->bm.mio_top) << "]" << endl;
#endif
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
    if(si->lm.has_stp)
        kout << "  Setup:        " << si->lm.stp_code_size + si->lm.stp_data_size << " bytes" << endl;
    if(si->lm.has_ini)
        kout << "  Init:         " << si->lm.ini_code_size + si->lm.ini_data_size << " bytes" << endl;
    if(si->lm.has_sys)
        kout << "  OS code:      " << si->lm.sys_code_size << " bytes" << "\tdata: " << si->lm.sys_data_size << " bytes" << "   stack: " << si->lm.sys_stack_size << " bytes" << endl;
    if(si->lm.has_app)
        kout << "  APP code:     " << si->lm.app_code_size << " bytes" << "\tdata: " << si->lm.app_data_size << " bytes" << endl;
    if(si->lm.has_ext)
        kout << "  Extras:       " << si->lm.app_extra_size << " bytes" << endl;

    kout << endl;
}

void Setup::load_parts()
{
    db<Setup>(TRC) << "load_parts" << endl;

    // Relocate System_Info
    if (sizeof(System_Info) > PAGE_SIZE)
    {
        db<Setup>(ERR) << "System_Info is bigger than a page (" << sizeof(System_Info) << ")!" << endl;
        _panic();
    }
    memcpy(reinterpret_cast<void *>(SYS_INFO), si, sizeof(System_Info));

    ELF *ini_elf = reinterpret_cast<ELF *>(&bi[si->bm.init_offset]);
    ELF *sys_elf = reinterpret_cast<ELF *>(&bi[si->bm.system_offset]);

    // Load INIT
    if (si->lm.has_ini)
    {
        db<Setup>(TRC) << "Setup_SifiveU::load_init()" << endl;
        if (ini_elf->load_segment(0) < 0)
        {
            db<Setup>(ERR) << "INIT code segment was corrupted during SETUP!" << endl;
            _panic();
        }

        for (int i = 1; i < ini_elf->segments(); i++)
            if (ini_elf->load_segment(i) < 0)
            {
                db<Setup>(ERR) << "INIT data segment was corrupted during SETUP!" << endl;
                _panic();
            }
    }
    db<Setup>(TRC) << "init has " << hex << sys_elf->segment_address(0) - ini_elf->segment_address(0) - ini_elf->segment_size(0) << " unused bytes of memory" << endl;

    // Load SYSTEM
    if (si->lm.has_sys)
    {
        db<Setup>(TRC) << "Setup_SifiveU::load_system()" << endl;
        if (sys_elf->load_segment(0) < 0)
        {
            db<Setup>(ERR) << "system code segment was corrupted during SETUP!" << endl;
            _panic();
        }
        for (int i = 1; i < sys_elf->segments(); i++)
            if (sys_elf->load_segment(i) < 0)
            {
                db<Setup>(ERR) << "system data segment was corrupted during SETUP!" << endl;
                _panic();
            }
    }
    db<Setup>(TRC) << "sys code has " << hex << sys_elf->segment_address(1) - sys_elf->segment_address(0) - sys_elf->segment_size(0) << " unused bytes of memory" << endl;
    db<Setup>(TRC) << "sys data has " << hex << 0x00100000 - sys_elf->segment_size(1) << " unused bytes of memory" << endl;
}

void Setup::build_lm()
{
    db<Setup>(TRC) << "Setup::build_lm()" << endl;

    // Get boot image structure
    si->lm.has_stp = (si->bm.setup_offset != -1u);
    si->lm.has_ini = (si->bm.init_offset != -1u);
    si->lm.has_sys = (si->bm.system_offset != -1u);
    si->lm.has_app = (si->bm.application_offset != -1u);
    si->lm.has_ext = (si->bm.extras_offset != -1u);

    // Check SETUP integrity and get the size of its segments
    si->lm.stp_entry = 0;
    si->lm.stp_segments = 0;
    si->lm.stp_code = ~0U;
    si->lm.stp_code_size = 0;
    si->lm.stp_data = ~0U;
    si->lm.stp_data_size = 0;
    if(si->lm.has_stp) {
        ELF * stp_elf = reinterpret_cast<ELF *>(&bi[si->bm.setup_offset]);
        if(!stp_elf->valid())
            db<Setup>(ERR) << "SETUP ELF image is corrupted!" << endl;
        si->lm.stp_entry = stp_elf->entry();
        int i = 0;
        for(; (i < stp_elf->segments()) && (stp_elf->segment_type(i) != PT_LOAD); i++);
        si->lm.stp_code = stp_elf->segment_address(i);
        si->lm.stp_code_size = stp_elf->segment_size(i);
        si->lm.stp_segments = 1;
        for(i++; i < stp_elf->segments(); i++) {
            if(stp_elf->segment_type(i) != PT_LOAD)
                continue;
            if(stp_elf->segment_address(i) < si->lm.stp_data)
                si->lm.stp_data = stp_elf->segment_address(i);
            si->lm.stp_data_size += stp_elf->segment_size(i);
            si->lm.stp_segments++;
        }
    }

    db<Setup>(WRN) << "Setup Offset! " << si->bm.setup_offset << endl;
    db<Setup>(WRN) << "Init Offset! " << si->bm.init_offset << endl;
    db<Setup>(WRN) << "System Offset! " << si->bm.system_offset << endl;
    db<Setup>(WRN) << "Application Offset! " << si->bm.application_offset << endl;



    // Check INIT integrity and get the size of its segments
    si->lm.ini_entry = 0;
    si->lm.ini_segments = 0;
    si->lm.ini_code = ~0U;
    si->lm.ini_code_size = 0;
    si->lm.ini_data = ~0U;
    si->lm.ini_data_size = 0;
    if(si->lm.has_ini) {
        ELF * ini_elf = reinterpret_cast<ELF *>(&bi[si->bm.init_offset]);
        if(!ini_elf->valid())
            db<Setup>(ERR) << "INIT ELF image is corrupted!" << endl;
        si->lm.ini_entry = ini_elf->entry();
        int i = 0;
        for(; (i < ini_elf->segments()) && (ini_elf->segment_type(i) != PT_LOAD); i++);
        si->lm.ini_code = ini_elf->segment_address(i);
        si->lm.ini_code_size = ini_elf->segment_size(i);
        si->lm.ini_segments = 1;
        for(i++; i < ini_elf->segments(); i++) {
            if(ini_elf->segment_type(i) != PT_LOAD)
                continue;
            if(ini_elf->segment_address(i) < si->lm.ini_data)
                si->lm.ini_data = ini_elf->segment_address(i);
            si->lm.ini_data_size += ini_elf->segment_size(i);
            si->lm.ini_segments++;
        }
    }

    // Check SYSTEM integrity and get the size of its segments
    si->lm.sys_entry = 0;
    si->lm.sys_segments = 0;
    si->lm.sys_code = 0;
    si->lm.sys_code_size = 0;
    si->lm.sys_data = ~0U;
    si->lm.sys_data_size = 0;
    si->lm.sys_stack = SYS_STACK;
    si->lm.sys_stack_size = Traits<System>::STACK_SIZE;
    if(si->lm.has_sys) {
        ELF * sys_elf = reinterpret_cast<ELF *>(&bi[si->bm.system_offset]);
        for (int i = 0; i < sys_elf->segments(); i++) {
          // db<Setup>(WRN) << "Segment!" << sys_elf->segment_size(i) << endl;
        }
        if(!sys_elf->valid())
            db<Setup>(ERR) << "OS ELF image is corrupted!" << endl;
        si->lm.sys_entry = sys_elf->entry();
        for(int i = 0; i < sys_elf->segments(); i++) {
            // db<Setup>(WRN) << "Index!" << i << endl;
            if((sys_elf->segment_size(i) == 0) || (sys_elf->segment_type(i) != PT_LOAD)) {
                db<Setup>(WRN) << "OS ELF image has an invalid segment!" << sys_elf->segment_size(i) << endl;
                continue;
            }
            if((sys_elf->segment_address(i) < SYS) || (sys_elf->segment_address(i) > SYS_HIGH)) {
                db<Setup>(WRN) << "Ignoring OS ELF segment " << i << " at " << hex << sys_elf->segment_address(i) << "!"<< endl;
                continue;
            }
            if(sys_elf->segment_address(i) < SYS_DATA) { // CODE
                if(si->lm.sys_code_size == 0) {
                    si->lm.sys_code_size = sys_elf->segment_size(i);
                    si->lm.sys_code = sys_elf->segment_address(i);
                } else if(sys_elf->segment_address(i) < si->lm.sys_code) {
                    si->lm.sys_code_size = si->lm.sys_code - sys_elf->segment_address(i) + sys_elf->segment_size(i);
                    si->lm.sys_code = sys_elf->segment_address(i);
                } else if(sys_elf->segment_address(i) > (si->lm.sys_code + si->lm.sys_code_size)) {
                    si->lm.sys_code_size = sys_elf->segment_address(i) - si->lm.sys_code;
                } else
                    si->lm.sys_code_size += sys_elf->segment_size(i);
            } else { // DATA
                // db<Setup>(WRN) << "Segment Address!" << sys_elf->segment_address(i) << endl;
                // db<Setup>(WRN) << "SYS DATA!" << si->lm.sys_data << endl;
                // assert((si->lm.sys_data - sys_elf->segment_address(i)) > 0U);
                if((si->lm.sys_data - sys_elf->segment_address(i)) > 0U) {
                  si->lm.sys_data = sys_elf->segment_address(i);
                  // db<Setup>(WRN) << "Entrou!" << si->lm.sys_data << endl;
                }
                si->lm.sys_data_size += sys_elf->segment_size(i);
            }
            si->lm.sys_segments++;
        }

        if(si->lm.sys_code != MMU::align_directory(si->lm.sys_code))
            db<Setup>(ERR) << "Unaligned OS code segment:" << hex << si->lm.sys_code << endl;
        // db<Setup>(WRN) << "SYS DATA FORA DO FOR!" << si->lm.sys_data << endl;
        if(si->lm.sys_data == ~0U) {
            db<Setup>(WRN) << "OS ELF image has no data segment!" << endl;
            si->lm.sys_data = MMU::align_page(APP_DATA);
        }
        if(si->lm.sys_code != SYS_CODE)
            db<Setup>(ERR) << "OS code segment address (" << reinterpret_cast<void *>(si->lm.sys_code) << ") does not match the machine's memory map (" << reinterpret_cast<void *>(SYS_CODE) << ")!" << endl;
        if(si->lm.sys_code + si->lm.sys_code_size > si->lm.sys_data)
            db<Setup>(ERR) << "OS code segment is too large!" << endl;
        if(si->lm.sys_data != SYS_DATA)
            db<Setup>(ERR) << "OS data segment address (" << reinterpret_cast<void *>(si->lm.sys_data) << ") does not match the machine's memory map (" << reinterpret_cast<void *>(SYS_DATA) << ")!" << endl;
        if(si->lm.sys_data + si->lm.sys_data_size > si->lm.sys_stack)
            db<Setup>(ERR) << "OS data segment is too large!" << endl;
        if(MMU::page_tables(MMU::pages(si->lm.sys_stack_size)) > 1)
            db<Setup>(ERR) << "OS stack segment is too large!" << endl;
    }

    // Check APPLICATION integrity and get the size of its segments
    si->lm.app_entry = 0;
    si->lm.app_segments = 0;
    si->lm.app_code = ~0U;
    si->lm.app_code_size = 0;
    si->lm.app_data = ~0U;
    si->lm.app_data_size = 0;
    si->lm.app_extra = ~0U;
    si->lm.app_extra_size = 0;
    if(si->lm.has_app) {
        ELF * app_elf = reinterpret_cast<ELF *>(&bi[si->bm.application_offset]);
        for (int i = 0; i < app_elf->segments(); i++) {
          db<Setup>(WRN) << "Segment!" << app_elf->segment_size(i) << endl;
        }
        if(!app_elf->valid())
            db<Setup>(ERR) << "APP ELF image is corrupted!" << endl;
        si->lm.app_entry = app_elf->entry();
        for(int i = 0; i < app_elf->segments(); i++) {
            if((app_elf->segment_size(i) == 0) || (app_elf->segment_type(i) != PT_LOAD)) {
              db<Setup>(WRN) << "APP ELF image has an invalid segment!" << app_elf->segment_size(i) << endl;
              continue;
            }
            if((app_elf->segment_address(i) < APP_LOW) || (app_elf->segment_address(i) > APP_HIGH)) {
                db<Setup>(WRN) << "Ignoring APP ELF segment " << i << " at " << hex << app_elf->segment_address(i) << "!"<< endl;
                continue;
            }
            db<Setup>(WRN) << "APP DATA!" << APP_DATA << endl;
            db<Setup>(WRN) << "APP Segment!" << app_elf->segment_address(i) << endl;
            if((APP_DATA - app_elf->segment_address(i)) > 0U) { // CODE
                db<Setup>(WRN) << "Entrou Code!" << endl;
                if(si->lm.app_code_size == 0) {
                    si->lm.app_code_size = app_elf->segment_size(i);
                    si->lm.app_code = app_elf->segment_address(i);
                } else if(app_elf->segment_address(i) < si->lm.app_code) {
                    si->lm.app_code_size = si->lm.app_code - app_elf->segment_address(i) + app_elf->segment_size(i);
                    si->lm.app_code = app_elf->segment_address(i);
                } else if(app_elf->segment_address(i) > (si->lm.app_code + si->lm.app_code_size)) {
                    si->lm.app_code_size = app_elf->segment_address(i) - si->lm.app_code;
                } else
                    si->lm.app_code_size += app_elf->segment_size(i);
            } else { // DATA
              db<Setup>(WRN) << "Segment Address!" << app_elf->segment_address(i) << endl;
              db<Setup>(WRN) << "APP DATA!" << si->lm.app_data << endl;
                if((si->lm.app_data - app_elf->segment_address(i)) > 0U)
                    si->lm.app_data = app_elf->segment_address(i);
                si->lm.app_data_size += app_elf->segment_size(i);
            }
            si->lm.app_segments++;
        }
        if(si->lm.app_code != MMU::align_directory(si->lm.app_code))
            db<Setup>(ERR) << "Unaligned APP code segment:" << hex << si->lm.app_code << endl;
        if(si->lm.app_data == ~0U) {
            db<Setup>(WRN) << "APP ELF image has no data segment!" << endl;
            si->lm.app_data = MMU::align_page(APP_DATA);
        }
        if(Traits<System>::multiheap) { // Application heap in data segment
            si->lm.app_data_size = MMU::align_page(si->lm.app_data_size);
            si->lm.app_stack = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_data_size += MMU::align_page(Traits<Application>::STACK_SIZE);
            si->lm.app_heap = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_data_size += MMU::align_page(Traits<Application>::HEAP_SIZE);
        }
        if(si->lm.has_ext) { // Check for EXTRA data in the boot image
            si->lm.app_extra = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_extra_size = si->bm.img_size - si->bm.extras_offset;
            if(Traits<System>::multiheap)
                si->lm.app_extra_size = MMU::align_page(si->lm.app_extra_size);
            si->lm.app_data_size += si->lm.app_extra_size;
        }
    }
}

void Setup::init_mmu()
{
    unsigned int PT_ENTRIES = MMU::PT_ENTRIES;
    unsigned long pages = MMU::pages(RAM_TOP + 1);

    kout << "Total Pages: " << pages << endl;

    unsigned total_pts = MMU::page_tables(pages);
    kout << "Total Page Tables: " << total_pts << endl;

    unsigned int PD_ENTRIES_LVL_2 = total_pts / PT_ENTRIES;
    unsigned int PD_ENTRIES_LVL_1 = PT_ENTRIES;
    unsigned int PT_ENTRIES_LVL_0 = PT_ENTRIES;

    kout << "LVL 2: " << PD_ENTRIES_LVL_2 << endl;

    Phy_Addr PD2_ADDR = PAGE_TABLES;
    Page_Directory *master = new ((void *)PD2_ADDR) Page_Directory();
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
    kout << "Page Table End Address" << PD1_ADDR << endl;

    kout << "Page Directory End Address" << PD2_ADDR << endl;

    MMU::master(master);

    // Set SATP and enable paging
    db<Setup>(WRN) << "Set SATP" << endl;
    CPU::satp((1UL << 63) | (reinterpret_cast<unsigned long>(master) >> 12));
    db<Setup>(WRN) << "Flush TLB" << endl;
    MMU::flush_tlb();

    // Flush TLB to ensure we've got the right memory organization
};

void Setup::setup_sys_pt()
{
    db<Setup>(TRC) << "Setup::setup_sys_pt(pmm="
                   << "{si="      << reinterpret_cast<void *>(si->pmm.sys_info)
                   << ",pt="      << reinterpret_cast<void *>(si->pmm.sys_pt)
                   << ",pd="      << reinterpret_cast<void *>(si->pmm.sys_pd)
                   << ",sysc={b=" << reinterpret_cast<void *>(si->pmm.sys_code) << ",s=" << MMU::pages(si->lm.sys_code_size) << "}"
                   << ",sysd={b=" << reinterpret_cast<void *>(si->pmm.sys_data) << ",s=" << MMU::pages(si->lm.sys_data_size) << "}"
                   << ",syss={b=" << reinterpret_cast<void *>(si->pmm.sys_stack) << ",s=" << MMU::pages(si->lm.sys_stack_size) << "}"
                   << "})" << endl;

    // Get the physical address for the SYSTEM Page Table
    PT_Entry * sys_pt = reinterpret_cast<PT_Entry *>(si->pmm.sys_pt);
    unsigned int n_pts = MMU::page_tables(MMU::pages(SYS_HIGH - SYS));

    // Clear the System Page Table
    memset(sys_pt, 0, n_pts * sizeof(Page_Table));

    // System Info
    sys_pt[MMU::index(SYS, SYS_INFO)] = MMU::phy2pte(si->pmm.sys_info, Flags::SYS);

    // Set an entry to this page table, so the system can access it later
    sys_pt[MMU::index(SYS, SYS_PT)] = MMU::phy2pte(si->pmm.sys_pt, Flags::SYS);

    // System Page Directory
    sys_pt[MMU::index(SYS, SYS_PD)] = MMU::phy2pte(si->pmm.sys_pd, Flags::SYS);

    unsigned int i;
    PT_Entry aux;

    // SYSTEM code
    for(i = 0, aux = si->pmm.sys_code; i < MMU::pages(si->lm.sys_code_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::index(SYS, SYS_CODE) + i] = MMU::phy2pte(aux, Flags::SYS);

    // SYSTEM data
    for(i = 0, aux = si->pmm.sys_data; i < MMU::pages(si->lm.sys_data_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::index(SYS, SYS_DATA) + i] = MMU::phy2pte(aux, Flags::SYS);

    // SYSTEM stack (used only during init and for the ukernel model)
    for(i = 0, aux = si->pmm.sys_stack; i < MMU::pages(si->lm.sys_stack_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::index(SYS, SYS_STACK) + i] = MMU::phy2pte(aux, Flags::SYS);

    // SYSTEM heap is handled by Init_System, so we don't map it here!

    for(unsigned int i = 0; i < n_pts; i++)
        db<Setup>(INF) << "SYS_PT[" << &sys_pt[i * MMU::PT_ENTRIES] << "]=" << *reinterpret_cast<Page_Table *>(&sys_pt[i * MMU::PT_ENTRIES]) << endl;
}


void Setup::setup_app_pt()
{
    db<Setup>(TRC) << "Setup::setup_app_pt(appc={b=" << reinterpret_cast<void *>(si->pmm.app_code) << ",s=" << MMU::pages(si->lm.app_code_size) << "}"
                   << ",appd={b=" << reinterpret_cast<void *>(si->pmm.app_data) << ",s=" << MMU::pages(si->lm.app_data_size) << "}"
                   << ",appe={b=" << reinterpret_cast<void *>(si->pmm.app_extra) << ",s=" << MMU::pages(si->lm.app_extra_size) << "}"
                   << "})" << endl;

    // Get the physical address for the first APPLICATION Page Tables
    PT_Entry * app_code_pt = reinterpret_cast<PT_Entry *>(si->pmm.app_code_pts);
    PT_Entry * app_data_pt = reinterpret_cast<PT_Entry *>(si->pmm.app_data_pts);
    unsigned int n_pts_code = MMU::page_tables(MMU::pages(si->lm.app_code_size));
    unsigned int n_pts_data = MMU::page_tables(MMU::pages(si->lm.app_data_size));

    // Clear the first APPLICATION Page Tables
    memset(app_code_pt, 0, n_pts_code * sizeof(Page_Table));
    memset(app_data_pt, 0, n_pts_data * sizeof(Page_Table));

    unsigned int i;
    PT_Entry aux;

    // APPLICATION code
    // Since load_parts() will load the code into memory, the code segment can't be marked R/O yet
    // The correct flags (APPC and APPD) will be configured after the execution of load_parts(), by adjust_perms()
    for(i = 0, aux = si->pmm.app_code; i < MMU::pages(si->lm.app_code_size); i++, aux = aux + sizeof(Page))
        app_code_pt[MMU::page(si->lm.app_code) + i] = MMU::phy2pte(aux, Flags::APP);

    // APPLICATION data (contains stack, heap and extra)
    for(i = 0, aux = si->pmm.app_data; i < MMU::pages(si->lm.app_data_size); i++, aux = aux + sizeof(Page))
        app_data_pt[MMU::page(si->lm.app_data) + i] = MMU::phy2pte(aux, Flags::APP);

    for(unsigned int i = 0; i < n_pts_code; i++)
        db<Setup>(INF) << "APPC_PT[" << &app_code_pt[i * MMU::PT_ENTRIES] << "]=" << *reinterpret_cast<Page_Table *>(&app_code_pt[i * MMU::PT_ENTRIES]) << endl;
    for(unsigned int i = 0; i < n_pts_data; i++)
        db<Setup>(INF) << "APPD_PT[" << &app_data_pt[i * MMU::PT_ENTRIES] << "]=" << *reinterpret_cast<Page_Table *>(&app_data_pt[i * MMU::PT_ENTRIES]) << endl;
}


void Setup::setup_sys_pd()
{
    db<Setup>(TRC) << "Setup::setup_sys_pd(bm="
                   << "{memb="  << reinterpret_cast<void *>(si->bm.mem_base)
                   << ",memt="  << reinterpret_cast<void *>(si->bm.mem_top)
                   << ",miob="  << reinterpret_cast<void *>(si->bm.mio_base)
                   << ",miot="  << reinterpret_cast<void *>(si->bm.mio_top)
                   << ",si="    << reinterpret_cast<void *>(si->pmm.sys_info)
                   << ",spt="   << reinterpret_cast<void *>(si->pmm.sys_pt)
                   << ",spd="   << reinterpret_cast<void *>(si->pmm.sys_pd)
                   << ",mem="   << reinterpret_cast<void *>(si->pmm.phy_mem_pts)
                   << ",io="    << reinterpret_cast<void *>(si->pmm.io_pts)
                   << ",umemb=" << reinterpret_cast<void *>(si->pmm.usr_mem_base)
                   << ",umemt=" << reinterpret_cast<void *>(si->pmm.usr_mem_top)
                   << ",sysc="  << reinterpret_cast<void *>(si->pmm.sys_code)
                   << ",sysd="  << reinterpret_cast<void *>(si->pmm.sys_data)
                   << ",syss="  << reinterpret_cast<void *>(si->pmm.sys_stack)
                   << ",apct="  << reinterpret_cast<void *>(si->pmm.app_code_pts)
                   << ",apdt="  << reinterpret_cast<void *>(si->pmm.app_data_pts)
                   << ",fr1b="  << reinterpret_cast<void *>(si->pmm.free1_base)
                   << ",fr1t="  << reinterpret_cast<void *>(si->pmm.free1_top)
                   << ",fr2b="  << reinterpret_cast<void *>(si->pmm.free2_base)
                   << ",fr2t="  << reinterpret_cast<void *>(si->pmm.free2_top)
                   << "})" << endl;

    // Get the physical address for the System Page Directory
    PT_Entry * sys_pd = reinterpret_cast<PT_Entry *>(si->pmm.sys_pd);

    // Clear the System Page Directory
    memset(sys_pd, 0, sizeof(Page_Directory));

    // Calculate the number of page tables needed to map the physical memory
    unsigned int mem_size = MMU::pages(si->bm.mem_top - si->bm.mem_base);
    unsigned int n_pts = MMU::page_tables(mem_size);

    // Map the whole physical memory into the page tables pointed by phy_mem_pts
    PT_Entry * pts = reinterpret_cast<PT_Entry *>(si->pmm.phy_mem_pts);
    for(unsigned int i = MMU::page(si->bm.mem_base), j = 0; i < MMU::page(si->bm.mem_base) + mem_size; i++, j++)
        pts[i] = MMU::phy2pte(si->bm.mem_base + j * sizeof(Page), Flags::SYS);

    // Attach all the physical memory starting at PHY_MEM
    assert((MMU::directory(MMU::align_directory(PHY_MEM)) + n_pts) < (MMU::PD_ENTRIES - 4)); // check if it would overwrite the OS
    for(unsigned int i = MMU::directory(MMU::align_directory(PHY_MEM)), j = 0; i < MMU::directory(MMU::align_directory(PHY_MEM)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde(si->pmm.phy_mem_pts + j * sizeof(Page_Table));

    // Attach the portion of the physical memory used by Setup at SETUP
    assert(MMU::directory(SETUP) == MMU::directory(RAM_BASE));
    sys_pd[MMU::directory(SETUP)] =  MMU::phy2pde(si->pmm.phy_mem_pts);

    // Attach the portion of the physical memory used by int_m2s at INT_M2S
    assert(MMU::directory(INT_M2S) == MMU::directory(RAM_TOP));
    sys_pd[MMU::directory(INT_M2S)] =  MMU::phy2pde(si->pmm.phy_mem_pts + (n_pts - 1) * sizeof(Page));

    // Attach all the physical memory starting at RAM_BASE (used in library mode)
    assert((MMU::directory(MMU::align_directory(RAM_BASE)) + n_pts) < (MMU::PD_ENTRIES - 1)); // check if it would overwrite the OS
    if(RAM_BASE != PHY_MEM)
        for(unsigned int i = MMU::directory(MMU::align_directory(RAM_BASE)), j = 0; i < MMU::directory(MMU::align_directory(RAM_BASE)) + n_pts; i++, j++)
            sys_pd[i] = MMU::phy2pde(si->pmm.phy_mem_pts + j * sizeof(Page_Table));

    // Calculate the number of page tables needed to map the IO address space
    unsigned int io_size = MMU::pages(si->bm.mio_top - si->bm.mio_base);
    n_pts = MMU::page_tables(io_size);

    // Map I/O address space into the page tables pointed by io_pts
    pts = reinterpret_cast<PT_Entry *>(si->pmm.io_pts);
    for(unsigned int i = 0; i < io_size; i++)
        pts[i] = MMU::phy2pte(si->bm.mio_base + i * sizeof(Page), Flags::IO);

    // Attach devices' memory at Memory_Map::IO
    assert((MMU::directory(MMU::align_directory(IO)) + n_pts) < (MMU::PD_ENTRIES - 3)); // check if it would overwrite the OS
    for(unsigned int i = MMU::directory(MMU::align_directory(IO)), j = 0; i < MMU::directory(MMU::align_directory(IO)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde(si->pmm.io_pts + j * sizeof(Page_Table));

    // Attach the OS (i.e. sys_pt)
    n_pts = MMU::page_tables(MMU::pages(SYS_HEAP - SYS)); // SYS_HEAP is handled by Init_System
    for(unsigned int i = MMU::directory(MMU::align_directory(SYS)), j = 0; i < MMU::directory(MMU::align_directory(SYS)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde(si->pmm.sys_pt + j * sizeof(Page_Table));

    // Attach the first APPLICATION CODE (i.e. app_code_pt)
    n_pts = MMU::page_tables(MMU::pages(si->lm.app_code_size));
    for(unsigned int i = MMU::directory(MMU::align_directory(si->lm.app_code)), j = 0; i < MMU::directory(MMU::align_directory(si->lm.app_code)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde(si->pmm.app_code_pts + j * sizeof(Page_Table));

    // Attach the first APPLICATION DATA (i.e. app_data_pt, containing heap, stack and extra)
    n_pts = MMU::page_tables(MMU::pages(si->lm.app_data_size));
    for(unsigned int i = MMU::directory(MMU::align_directory(si->lm.app_data)), j = 0; i < MMU::directory(MMU::align_directory(si->lm.app_data)) + n_pts; i++, j++)
        sys_pd[i] = MMU::phy2pde(si->pmm.app_data_pts + j * sizeof(Page_Table));

    db<Setup>(INF) << "SYS_PD[" << sys_pd << "]=" << *reinterpret_cast<Page_Directory *>(sys_pd) << endl;
}


void Setup::setup_m2s()
{
    db<Setup>(TRC) << "Setup::setup_m2s()" << endl;

    memcpy(reinterpret_cast<void *>(INT_M2S), reinterpret_cast<void *>(&_int_m2s), sizeof(Page));
}


void Setup::enable_paging()
{
    db<Setup>(TRC) << "Setup::enable_paging()" << endl;
    if(Traits<Setup>::hysterically_debugged) {
        db<Setup>(INF) << "Setup::pc=" << CPU::pc() << endl;
        db<Setup>(INF) << "Setup::sp=" << CPU::sp() << endl;
    }

    // Set SATP and enable paging
    MMU::pd(si->pmm.sys_pd);

    // Flush TLB to ensure we've got the right memory organization
    MMU::flush_tlb();

    if(Traits<Setup>::hysterically_debugged) {
        db<Setup>(INF) << "Setup::pc=" << CPU::pc() << endl;
        db<Setup>(INF) << "Setup::sp=" << CPU::sp() << endl;
    }
}


void Setup::load_parts()
{
    db<Setup>(TRC) << "Setup::load_parts()" << endl;

    // Adjust bi to its logical address
    bi = static_cast<char *>(MMU::phy2log(bi));

    // Relocate System_Info
    if(sizeof(System_Info) > sizeof(Page))
        db<Setup>(WRN) << "System_Info is bigger than a page (" << sizeof(System_Info) << ")!" << endl;
    if(Traits<Setup>::hysterically_debugged) {
        db<Setup>(INF) << "Setup:BOOT_IMAGE: " << MMU::Translation(bi) << endl;
        db<Setup>(INF) << "Setup:SYS_INFO[phy]: " << MMU::Translation(si) << endl;
        db<Setup>(INF) << "Setup:SYS_INFO[log]: " << MMU::Translation(SYS_INFO) << endl;
    }
    memcpy(reinterpret_cast<System_Info *>(SYS_INFO), si, sizeof(System_Info));
    si = reinterpret_cast<System_Info *>(SYS_INFO);

    // Load INIT
    if(si->lm.has_ini) {
        db<Setup>(TRC) << "Setup::load_init()" << endl;
        ELF * ini_elf = reinterpret_cast<ELF *>(&bi[si->bm.init_offset]);
        for(int i = 0; i < ini_elf->segments(); i++) {
            if(ini_elf->segment_type(i) != PT_LOAD)
                continue;
            if(Traits<Setup>::hysterically_debugged) {
                db<Setup>(INF) << "Setup:ini_elf: " << MMU::Translation(ini_elf) << endl;
                db<Setup>(INF) << "Setup:ini_elf[" << i <<"]: " << MMU::Translation(ini_elf->segment_address(i)) << endl;
                db<Setup>(INF) << "Setup:ini_elf[" << i <<"].size: " << ini_elf->segment_size(i) << endl;
            }
            if(ini_elf->load_segment(i) < 0)
                db<Setup>(ERR) << "INIT segment " << i << " was corrupted during SETUP!" << endl;
        }
    }

    // Load SYSTEM
    if(si->lm.has_sys) {
        db<Setup>(TRC) << "Setup::load_sys()" << endl;
        ELF * sys_elf = reinterpret_cast<ELF *>(&bi[si->bm.system_offset]);
        for(int i = 0; i < sys_elf->segments(); i++) {
            if(sys_elf->segment_type(i) != PT_LOAD)
                continue;
            if(Traits<Setup>::hysterically_debugged) {
                db<Setup>(INF) << "Setup:sys_elf: " << MMU::Translation(sys_elf) << endl;
                db<Setup>(INF) << "Setup:sys_elf[" << i <<"]: " << MMU::Translation(sys_elf->segment_address(i)) << endl;
                db<Setup>(INF) << "Setup:sys_elf[" << i <<"].size: " << sys_elf->segment_size(i) << endl;
            }
            if(sys_elf->load_segment(i) < 0)
                db<Setup>(ERR) << "SYS segment " << i << " was corrupted during SETUP!" << endl;
        }
    }

    // Load APP
    if(si->lm.has_app) {
        db<Setup>(TRC) << "Setup::load_app()" << endl;
        ELF * app_elf = reinterpret_cast<ELF *>(&bi[si->bm.application_offset]);
        for(int i = 0; i < app_elf->segments(); i++) {
            if(app_elf->segment_type(i) != PT_LOAD)
                continue;
            if(Traits<Setup>::hysterically_debugged) {
                db<Setup>(INF) << "Setup:app_elf: " << MMU::Translation(app_elf) << endl;
                db<Setup>(INF) << "Setup:app_elf[" << i <<"]: " << MMU::Translation(app_elf->segment_address(i)) << endl;
                db<Setup>(INF) << "Setup:app_elf[" << i <<"].size: " << app_elf->segment_size(i) << endl;
            }
            if(app_elf->load_segment(i) < 0)
                db<Setup>(ERR) << "APP segment " << i << " was corrupted during SETUP!" << endl;
        }
    }

    // Load EXTRA
    if(si->lm.has_ext) {
        db<Setup>(TRC) << "Setup::load_extra()" << endl;
        if(Traits<Setup>::hysterically_debugged)
            db<Setup>(INF) << "Setup:app_ext:" << MMU::Translation(si->lm.app_extra) << endl;
        memcpy(Log_Addr(si->lm.app_extra), &bi[si->bm.extras_offset], si->lm.app_extra_size);
    }
}


void Setup::adjust_perms()
{
    db<Setup>(TRC) << "Setup::adjust_perms(appc={b=" << reinterpret_cast<void *>(si->pmm.app_code) << ",s=" << MMU::pages(si->lm.app_code_size) << "}"
                   << ",appd={b=" << reinterpret_cast<void *>(si->pmm.app_data) << ",s=" << MMU::pages(si->lm.app_data_size) << "}"
                   << ",appe={b=" << reinterpret_cast<void *>(si->pmm.app_extra) << ",s=" << MMU::pages(si->lm.app_extra_size) << "}"
                   << "})" << endl;

    // Get the logical address of the first APPLICATION Page Tables
    PT_Entry * app_code_pt = MMU::phy2log(reinterpret_cast<PT_Entry *>(si->pmm.app_code_pts));
    PT_Entry * app_data_pt = MMU::phy2log(reinterpret_cast<PT_Entry *>(si->pmm.app_data_pts));

    unsigned int i;
    PT_Entry aux;

    // APPLICATION code
    for(i = 0, aux = si->pmm.app_code; i < MMU::pages(si->lm.app_code_size); i++, aux = aux + sizeof(Page))
        app_code_pt[MMU::page(APP_CODE) + i] = MMU::phy2pte(aux, Flags::APPC);

    // APPLICATION data (contains stack, heap and extra)
    for(i = 0, aux = si->pmm.app_data; i < MMU::pages(si->lm.app_data_size); i++, aux = aux + sizeof(Page))
        app_data_pt[MMU::page(APP_DATA) + i] = MMU::phy2pte(aux, Flags::APPD);
}


void Setup::call_next()
{
    // Check for next stage and obtain the entry point
    Log_Addr pc;
    if(multitask) {
        if(si->lm.has_ini) {
            db<Setup>(TRC) << "Executing system's global constructors ..." << endl;
            reinterpret_cast<void (*)()>((void *)si->lm.sys_entry)();
            pc = si->lm.ini_entry;
        } else if(si->lm.has_sys)
            pc = si->lm.sys_entry;
        else
            pc = si->lm.app_entry;

        // Arrange a stack for each CPU to support stage transition
        // The 2 integers on the stacks are room for the return address
        Log_Addr sp = SYS_STACK + Traits<System>::STACK_SIZE - 2 * sizeof(int);

        db<Setup>(TRC) << "Setup::call_next(pc=" << pc << ",sp=" << sp << ") => ";
        if(si->lm.has_ini)
            db<Setup>(TRC) << "INIT" << endl;
        else if(si->lm.has_sys)
            db<Setup>(TRC) << "SYSTEM" << endl;
        else
            db<Setup>(TRC) << "APPLICATION" << endl;

        // Set SP and call the next stage
        CPU::sp(sp);
    } else
        pc = &_start;

    db<Setup>(INF) << "SETUP ends here!" << endl;

    static_cast<void (*)()>(pc)();

    if(multitask) {
        // This will only happen when INIT was called and Thread was disabled
        // Note we don't have the original stack here anymore!
        reinterpret_cast<CPU::FSR *>(si->lm.app_entry)();
    }

    CPU::sepc(si->lm.ini_entry);
    CLINT::stvec(CLINT::DIRECT, CPU::Reg(&_int_entry));

    CPU::sret();
    db<Setup>(ERR) << "OS failed to init!" << endl;
}

__END_SYS

using namespace EPOS::S;

void _entry() // machine mode
{
    db<Setup>(WRN) << "Entrou no Entry" << endl;
    // SiFive-U core 0 doesn't have MMU
    if (CPU::mhartid() == 0)
        CPU::halt();

    CPU::mstatusc(CPU::MIE);                            // disable interrupts (they will be reenabled at Init_End)
    CPU::mies(CPU::MSI | CPU::MTI | CPU::MEI);          // enable interrupts generation by CLINT at machine level

    // ensure that sapt is 0
    db<Setup>(WRN) << "SATP" << endl;

    CPU::satp(0);
    Machine::clear_bss();
    db<Setup>(WRN) << "Stack Pointer" << endl;

    if(Traits<System>::multitask) {
        CLINT::mtvec(CLINT::DIRECT, Memory_Map::INT_M2S); // setup a machine mode interrupt handler to forward timer interrupts (which cannot be delegated via mideleg)
        CPU::mideleg(0xffff);                           // delegate all possible interrupts to supervisor mode (MTI can't be delegated https://groups.google.com/a/groups.riscv.org/g/sw-dev/c/A5XmyE5FE_0/m/TEnvZ0g4BgAJ)
        CPU::medeleg(0xffff);                           // delegate all exceptions to supervisor mode
        CPU::mstatuss(CPU::MPP_S);                      // prepare jump into supervisor mode at mret
        CPU::sstatuss(CPU::SUM);                        // allows User Memory access in supervisor mode
    } else {
        CPU::mstatus(CPU::MPP_M);                       // stay in machine mode at mret
    }

    // Set up the Physical Memory Protection registers correctly
    // A = NAPOT, X, R, W
    CPU::pmpcfg0(0x1f);
    // All memory
    CPU::pmpaddr0((1UL << 55) - 1);

    // Delegate all traps to supervisor
    // Timer will not be delegated due to architecture reasons.
    CPU::mideleg(CPU::SSI | CPU::STI | CPU::SEI);
    CPU::medeleg(0xffff);

    // Relocate _mmode_forward - 1024 bytes are enough
    char *src = reinterpret_cast<char *>(&_mmode_forward);
    char *dst = reinterpret_cast<char *>(Memory_Map::MMODE_F);
    for (int i = 0; i < 1024; i++)
    {
        *dst = *src;
        src++;
        dst++;
    }

    CPU::mies(CPU::MSI | CPU::MTI | CPU::MEI);                  // enable interrupts generation by CLINT                                      // (mstatus) disable interrupts (they will be reenabled at Init_End)
    CLINT::mtvec(CLINT::DIRECT, CPU::Reg(Memory_Map::MMODE_F)); // setup a preliminary machine mode interrupt handler pointing it to _mmode_forward

    db<Setup>(WRN) << "Setup" << endl;

    // MPP_S = change to supervirsor
    // MPIE = otherwise we won't ever receive interrupts
    CPU::mstatus(CPU::MPP_S | CPU::MPIE);
    CPU::mepc(CPU::Reg(&_setup)); // entry = _setup
    CPU::mret();                  // enter supervisor mode at setup (mepc) with interrupts enabled (mstatus.mpie = true)
};

void _setup() // supervisor mode
{
    db<Setup>(WRN) << "Entrou no Setup" << endl;

    kerr << endl;
    kout << endl;

    Setup setup;
}

// RISC-V 32 doesn't allow timer interrupts (MTI) to be handled in supervisor mode. The matching of MTIMECMP always triggers interrupt MTI and there seems to be no mechanism in CLINT to trigger STI.
// Therefore, an interrupt forwarder must be installed. We use RAM_TOP for this, with the code at the beginning of the last page and a stack at the end of the same page.
void _int_m2s()
{
    // Save context
    ASM("        csrw  mscratch,     sp                                 \n");
if(Traits<CPU>::WORD_SIZE == 32) {
    ASM("        la          sp,     %0                                 \n"
        "        sw          a2,   0(sp)                                \n"
        "        sw          a3,   4(sp)                                \n"
        "        sw          a4,   8(sp)                                \n"
        "        sw          a5,  12(sp)                                \n" : : "i"(Memory_Map::BOOT_STACK - 16));
} else {
    ASM("        lui         sp,     %0                                 \n"
        "        addi        sp, sp, %1                                 \n"
        "        sd          a2,   0(sp)                                \n"
        "        sd          a3,   8(sp)                                \n"
        "        sd          a4,  16(sp)                                \n"
        "        sd          a5,  24(sp)                                \n" : : "i"((Memory_Map::BOOT_STACK - 32) >> 12), "i"((Memory_Map::BOOT_STACK - 32) && 0xfff));
}

    CPU::Reg id = CPU::mcause();

    if((id & CLINT::INT_MASK) == CLINT::IRQ_MAC_TIMER) {
        // MIP.MTI is a direct logic on (MTIME == MTIMECMP) and reseting the Timer (i.e. adjusting MTIMECMP) seems to be the only way to clear it
        Timer::reset();
        CPU::sies(CPU::STI);
    }

    CPU::Reg i = 1 << ((id & CLINT::INT_MASK) - 2);
    if(CPU::int_enabled() && (CPU::sie() & i))
        CPU::mips(i); // forward to supervisor mode

    // Restore context
if(Traits<CPU>::WORD_SIZE == 32) {
    ASM("        lw          a2,   0(sp)                                \n"
        "        lw          a3,   4(sp)                                \n"
        "        lw          a4,   8(sp)                                \n"
        "        lw          a5,  12(sp)                                \n");
} else {
    ASM("        ld          a2,   0(sp)                                \n"
        "        ld          a3,   8(sp)                                \n"
        "        ld          a4,  16(sp)                                \n"
        "        ld          a5,  24(sp)                                \n");
}
    ASM("        csrr        sp, mscratch                               \n"
        "        mret                                                   \n");
}
