// EPOS Thread Initialization

#include <utility/elf.h>
#include <architecture/mmu.h>
#include <machine/timer.h>
#include <machine/ic.h>
#include <system.h>
#include <process.h>
#include <memory.h>


__BEGIN_SYS

extern "C" { void __epos_app_entry(); }

void Thread::init()
{
    db<Init, Thread>(WRN) << "Thread::init()" << endl;
    typedef int (Main)();
    System_Info * si = System::info();

    if(Traits<System>::multitask) {
        char * bi = reinterpret_cast<char*>(Memory_Map::RAM_BASE);

        for(unsigned i = 0; i < si->bm.n_apps; i++) {
             // We need W permission to load the segment
            Segment * code_seg = new (SYSTEM) Segment(64*4096, MMU::RV64_Flags::UA);
            Segment * data_seg = new (SYSTEM) Segment(64*4096, MMU::RV64_Flags::UA);
            Task * app_task =  new (SYSTEM) Task(code_seg, data_seg);

            db<Setup>(TRC) << "app_task = " << hex << app_task << endl;
            Task::activate(app_task);

            if(si->lm.has_app) {
                ELF * app_elf = reinterpret_cast<ELF *>(&bi[si->bm.application_offset[i]]);
                db<Setup>(TRC) << "Setup::load_app()" << endl;
                if(app_elf->load_segment(0) < 0) {
                    db<Setup>(ERR) << "Application code segment was corrupted during INIT!" << endl;
                    Machine::panic();
                }
                for(int i = 1; i < app_elf->segments(); i++)
                    if(app_elf->load_segment(i) < 0) {
                        db<Setup>(ERR) << "Application data segment was corrupted during INIT!" << endl;
                        Machine::panic();
                    }
            }

            new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), reinterpret_cast<Main *>(si->lm.app[i].app_entry));
        }

        // We need to be in the AS of the first thread.
        Task::activate(Thread::self()->_task);
    }

    // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
    new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);

    // The installation of the scheduler timer handler does not need to be done after the
    // creation of threads, since the constructor won't call reschedule() which won't call
    // dispatch that could call timer->reset()
    // Letting reschedule() happen during thread creation is also harmless, since MAIN is
    // created first and dispatch won't replace it nor by itself neither by IDLE (which
    // has a lower priority)
    if(Criterion::timed)
        _timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);

    // No more interrupts until we reach init_first
    CPU::int_disable();

    // Transition from CPU-based locking to thread-based locking
    This_Thread::not_booting();
}

__END_SYS
