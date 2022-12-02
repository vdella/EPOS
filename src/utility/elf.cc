// EPOS ELF Utility Implementation

#include <architecture/cpu.h>
#include <utility/elf.h>
#include <utility/string.h>

__BEGIN_UTIL

int ELF::load_segment(int i, Elf64_Addr addr)
{
  db<Setup>(WRN) << "Load Segment" << endl;

    if((i > segments()) || (segment_type(i) != PT_LOAD))
        return 0;

    db<Setup>(WRN) << "Segment Valid" << endl;

    char * src = reinterpret_cast<char *>(CPU::Reg(this) + seg(i)->p_offset);
    char * dst = reinterpret_cast<char *>((addr) ? addr : segment_address(i));

    db<Setup>(WRN) << "src" << endl;


    memcpy(dst, src, seg(i)->p_filesz);
    db<Setup>(WRN) << "after memcpy" << endl;

    memset(dst + seg(i)->p_filesz, 0, seg(i)->p_memsz - seg(i)->p_filesz);

    db<Setup>(WRN) << "Return Load Segment" << endl;

    return seg(i)->p_memsz;
}

__END_UTIL
