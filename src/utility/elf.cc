// EPOS ELF Utility Implementation

#include <architecture/cpu.h>
#include <utility/elf.h>
#include <utility/string.h>

__BEGIN_UTIL

int ELF::load_segment(int i, Elf64_Addr addr)
{
    db<Setup>(TRC) << "ELF::load_segment(i=" << i << ",addr=" << addr << ")" << endl;
    if((i > segments()) || (segment_type(i) != PT_LOAD)) {
        db<Setup>(WRN) << "src " << endl;
        db<Setup>(WRN) << "dst " << endl;
        return 0;
    }

    db<Setup>(TRC) << "src " << segment_offset(i) << endl;
    char * src = reinterpret_cast<char *>(CPU::Reg(this) + seg(i)->p_offset);
    char * dst = reinterpret_cast<char *>((addr) ? addr : segment_address(i));
    db<Setup>(TRC) << "dst " << dst << endl;

    memcpy(dst, src, seg(i)->p_filesz);
    memset(dst + seg(i)->p_filesz, 0, seg(i)->p_memsz - seg(i)->p_filesz);

    db<Setup>(WRN) << "Return Load Segment" << endl;

    return seg(i)->p_memsz;
}

__END_UTIL
