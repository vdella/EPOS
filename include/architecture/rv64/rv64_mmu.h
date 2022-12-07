// EPOS-- RISC-V 64 MMU Mediator Declarations

#ifndef __rv64_mmu_h
#define __rv64_mmu_h

#include <system/memory_map.h>
#include <utility/string.h>
#include <utility/list.h>
#include <utility/debug.h>
#include <architecture/cpu.h>
#include <architecture/mmu.h>

__BEGIN_SYS

class MMU : public MMU_Common<9, 9, 9, 12>
{
    friend class CPU;
    friend class Setup_SifiveU;

private:
    typedef Grouping_List<Log_Addr> List;

    static const unsigned long PHY_MEM = Memory_Map::PHY_MEM;
    static const unsigned long RAM_BASE = Memory_Map::RAM_BASE;
    static const unsigned long APP_LOW = Memory_Map::APP_LOW;

public:
    // Page Flags
    class RV64_Flags
    {
    public:
        enum
        {
            V = 1 << 0,   // Valid
            R = 1 << 1,   // Readable
            W = 1 << 2,   // Writable
            X = 1 << 3,   // Executable
            U = 1 << 4,   // User
            G = 1 << 5,   // Global
            A = 1 << 6,   // Accesed
            D = 1 << 7,   // Dirty
            CT = 1 << 8,  // Contiguous
            MIO = 1 << 9, // I/O
            MASK = (1 << 10) - 1,
            APP = (V | R | W | X),
            SYS = (V | R | W | X | A | D),
            KC = (V | R | X),
            KD = (V | R | W),
            UD = (V | R | W | U),
            PD = (V | A | D),
            UA = (V | R | W | X | U)
        };

        RV64_Flags() {}
        RV64_Flags(const RV64_Flags &f) : _flags(f) {}
        RV64_Flags(unsigned int f) : _flags(f) {}
        RV64_Flags(const Flags &f) : _flags(((f & Flags::PRE) ? V : 0) |
                                            ((f & Flags::RW) ? (R | W) : R) |
                                            ((f & Flags::USR) ? U : 0) |
                                            ((f & Flags::EX) ? X : 0)) {}
        operator unsigned int() const { return _flags; }

    private:
        unsigned int _flags;
    };

    // Page_Table
    class Page_Table
    {
    private:
        PT_Entry ptes[PT_ENTRIES];

    public:
        Page_Table() {}

        PT_Entry &operator[](unsigned int i) { return ptes[i]; }

        PT_Entry get_entry(unsigned int i) { return ptes[i]; }

        void map(RV64_Flags flags, int from, int to)
        {
            db<MMU>(WRN) << "to - from=" << to - from << endl;
            Phy_Addr *addr = alloc(to - from);
            if (addr)
            {
                remap(addr, flags, from, to);
            }
            else
            {
                db<MMU>(WRN) << "Entrou no else" << endl;
                for (; from < to; from++)
                {
                    ptes[from] = phy2pte(alloc(1), flags);
                }
            }
        }

        void remap(Phy_Addr addr, RV64_Flags flags, int from = 0, int to = 512, int size = sizeof(Page))
        {
            addr = align_page(addr);
            for (; from < to; from++)
            {
                Log_Addr *pte = phy2log(&ptes[from]);
                *pte = phy2pte(addr, flags);

                // ptes[from] = phy2pte(addr, flags);
                addr += size;
            }
        }
    };

    // Chunk (for Segment)
    class Chunk
    {
    public:
        Chunk() {}

        // from 0 to bytes % PAGE_SIZE
        // pts = number of page_tables
        // pages = number of pages
        Chunk(unsigned int bytes, Flags flags)
        {

            _from = 0;
            _to = pages(bytes);
            _pts = page_tables(_to - _from);
            _bytes = bytes;

            db<MMU>(WRN) << "Chunk from " << _from << " to " << _to << endl;
            db<MMU>(WRN) << "Chunk pts " << _pts << endl;

            _flags = RV64_Flags(flags);
            _pt = calloc(_pts);
            db<MMU>(WRN) << "Chunk pt " << _pt << endl;
            for (unsigned int i = 0; i < _pts; i++)
            {
                // if(_to < 512 && _to > 0){ _from, _to} else {_from, 512  _to - 512}
                if (_to < 512 && _to > 0)
                    (_pt + (i * PAGE_SIZE))->map(_flags, _from, _to);
                else {
                    (_pt + (i * PAGE_SIZE))->map(_flags, _from, 512);
                    _to -= 512;
                }
            }
            // _pt->map(_flags, _from, _to);
        }

        ~Chunk()
        {
            for (; _from < _to; _from++)
                free((*static_cast<Page_Table *>(phy2log(_pt)))[_from]);
            free(_pt, _pts);
        }

