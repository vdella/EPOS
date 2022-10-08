// EPOS-- RISC-V 64 MMU Mediator Declarations

#ifndef __riscv64_mmu_h
#define __riscv64_mmu_h

#include <system/memory_map.h>
#include <utility/string.h>
#include <utility/list.h>
#include <utility/debug.h>
#include <architecture/cpu.h>
#include <architecture/mmu.h>

__BEGIN_SYS

class MMU : public MMU_Common<0, 0, 0>
{
    friend class CPU;

private:
    typedef Grouping_List<Frame> List;

    static const unsigned int MEM_BASE = Memory_Map::RAM_BASE;
    static const unsigned int APP_LOW = Memory_Map::APP_LOW;
    static const unsigned int PHY_MEM = Memory_Map::PHY_MEM;

public:
    class Page_Flags
    {
    public:
        enum
        {
            V = 1 << 0, // Valid
            R = 1 << 1, // Readable
            W = 1 << 2, // Writable
            X = 1 << 3, // Executable
            U = 1 << 4, // User accessible
            G = 1 << 5, // Global (mapped in multiple PTs)
            A = 1 << 6, // Accessed
            D = 1 << 7, // Dirty

            CT = 1 << 8,  // Contiguous (reserved for use by supervisor RSW)
            MIO = 1 << 9, // I/O (reserved for use by supervisor RSW)
            APP = (V | R | W | X),
            SYS = (V | R | W | X),
            IO = (SYS | MIO),
            DMA = (SYS | CT),
            MASK = (1 << 10) - 1
        };

    public:
        Page_Flags() {}
        Page_Flags(unsigned int f) : _flags(f) {}
        Page_Flags(Flags f) : _flags(V | R | X |
                                     ((f & Flags::RW) ? W : 0) |
                                     ((f & Flags::USR) ? U : 0) |
                                     ((f & Flags::CWT) ? 0 : 0) |
                                     ((f & Flags::CD) ? 0 : 0) |
                                     ((f & Flags::CT) ? CT : 0) |
                                     ((f & Flags::IO) ? IO : 0)) {}

        operator unsigned int() const { return _flags; }

        friend Debug &operator<<(Debug &db, const Page_Flags &f)
        {
            db << hex << f._flags;
            return db;
        }

    private:
        unsigned int _flags;
    };

    class Page_Table
    {
    public:
        Page_Table() {}

        PT_Entry &operator[](unsigned int i) { return _entry[i]; }

        void map(int from, int to, Page_Flags flags, Color color)
        {
            Phy_Addr *addr = alloc(to - from, color);
            if (addr)
                remap(addr, from, to, flags);
            else
                for (; from < to; from++)
                {
                    Log_Addr *pte = phy2log(&_entry[from]);
                    *pte = phy2pte(alloc(1, color), flags);
                }
        }

        void map_contiguous(int from, int to, Page_Flags flags, Color color)
        {
            remap(alloc(to - from, color), from, to, flags);
        }

        void remap(Phy_Addr addr, int from, int to, Page_Flags flags)
        {
            addr = align_page(addr);  // Faz sentido? Páginas são desalinhadas, não?
            for (; from < to; from++)
            {
                Log_Addr *pte = phy2log(&_entry[from]);
                *pte = phy2pte(addr, flags);
                addr += sizeof(Page);
            }
        }

        void unmap(int from, int to)
        {
            for (; from < to; from++)
            {
                free(_entry[from]);
                Log_Addr *tmp = phy2log(&_entry[from]);
                *tmp = 0;
            }
        }

        friend OStream &operator<<(OStream &os, Page_Table &pt)
        {
            os << "{\n";
            int brk = 0;
            for (unsigned int i = 0; i < PT_ENTRIES; i++)
                if (pt[i])
                {
                    os << "[" << i << "]=" << reinterpret_cast<void *>((pt[i] & ~Page_Flags::MASK) << 2 | (pt[i] & Page_Flags::MASK)) << "  ";
                    if (!(++brk % 4))
                        os << "\n";
                }
            os << "\n}";
            return os;
        }

    private:
        PT_Entry _entry[PT_ENTRIES];
    }

    // Chunk (for Segment)
    class Chunk
    {
    public:
        Chunk() {}
        Chunk(unsigned int bytes, Flags flags) : _phy_addr(alloc(bytes)), _bytes(bytes), _flags(flags) {}
        Chunk(Phy_Addr phy_addr, unsigned int bytes, Flags flags) : _phy_addr(phy_addr), _bytes(bytes), _flags(flags) {}

        ~Chunk() { free(_phy_addr, _bytes); }

        unsigned int pts() const { return _pts; }
        Flags flags() const { return _flags; }
        Page_Table *pt() const { return _pt; }
        unsigned int size() const { return (_to - _from) * sizeof(Page) ; }
        Phy_Addr phy_address() const { return _phy_addr; } // always CT
        int resize(unsigned int amount) { return 0; }      // no resize in CT

    private:
        unsigned int _from;
        unsigned int _to;
        unsigned int _pts;
        Page_Flags _flags;
        Page_Table * _pt;
    };

    // Page Directory
    typedef Page_Table Page_Directory;

    // Directory (for Address_Space)
    class Directory
    {
    public:
        Directory() {}
        Directory(Page_Directory *pd) {}

