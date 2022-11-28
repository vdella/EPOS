// EPOS RISC V Initialization

#include <machine.h>

__BEGIN_SYS

void Machine::pre_init(System_Info * si)
{
    // Adjust stvec to point to _int_entry's logical address
    if(Traits<System>::multitask)
        CLINT::stvec(CLINT::DIRECT, &IC::entry);
    else
        CLINT::mtvec(CLINT::DIRECT, &IC::entry);

    Display::init();

    db<Init, Machine>(TRC) << "Machine::pre_init()" << endl;
}


void Machine::init()
{
    db<Init, Machine>(TRC) << "Machine::init()" << endl;

    if(Traits<IC>::enabled)
        IC::init();

    if(Traits<Timer>::enabled)
        Timer::init();
}

__END_SYS