        unsigned int pts() const { return _pts; }
        Page_Table *pt() const { return _pt; }
        unsigned int size() const { return _bytes; }
        RV64_Flags flags() const {return _flags;}
        int resize(unsigned int amount) { return 0; }

    private:
        unsigned int _from;
        unsigned int _to;
        unsigned int _pts;
        unsigned int _bytes;
        RV64_Flags _flags;
        Page_Table *_pt;
    };

    // Page Directory
    typedef Page_Table Page_Directory;

    // Directory (for Address_Space)
    class Directory
    {
    public:
        Directory() : _pd(phy2log(calloc(513))), _free(true)
        {
            db<MMU>(WRN) << "Directory pd antes " << _pd << endl;
            for (unsigned int i = 0; i < PD_ENTRIES_LVL_1; i++)
            {
                // db<MMU>(WRN) << "_master = " << _master << endl;
                db<MMU>(WRN) << "_master[i] = " << _master->get_entry(i) << " " << i << endl;
                // db<MMU>(WRN) << "i = " << i << endl;
                (*_pd)[i] = _master->get_entry(i);
            }
            db<MMU>(WRN) << "pte2phy = " << pte2phy(_master->get_entry(10)) << endl;
            db<MMU>(WRN) << "Directory pd depois " << _pd << endl;
        }

        Directory(Page_Directory *pd) : _pd(pd), _free(false) {}

        ~Directory()
        {
            if (_free)
                free(_pd);
        }

        Page_Table *pd() const { return _pd; }

        void activate() const {
          CPU::satp((1UL << 63) | (reinterpret_cast<unsigned long>(_pd) >> 12));
          MMU::flush_tlb();
        }

        // Attach Chunk's PT into the Address Space and return the Page Directory base address.
        Log_Addr attach(const Chunk &chunk, unsigned int lvl2 = directory_lvl_2(APP_LOW), unsigned int lvl1 = directory_lvl_1(APP_LOW))
        {
            for (unsigned int i = lvl2; i < PD_ENTRIES_LVL_2; i++)
                for (unsigned int j = lvl1; j < PD_ENTRIES_LVL_1 - chunk.pts(); j++)
                    if (attach(i, j, chunk.pt(), chunk.pts(), chunk.flags()))
                    {
                        Log_Addr addr = i << (DIRECTORY_SHIFT_LVL_2) | j << DIRECTORY_SHIFT_LVL_1;
                        db<MMU>(WRN) << "Attach Return: " << addr << "!!!!" << endl;
                        return addr;
                    }
            return false;
        }

        // Used to create non-relocatable segments such as code
        Log_Addr attach(const Chunk &chunk, const Log_Addr &addr)
        {
            unsigned int lvl2 = directory_lvl_2(addr);
            db<MMU>(WRN) << "Attach lvl2: " << lvl2 << endl;

            unsigned int lvl1 = directory_lvl_1(addr);
            db<MMU>(WRN) << "Attach lvl1: " << lvl1 << endl;

            if (!attach(lvl2, lvl1, chunk.pt(), chunk.pts(), chunk.flags()))
                return Log_Addr(false);

            return lvl2 << (DIRECTORY_SHIFT_LVL_2) | lvl1 << DIRECTORY_SHIFT_LVL_1;
        }

        void detach(const Chunk &chunk, unsigned int lvl2 = directory_lvl_2(APP_LOW), unsigned int lvl1 = directory_lvl_1(APP_LOW))
        {
            for (unsigned int i = lvl2; i < PD_ENTRIES_LVL_2; i++)
            {
                if ((*_pd)[i])
                {
                    Page_Directory *pd1 = new ((void *)(pte2phy((*_pd)[i]))) Page_Directory();
                    for (unsigned int j = lvl1; j < PD_ENTRIES_LVL_1; j++)
                    {
                        if (indexes(pte2phy((*pd1)[j])) == indexes(phy2log(chunk.pt())))
                        {
                            detach(i, j, chunk.pt(), chunk.pts());
                            return;
                        }
                    }
                }
            }
            db<MMU>(WRN) << "MMU::Directory::detach(pt=" << chunk.pt() << ") failed!" << endl;
        }

        void detach(const Chunk &chunk, Log_Addr addr)
        {
            db<MMU>(WRN) << "Addr " << addr << endl;
            unsigned int lvl2 = directory_lvl_2(addr);
            unsigned int lvl1 = directory_lvl_1(addr);
            Page_Directory *pd1 = new ((void *)(pte2phy((*_pd)[lvl2]))) Page_Directory();
            if (indexes(pte2phy((*pd1)[lvl1])) != indexes(chunk.pt()))
            {
                db<MMU>(WRN) << "MMU::Directory::detach(pt=" << chunk.pt() << ",addr=" << addr << ") failed!" << endl;
                return;
            }
            detach(lvl2, lvl1, chunk.pt(), chunk.pts());
        }