        Page_Table *pd() const { return 0; }

        void activate() {}

        Log_Addr attach(const Chunk &chunk) { return chunk.phy_address(); }
        Log_Addr attach(const Chunk &chunk, Log_Addr addr) { return (addr == chunk.phy_address()) ? addr : Log_Addr(false); }
        
        void detach(const Chunk &chunk) {}
        void detach(const Chunk &chunk, Log_Addr addr) {}

        Phy_Addr physical(Log_Addr addr) { return addr; }
    };

    // DMA_Buffer (straightforward without paging)
    class DMA_Buffer : public Chunk
    {
    public:
        DMA_Buffer(unsigned int s) : Chunk(s, Flags::CT)
        {
            db<MMU>(TRC) << "MMU::DMA_Buffer() => " << *this << endl;
        }

        Log_Addr log_address() const { return phy_address(); }

        friend Debug &operator<<(Debug &db, const DMA_Buffer &b)
        {
            db << "{phy=" << b.phy_address()
               << ",log=" << b.log_address()
               << ",size=" << b.size()
               << ",flags=" << b.flags() << "}";
            return db;
        }
    };

public:
    MMU() {}

    static Phy_Addr alloc(unsigned int bytes = 1)
    {
        Phy_Addr phy(false);

        if (bytes)
        {
            List::Element *e = _free.search_decrementing(bytes);
            if (e)
                phy = Phy_Addr(e->object()) + e->size();
            else
                db<MMU>(ERR) << "MMU::alloc() failed!" << endl;
        }
        db<MMU>(TRC) << "MMU::alloc(bytes=" << bytes << ") => " << phy << endl;

        return phy;
    };

    static Phy_Addr calloc(unsigned int frames = 1)
    {
        Phy_Addr phy = alloc(frames);
        memset(phy2log(phy), 0, frames);
        return phy;
    }

    static void free(Phy_Addr frame, unsigned int n = 1)
    {
        frame = indexes(frame);  // Cleans up MMU flags in frame address

        db<MMU>(TRC) << "MMU::free(addr=" << frame << ",n=" << n << ")" << endl;

        // Free blocks must be large enough to contain a list element
        assert(n > sizeof(List::Element));

        if (frame && n)
        {
            List::Element * e = new (phy2log(frame)) List::Element(frame, n);
            List::Element * m1, * m2;
            _free.insert_merging(e, &m1, &m2);
        }
    }

    static unsigned int allocable() { return _free.head() ? _free.head()->size() : 0; }

    static Page_Directory *volatile current() { 
        //pdp() é satp deslocado para ficar 64.
        return static_cast<Page_Directory *volatile>(phy2log(CPU::pdp()));
    }

    static Phy_Addr physical(Log_Addr addr)
    {
        Page_Directory *pd = current();
        Page_Table *pt = (*pd)[directory(addr)];
        return (*pt)[page(addr)] | offset(addr);
    }

    // Retiro os 2 bit que o offset tem a mais, coloco as flags e adiciono a parte reservada
    // static PT_Entry phy2pte(Phy_Addr frame, Page_Flags flags) { return (frame >> 2) | flags; }
    static PT_Entry phy2pte(Phy_Addr frame, Page_Flags flags)
    {
        return ((frame >> 2) | flags) << 10;
        // ou return (1 << 10 | frame >> 2 | flags)
    }

    // Retiro os Bits reservados, pega os PPN e relaciona direto com os PPN do PA, transforma as flags no offset e desloca 2 para chegar no tamanho certo
    //  static Phy_Addr pte2phy(PT_Entry entry) { return (entry & ~Page_Flags::MASK) << 2; }
    static Phy_Addr pte2phy(PT_Entry entry)
    {
        return ((entry >> 10) & ~Page_Flags::MASK) << 2;
    }

    // | -> Junta
    // & -> Separa

    // static PD_Entry phy2pde(Phy_Addr frame) { return (frame >> 2) | Page_Flags::V; }
    // TODO Verificar
    static PD_Entry phy2pde(Phy_Addr frame) { return ((frame >> 2) | Page_Flags::V) << 10 ; }

    // static Phy_Addr pde2phy(PD_Entry entry) { return (entry & ~Page_Flags::MASK) << 2; }
    //TODO Verificar
    static Phy_Addr pde2phy(PD_Entry entry){ return ((entry >> 10) & ~Page_Flags::MASK) << 2; }

    static void flush_tlb()  // Invalidates both TLB1 and unified TLB2.
    {
        CPU::flush_tlb();
    }

    static void flush_tlb(Log_Addr addr)
    {
        CPU::flush_tlb(Reg(addr)); // TODO check how to convert addr to Reg properly.
    }

private:
    static void init();

    static Log_Addr phy2log(Phy_Addr phy)
    {
        // TODO check.
        return Log_Addr((MEM_BASE == PHY_MEM) ? phy : (MEM_BASE > PHY_MEM) ? phy - (MEM_BASE - PHY_MEM)
                                                                           : phy + (PHY_MEM - MEM_BASE));
    }

    //log2phy Precisa?

private:
    static List _free;
    // static Page_Directory * _master;
};

// class MMU: public IF<Traits<System>::multitask, RV32S_MMU, No_MMU>::Result {};

__END_SYS

#endif