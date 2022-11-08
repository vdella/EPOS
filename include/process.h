// EPOS Thread Component Declarations

#ifndef __process_h
#define __process_h

#include <architecture.h>
#include <machine.h>
#include <utility/queue.h>
#include <utility/handler.h>
#include <scheduler.h>

extern "C"
{
    void __exit();
}

__BEGIN_SYS

class Thread
{
    friend class Init_End;            // context->load()
    friend class Init_System;         // for init() on CPU != 0
    friend class Scheduler<Thread>;   // for link()
    friend class Synchronizer_Common; // for lock() and sleep()
    friend class Alarm;               // for lock()
    friend class System;              // for init()
    friend class IC;                  // for link() for priority ceiling

protected:
    static const bool preemptive = Traits<Thread>::Criterion::preemptive;
    static const bool reboot = Traits<System>::reboot;

    static const unsigned int QUANTUM = Traits<Thread>::QUANTUM;
    static const unsigned int STACK_SIZE = Traits<Application>::STACK_SIZE;

    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Context Context;

public:
    // Thread State
    enum State
    {
        RUNNING,
        READY,
        SUSPENDED,
        WAITING,
        FINISHING
    };

    // Thread Scheduling Criterion
    typedef Traits<Thread>::Criterion Criterion;
    enum
    {
        HIGH = Criterion::HIGH,
        NORMAL = Criterion::NORMAL,
        LOW = Criterion::LOW,
        MAIN = Criterion::MAIN,
        IDLE = Criterion::IDLE
    };

    // Thread Queue
    typedef Ordered_Queue<Thread, Criterion, Scheduler<Thread>::Element> Queue;

    // Thread Configuration
    struct Configuration
    {
        Configuration(const State &s = READY, const Criterion &c = NORMAL, unsigned int ss = STACK_SIZE)
            : state(s), criterion(c), stack_size(ss) {}

        State state;
        Criterion criterion;
        unsigned int stack_size;
    };

public:
    template <typename... Tn>
    Thread(int (*entry)(Tn...), Tn... an);
    template <typename... Tn>
    Thread(const Configuration &conf, int (*entry)(Tn...), Tn... an);
    ~Thread();

    const volatile State &state() const { return _state; }
    const volatile Criterion::Statistics &statistics() { return criterion().statistics(); }

    const volatile Criterion &priority() const { return _link.rank(); }
    void priority(const Criterion &p);

    int join();
    void pass();
    void suspend();
    void resume();

    static Thread *volatile self() { return running(); }
    static void yield();
    static void exit(int status = 0);

protected:
    void constructor_prologue(unsigned int stack_size);
    void constructor_epilogue(Log_Addr entry, unsigned int stack_size);

    Criterion &criterion() { return const_cast<Criterion &>(_link.rank()); }
    Queue::Element *link() { return &_link; }

    static Thread *volatile running() { return _scheduler.chosen(); }

    static void lock() { CPU::int_disable(); }
    static void unlock() { CPU::int_enable(); }
    static bool locked() { return CPU::int_disabled(); }

    static void sleep(Queue *q);
    static void wakeup(Queue *q);
    static void wakeup_all(Queue *q);

    static void reschedule();
    static void time_slicer(IC::Interrupt_Id interrupt);

    static void dispatch(Thread *prev, Thread *next, bool charge = true);

    static int idle();

private:
    static void init();

protected:
    char *_stack;
    Context *volatile _context;
    volatile State _state;
    Queue *_waiting;
    Thread *volatile _joining;
    Queue::Element _link;

    volatile Task * _task;

    static volatile unsigned int _thread_count;
    static Scheduler_Timer *_timer;
    static Scheduler<Thread> _scheduler;
};

template <typename... Tn>
inline Thread::Thread(int (*entry)(Tn...), Tn... an)
    : _state(READY), _waiting(0), _joining(0), _link(this, NORMAL)
{
    constructor_prologue(STACK_SIZE);
    _context = CPU::init_stack(0, _stack + STACK_SIZE, &__exit, entry, an...);
    constructor_epilogue(entry, STACK_SIZE);
}

template <typename... Tn>
inline Thread::Thread(const Configuration &conf, int (*entry)(Tn...), Tn... an)
    : _state(conf.state), _waiting(0), _joining(0), _link(this, conf.criterion)
{
    constructor_prologue(conf.stack_size);
    _context = CPU::init_stack(0, _stack + conf.stack_size, &__exit, entry, an...);
    constructor_epilogue(entry, conf.stack_size);
}

// A Java-like Active Object
class Active : public Thread
{
public:
    Active() : Thread(Configuration(Thread::SUSPENDED), &entry, this) {}
    virtual ~Active() {}

    virtual int run() = 0;

    void start() { resume(); }

private:
    static int entry(Active *runnable) { return runnable->run(); }
};

// An event handler that triggers a thread (see handler.h)
class Thread_Handler : public Handler
{
public:
    Thread_Handler(Thread *h) : _handler(h) {}
    ~Thread_Handler() {}

    void operator()() { _handler->resume(); }

private:
    Thread *_handler;
};

class Task
{
private:
    static const bool multitask = Traits<System>::multitask;
    typedef CPU::Log_Addr Log_Addr;

public:
    static volatile Task *_active;  // Why?
    Heap *_heap;
    bool has_idle;

    Task(Segment *cs, Segment *ds)
        : addr_space(new (SYSTEM) Address_Space), 
          code_segment(cs), 
          data_segment(ds), 
          code_address(addr_space->attach(code_segment, Memory_Map::APP_CODE)), 
          data_address(addr_space->attach(data_segment, Memory_Map::APP_DATA))
    {
        db<Task>(TRC) << "Task(as=" << addr_space << ",cs=" << code_segment << ",ds=" << data_segment << ",code=" << code_address << ",data=" << data_address << ") => " << this << endl;

        has_idle = false;
    }

    Task(Address_Space *as, Segment *cs, Segment *ds)
        : addr_space(as), 
          code_segment(cs), 
          data_segment(ds), 
          code_address(addr_space->attach(code_segment, Memory_Map::APPcode_address)), 
          data_address(addr_space->attach(data_segment, Memory_Map::APPdata_address))
    {
        db<Task>(TRC) << "Task(as=" << addr_space << ",cs=" << code_segment << ",ds=" << data_segment << ",code=" << code_address << ",data=" << data_address << ") => " << this << endl;

        has_idle = false;
    }

    ~Task()
    {
        addr_space->detach(code_segment, Memory_Map::APP_CODE);
        addr_space->detach(data_segment, Memory_Map::APP_DATA);

        delete code_segment;
        delete data_segment;
        
        delete addr_space;
    }

    static void activate(volatile Task *t)
    {
        Task::_active = t;
        t->addr_space->activate();
    }

    static unsigned int get_active_pd()
    {
        return Task::_active->addr_space->pd();
    }

    Address_Space *address_space() const { return addr_space; }

    Segment *code_segment() const { return code_segment; }
    Segment *data_segment() const { return data_segment; }

    Log_Addr code() const { return code_address; }
    Log_Addr data() const { return data_address; }

private:
    Address_Space *addr_space;

    Segment *code_segment;
    Segment *data_segment;

    Log_Addr code_address;
    Log_Addr data_address;
};

__END_SYS

#endif