        Phy_Addr physical(Log_Addr addr) { return addr; }

    private:
        bool attach(unsigned int lvl2, unsigned int lvl1, const Page_Table *pt, unsigned int n, RV64_Flags flags)
        {
            if (lvl2 > PD_ENTRIES_LVL_2 - 1)
                return false;

            if ((*_pd)[lvl2])
            {

                Page_Directory *pd1 = new ((void *)(pte2phy(_pd->get_entry(lvl2)))) Page_Directory();
                // db<MMU>(WRN) << "Directory pd1 " << pd1 << endl;

                for (unsigned int i = lvl1; i < lvl1 + n; i++)
                {
                    if (pd1->get_entry(i))
                    {
                        return false;
                    }
                }

                for (unsigned int i = lvl1; i < lvl1 + n; i++, pt++)
                {
                    (*pd1)[i] = phy2pte(Phy_Addr(pt), flags);
                }
                return true;
            }

            Page_Directory * pd1 = new ((void*)(_pd + (lvl2 + 1) * PAGE_SIZE)) Page_Directory();
            _pd->remap(pd1, flags, lvl2, lvl2+1);
            return attach(lvl2, lvl1, pt, n, flags);
        }

        void detach(unsigned int lvl2, unsigned int lvl1, Page_Table *pt, unsigned int n)
        {
            Page_Directory *pd1 = new ((void *)(pte2phy((*_pd)[lvl2]))) Page_Directory();
            for (unsigned int i = lvl1; i < lvl1 + n; i++)
            {
                (*pd1)[i] = 0;
            }
        }

    private:
        Page_Directory *_pd;
        bool _free;
    };

public:
    MMU() {}

    static Phy_Addr alloc(unsigned int bytes = 1)
    {
        Phy_Addr phy(false);
        unsigned long size = 0;
        if (bytes)
        {
            List::Element *e = _free.search_decrementing(bytes);
            if (e)
            {
                db<MMU>(INF) << "Object: " << e->object() << endl;
                size = e->size();
                phy = reinterpret_cast<unsigned long>(e->object()) + e->size(); // PAGE_SIZE
            }
            else
            {
                db<MMU>(ERR) << "MMU::alloc() failed!" << endl;
            }
            db<MMU>(INF) << "MMU::alloc(bytes=" << bytes << ") => " << phy << endl;
            db<MMU>(INF) << "MMU::List Element: " << sizeof(List::Element) << endl;
        }

        db<MMU>(INF) << "Size: " << size << endl;

        return phy;
    };

    static Phy_Addr calloc(unsigned int bytes = 1)
    {
        Phy_Addr phy = alloc(bytes);
        memset(phy2log(phy), 0, bytes * PAGE_SIZE);
        return phy;
    }

    static void free(Phy_Addr addr, unsigned long n = 1)
    {
        db<MMU>(TRC) << "MMU::free(addr=" << addr << ",n=" << n << ")" << endl;

        addr = indexes(addr);

        assert(Traits<CPU>::unaligned_memory_access || !(addr % 4));

        if (addr && n)
        {
            List::Element *e = new (addr) List::Element(addr, n);
            List::Element *m1, *m2;
            _free.insert_merging(e, &m1, &m2);
        }
        db<MMU>(INF) << "List Size: " << _free.size() << endl;
    }

    static unsigned int allocable() { return _free.head() ? _free.head()->size() : 0; }

    static void master(Page_Directory * pd) {_master = pd;}

    static Page_Directory *volatile current() { return static_cast<Page_Directory *volatile>(phy2log(CPU::pdp())); }

    static void flush_tlb() { CPU::flush_tlb(); }
    static void flush_tlb(Log_Addr addr) { CPU::flush_tlb(addr); }

// private:
    static void init();

    // static Log_Addr phy2log(const Phy_Addr & phy) { return phy + (PHY_MEM - RAM_BASE); }
    static Log_Addr phy2log(const Phy_Addr &phy) { return phy; }

    static PD_Entry phy2pde(Phy_Addr bytes) { return ((bytes >> 12) << 10) | RV64_Flags::V; }

    static Phy_Addr pde2phy(PD_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }

    static PT_Entry phy2pte(Phy_Addr bytes, RV64_Flags flags) { return ((bytes >> 12) << 10) | flags; }

    static Phy_Addr pte2phy(PT_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }

private:
    static List _free;
    static Page_Directory *_master;
};
__END_SYS

#endif
