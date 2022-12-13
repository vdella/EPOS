// EPOS System Initializer

#include <utility/random.h>
#include <machine.h>
#include <memory.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

class Init_System
{
private:
    static const unsigned int HEAP_SIZE = Traits<System>::HEAP_SIZE;

public:
    Init_System() {
        db<Init>(TRC) << "Init_System()" << endl;

        db<Init>(INF) << "Init:si=" << *System::info() << endl;

        db<Init>(WRN) << "Initializing the architecture: " << endl;
        CPU::init();

        db<Init>(WRN) << "Initializing system's heap: " << endl;
        if(Traits<System>::multiheap) {
            db<Init>(INF) << "MULTIHEAP: Initializing system's heap: " << endl;
            db<Init>(WRN) << "Master Address: " << MMU::current() << endl;

            System::_heap_segment = new (&System::_preheap[0]) Segment(HEAP_SIZE, Segment::Flags::SYS);
            char * heap;
            db<Init>(INF) << "Heap Segment: " << System::_heap_segment << endl;

            if(Memory_Map::SYS_HEAP == Traits<Machine>::NOT_USED)
                heap = Address_Space(MMU::current()).attach(System::_heap_segment);
            else
               {
                heap = Address_Space(MMU::current()).attach(System::_heap_segment, Memory_Map::SYS_HEAP);
                // db<Init>(INF) << *MMU::current() << endl;
                MMU::show_table(Memory_Map::SYS_HEAP);
                asm("break:");
               }
            if(!heap)
                db<Init>(ERR) << "Failed to initialize the system's heap!" << endl;
            System::_heap = new (&System::_preheap[sizeof(Segment)]) Heap(heap, System::_heap_segment->size());
            db<Init>(WRN) << "Heap: " << heap << endl;

        } else {
          db<Init>(INF) << "MONOHEAP: Initializing system's heap: " << endl;
          db<Init>(INF) << "Multieap: " << Traits<System>::multiheap << endl;
          System::_heap = new (&System::_preheap[0]) Heap(MMU::alloc(MMU::pages(HEAP_SIZE)), HEAP_SIZE);

        }
        db<Init>(INF) << "Heap Size: " << HEAP_SIZE << " | Pages : " << MMU::pages(HEAP_SIZE) << endl;
        db<Init>(WRN) << "Initializing the machine: " << endl;
        Machine::init();

        db<Init>(INF) << "Initializing system abstractions: " << endl;
        System::init();

        // Randomize the Random Numbers Generator's seed
        if(Traits<Random>::enabled) {
            db<Init>(INF) << "Randomizing the Random Numbers Generator's seed." << endl;
            if(Traits<TSC>::enabled)
                Random::seed(TSC::time_stamp());

            if(!Traits<TSC>::enabled)
                db<Init>(WRN) << "Due to lack of entropy, Random is a pseudo random numbers generator!" << endl;
        }

        // Initialization continues at init_end
    }
};

// Global object "init_system" must be constructed first.
Init_System init_system;

__END_SYS
