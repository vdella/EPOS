// EPOS Thread Initialization

#include <machine/timer.h>
#include <machine/ic.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

extern "C"
{
    void __epos_app_entry();
}

void Thread::init()
{
    db<Init, Thread>(TRC) << "Thread::init()" << endl;

    Criterion::init();

    typedef int(Main)();

    System_Info *si = System::info();

    if (Traits<System>::multitask)
    {
        char *bi = reinterpret_cast<char *>(Memory_Map::MEM_BASE);

        for (unsigned i = 0; i < si->bm.n_apps; i++)
        {
            // We need W permission to load the segment
            Segment *code_seg = new (SYSTEM) Segment(si->lm.app[i].app_code_size, MMU::Flags::ALL);
            Segment *data_seg = new (SYSTEM) Segment(8 * 1024 * 1024, MMU::Flags::UDATA);
            Task *app_task = new (SYSTEM) Task(code_seg, data_seg);

            db<Setup>(TRC) << "app_task = " << app_task << endl;
            Task::activate(app_task);

            Task::_active->_heap = reinterpret_cast<Heap *>(Memory_Map::APP_HEAP);

            if (si->lm.has_app)
            {
                ELF *app_elf = reinterpret_cast<ELF *>(&bi[si->bm.application_offset[i]]);
                db<Setup>(TRC) << "Setup_SifiveE::load_app()" << endl;
                if (app_elf->load_segment(0) < 0)
                {
                    db<Setup>(ERR) << "Application code segment was corrupted during INIT!" << endl;
                    Machine::panic();
                }
                for (int j = 1; j < app_elf->segments(); j++)
                    if (app_elf->load_segment(j) < 0)
                    {
                        db<Setup>(ERR) << "Application data segment was corrupted during INIT!" << endl;
                        Machine::panic();
                    }
            }

            // Delegate MAIN thread.
            new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), reinterpret_cast<Main *>(si->lm.app[i].app_entry));
        }

        // We need to be in the Address_Space of the first thread.
        Task::activate(Thread::self()->_task);
    }

    // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
    Thread *idle = new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), Thread::idle);
    idle->_context->_st |= CPU::SPP_S;

    // The installation of the scheduler timer handler does not need to be done after the
    // creation of threads, since the constructor won't call reschedule() which won't call
    // dispatch that could call timer->reset()
    // Letting reschedule() happen during thread creation is also harmless, since MAIN is
    // created first and dispatch won't replace it nor by itself neither by IDLE (which
    // has a lower priority)
    if (Criterion::timed)
        _timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);

    // No more interrupts until we reach init_first
    CPU::int_disable();

    // Transition from CPU-based locking to thread-based locking
    This_Thread::not_booting();
}

__END_SYS