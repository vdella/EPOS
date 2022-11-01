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

class MMU: public MMU_Common<9, 9, 9, 12>
{
    friend class CPU;
    friend class Setup_SifiveU;

private:
    typedef Grouping_List<Log_Addr> List;


    static const unsigned long PHY_MEM  = Memory_Map::PHY_MEM;
    static const unsigned long RAM_BASE = Memory_Map::RAM_BASE;

public:
    // Page Flags
    class RV64_Flags
    {
    public:
        enum {
            V   = 1 << 0, //Valid
            R   = 1 << 1, //Readable
            W   = 1 << 2, //Writable
            X   = 1 << 3, //Executable
            U   = 1 << 4, //User
            G   = 1 << 5, // Global
            A   = 1 << 6, //Accesed
            D   = 1 << 7, //Dirty
            CT  = 1 << 8, // Contiguous
            MIO  = 1 << 9, // I/O
            MASK = (1 << 10) - 1,
            APP  = (V | R | W | X),
            SYS  = (V | R | W | X),
            KC   = (V | R | X),
            KD   = (V | R | W),
            UD   = (V | R | W | U),
            UC   = (V | R | X | U),
        };

        RV64_Flags() {}
        RV64_Flags(const RV64_Flags & f): _flags(f) {}
        RV64_Flags(unsigned int f): _flags(f) {}
        RV64_Flags(const Flags & f): _flags(((f & Flags::PRE)  ? V : 0) |
                                            ((f & Flags::RW)   ? (R | W) : R) |
                                            ((f & Flags::USR)  ? U : 0) |
                                            ((f & Flags::EX) ? X : 0)) {}
        operator unsigned int() const { return _flags; }

    private:
        unsigned int _flags;

    };

    // Page_Table
    class Page_Table {

    private:
        PT_Entry ptes[PT_ENTRIES];

    public:
        Page_Table() {}

        PT_Entry & operator[](unsigned int i) { return ptes[i]; }

        void map(RV64_Flags flags, int from, int to) {
            Phy_Addr * addr = alloc(to - from);
            if (addr) {
                remap(addr, flags, from , to);
            } else {
                for(; from < to; from++){
                    ptes[from] = phy2pte(alloc(1), flags);

                }
            }
        }
        void remap(Phy_Addr addr, RV64_Flags flags, int from = 0, int to = 512) {
            addr = align_page(addr);
            for( ; from < to; from++) {
                ptes[from] = phy2pte(addr, flags);
                addr += sizeof(Page);
            }
        }
    };

    // Chunk (for Segment)
    class Chunk
    {
    public:
        Chunk() {}

        // from 0 to bytes % PAGE_SIZE
        //pts = number of page_tables
        //pages = number of pages
        Chunk(unsigned int bytes, Flags flags)
        : _from(0), _to(pages(bytes)), _pts(page_tables(_to - _from)), _bytes(bytes), _flags(RV64_Flags(flags)), _pt(calloc(_pts)) {
            _pt->map(_flags, _from, _to);
        }

        ~Chunk() {
            for( ; _from < _to; _from++)
                free((*static_cast<Page_Table *>(phy2log(_pt)))[_from]);
            free(_pt, _pts);
        }

        unsigned int pts() const { return _pts; }
        Page_Table * pt() const { return _pt; }
        unsigned int size() const { return _bytes; }
        int resize(unsigned int amount) { return 0; }

    private:
        unsigned int _from;
        unsigned int _to;
        unsigned int _pts;
        unsigned int _bytes;
        RV64_Flags _flags;
        Page_Table * _pt;
    };

    // Page Directory
    typedef Page_Table Page_Directory;

    // Directory (for Address_Space)
    class Directory
    {
    public:
        Directory() : _pd(calloc(1)) {
            for(unsigned int i = 0; i < PD_ENTRIES; i++){
                (*_pd)[i] = (*_master)[i];
                for (unsigned int j = 0; j < PD_ENTRIES; j++)
                  (*_pd)[i][j] = (*_master)[i][j];
            }
        }
        Directory(Page_Directory * pd) : _pd(pd) {}

        Page_Table * pd() const { return _pd; }

        void activate() const {CPU::pdp(reinterpret_cast<CPU::Reg64>(_pd));}


      //Attach Chunk's PT into the Address Space and return the Page Directory base address.
      Log_Addr attach(const Chunk & chunk, unsigned int lvl2 = 0, unsigned int lvl1 = 0) {
          for(unsigned int i = lvl2; i < PD_ENTRIES; i++)
            for (unsigned int j = lvl1; j < PD_ENTRIES - chunk.pts(); j++)
              if(attach(i, j, chunk.pt(), chunk.pts(), RV64_Flags::V))
                return i << (DIRECTORY_SHIFT_LVL_2) | j << DIRECTORY_SHIFT_LVL_1;
          return false;
      }


