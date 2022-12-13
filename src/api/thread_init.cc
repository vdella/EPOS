// EPOS Thread Initialization

#include <machine/timer.h>
#include <machine/ic.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

extern "C" { void __epos_app_entry(); }

void Thread::init()
{
    db<Init, Thread>(TRC) << "Thread::init()" << endl;

    // typedef int (Main)(int argc, char * argv[]);
    typedef int (Main)();

    System_Info * si = System::info();
    Main * main;

    if(Traits<System>::multitask)
        main = reinterpret_cast<Main *>(si->lm.app_entry);
    else
        // If EPOS is a library, then adjust the application entry point to __epos_app_entry, which will directly call main().
        // In this case, _init will have already been called, before Init_Application to construct MAIN's global objects.
        main = reinterpret_cast<Main *>(__epos_app_entry);

    // CPU::smp_barrier();

    Criterion::init();

    if(Traits<System>::multitask) {
      // Address_Space * as = new (SYSTEM) Address_Space(MMU::current());
      Segment * cs = new ((void*)Memory_Map::APP_CODE) Segment(64 * 1024, MMU::RV64_Flags::UA);
      Segment * ds = new ((void*)Memory_Map::APP_DATA) Segment(64 * 1024, MMU::RV64_Flags::UD);
      Task * app_task =  new (SYSTEM) Task(cs, ds);

      db<Setup>(TRC) << "app_task = " << hex << app_task << endl;
      Task::activate(app_task);

      new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), reinterpret_cast<Main *>(main));

      // We need to be in the AS of the first thread.
      db<Init, Thread>(TRC) << "Task::activate()" << endl;
      Task::activate(Thread::self()->_task);

    } else {
      // If EPOS is a library, then adjust the application entry point to __epos_app_entry,
      // which will directly call main(). In this case, _init will already have been called,
      // before Init_Application to construct MAIN's global objects.
      new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), reinterpret_cast<int (*)()>(main));
    }

    // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
    new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);

    // CPU::smp_barrier();

    // The installation of the scheduler timer handler does not need to be done after the
    // creation of threads, since the constructor won't call reschedule() which won't call
    // dispatch that could call timer->reset()
    // Letting reschedule() happen during thread creation is also harmless, since MAIN is
    // created first and dispatch won't replace it nor by itself neither by IDLE (which
    // has a lower priority)
    if(Criterion::timed && (CPU::id() == 0))
        _timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);

    // No more interrupts until we reach init_end
    CPU::int_disable();

    // Transition from CPU-based locking to thread-based locking
    // CPU::smp_barrier();
    This_Thread::not_booting();
}

__END_SYS
