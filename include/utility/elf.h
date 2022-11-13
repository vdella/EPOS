// EPOS ELF Utility Declarations

#ifndef __elf_h
#define	__elf_h

#include <system/config.h>
#include "elf-linux.h"

__BEGIN_UTIL

// typedef IF<Traits<CPU>::WORD_SIZE == 32, Elf32_Ehdr, Elf64_Ehdr>::Result Elf_Ehdr;
// typedef IF<Traits<CPU>::WORD_SIZE == 32, Elf32_Addr, Elf64_Addr>::Result Elf_Addr;
// typedef IF<Traits<CPU>::WORD_SIZE == 32, Elf32_Word, Elf64_Word>::Result Elf_Word;
// typedef IF<Traits<CPU>::WORD_SIZE == 32, Elf32_Phdr, Elf64_Phdr>::Result Elf_Phdr;

class ELF: private Elf64_Ehdr
{
public:
    ELF() {}

    // check for ELF magic number
    bool valid() {
        return (e_ident[EI_MAG0] == ELFMAG0) && (e_ident[EI_MAG1] == ELFMAG1)
            && (e_ident[EI_MAG2] == ELFMAG2) && (e_ident[EI_MAG3] == ELFMAG3);
    }

    Elf64_Addr entry() { return e_entry; }

    int segments() { return e_phnum; }

    Elf64_Word segment_type(int i) { return (i > segments()) ? PT_NULL : seg(i)->p_type; }

    Elf64_Addr segment_address(int i) { return (i > segments()) ? 0 : seg(i)->p_align ? seg(i)->p_vaddr: (seg(i)->p_vaddr & ~(seg(i)->p_align - 1)); }

    int segment_size(int i) { return (i > segments()) ? -1 : (int)(((seg(i)->p_offset % seg(i)->p_align) + seg(i)->p_memsz + seg(i)->p_align - 1) & ~(seg(i)->p_align - 1)); }

    int load_segment(int i, Elf64_Addr addr = 0);

private:
    Elf64_Phdr * pht() { return (Elf64_Phdr *)(((char *) this) + e_phoff); }
    Elf64_Phdr * seg(int i) { return &pht()[i];  }
};

__END_UTIL

#endif