      // Used to create non-relocatable segments such as code
      Log_Addr attach(const Chunk & chunk, const Log_Addr & addr) {
          unsigned int lvl2 = directory_lvl_2(addr);
          unsigned int lvl1 = directory_lvl_1(addr);
          if(!attach(lvl2, lvl1, chunk.pt(), chunk.pts(), RV64_Flags::V))
              return Log_Addr(false);
          return lvl2 << (DIRECTORY_SHIFT_LVL_2) | lvl1 << DIRECTORY_SHIFT_LVL_1;
      }

      void detach(const Chunk & chunk) {
          for(unsigned int i = 0; i < PD_ENTRIES; i++) {
            for(unsigned int j = 0; j < PD_ENTRIES; j++)
              if(indexes((*_pd)[i][j]) == indexes(phy2pde(chunk.pt()))) {
                  detach(i, j, chunk.pt(), chunk.pts());
                  return;
              }
          }
          db<MMU>(WRN) << "MMU::Directory::detach(pt=" << chunk.pt() << ") failed!" << endl;
      }

      void detach(const Chunk & chunk, Log_Addr addr) {
          unsigned int lvl2 = directory_lvl_2(addr);
          unsigned int lvl1 = directory_lvl_1(addr);
          if(indexes((*_pd)[lvl2][lvl1]) != indexes(chunk.pt())) {
              db<MMU>(WRN) << "MMU::Directory::detach(pt=" << chunk.pt() << ",addr=" << addr << ") failed!" << endl;
              return;
          }
          detach(lvl1, lvl2, chunk.pt(), chunk.pts());
      }


        Phy_Addr physical(Log_Addr addr) { return addr; }

    private:

        bool attach(unsigned int lvl2, unsigned int lvl1, const Page_Table * pt, unsigned int n, RV64_Flags flags) {
            for(unsigned int i = lvl1; i < lvl1 + n; i++)
                if((*_pd)[lvl2][i])
                    return false;
            for(unsigned int i = lvl1; i < lvl1 + n; i++, pt++)
                (*_pd)[lvl2][i] = phy2pte(Phy_Addr(pt), flags);
            return true;
        }

        void detach(unsigned int lvl2, unsigned int lvl1, Page_Table * pt,  unsigned int n) {
            for (unsigned int i = lvl1; i < lvl1 + n; i++) {
              (*pt)[lvl2][i] = 0;
            }
        }

    private:
        Page_Directory * _pd;
    };

public:
    MMU() {}

    static Phy_Addr alloc(unsigned int bytes = 1) {
        Phy_Addr phy(false);
        if(bytes) {
            List::Element * e = _free.search_decrementing(bytes);
            if(e)
                phy = reinterpret_cast<unsigned long>(e->object()) + e->size() * PAGE_SIZE;
            else
                db<MMU>(ERR) << "MMU::alloc() failed!" << endl;
        }
        db<MMU>(TRC) << "MMU::alloc(bytes=" << bytes << ") => " << phy << endl;

        return phy;
    };

    static Phy_Addr calloc(unsigned int bytes = 1) {
        Phy_Addr phy = alloc(bytes);
        memset(phy2log(phy), 0, bytes*PAGE_SIZE);
        return phy;
    }

    static void free(Phy_Addr addr, unsigned int n = 1) {
        db<MMU>(TRC) << "MMU::free(addr=" << addr << ",n=" << n << ")" << endl;

        assert(Traits<CPU>::unaligned_memory_access || !(addr % 4));

        if(addr && n) {
            List::Element * e = new (addr) List::Element(addr, n);
            List::Element * m1, * m2;
            _free.insert_merging(e, &m1, &m2);
        }
    }

    static unsigned int allocable() { return _free.head() ? _free.head()->size() : 0; }

    //return _master;
    static Page_Directory *volatile current()
    {
        return static_cast<Page_Directory *volatile>(phy2log(CPU::pdp()));
    }


    static void flush_tlb() { CPU::flush_tlb(); }
    static void flush_tlb(Log_Addr addr) { CPU::flush_tlb(addr); }

private:
    static void init();

    static Log_Addr phy2log(const Phy_Addr & phy) { return phy + (PHY_MEM - RAM_BASE); }

    static PD_Entry phy2pde(Phy_Addr bytes) { return ((bytes >> 12) << 10) | RV64_Flags::V; }

    static Phy_Addr pde2phy(PD_Entry entry){ return (entry & ~RV64_Flags::MASK) << 2; }

    static PT_Entry phy2pte(Phy_Addr bytes, RV64_Flags flags) { return ((bytes >> 12) << 10) | flags; }

    static Phy_Addr pte2phy(PT_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }


private:
    static List _free;
    static Page_Directory * _master;
};
__END_SYS

#endif
