// EPOS Memory Segment Implementation

#include <memory.h>

__BEGIN_SYS


Segment::Segment(unsigned int bytes, const Flags & flags): Chunk(bytes, flags)
{
    db<Segment>(TRC) << "Segment(bytes=" << bytes << ",flags=" << flags << ") [Chunk::_pt=" << Chunk::pt() << "] => " << this << endl;
}


Segment::~Segment()
{
    db<Segment>(TRC) << "~Segment() [Chunk::pt=" << Chunk::pt() << "]" << endl;
}


unsigned int Segment::size() const
{
    return Chunk::size();
}


int Segment::resize(int amount)
{
    db<Segment>(TRC) << "Segment::resize(amount=" << amount << ")" << endl;

    return Chunk::resize(amount);
}

__END_SYS
