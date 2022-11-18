// EPOS Energy-aware Component Management Aspect Program

#ifndef __energy_aware_h
#define __energy_aware_h

#include <framework/id.h>
#include <utility/hash.h>

__BEGIN_SYS

class Energized
{
public:
    static const unsigned int TYPES = LAST_MEDIATOR_ID - FIRST_MEDIATOR_ID;

    typedef Hash<Energized, TYPES, Type_Id> _Hash;
    typedef _Hash::Element Element;

public:
    Energized(const Type_Id & id): _id(id), _mode(OFF), _link(this, id) {
        db<Aspect>(WRN) << "Energized()" << endl;
        _devices.insert(&_link);
    }
    ~Energized() {
        db<Aspect>(WRN) << "~Energized()" << endl;
        _devices.remove(&_link);
    }

    const Power_Mode & mode() { return _mode; }
    Power_Mode mode(const Power_Mode & mode) {
        db<Aspect>(WRN) << "Energized::mode()" << endl;
        Power_Mode old = _mode;
        switch(mode) {
        case ENROLL:
            //            _threaads.insert(Thread::self());
            break;
        case DISMISS:
            //            _threaads.remove(Thread::self());
            break;
        default: // Contest current mode
            _mode = mode;
        }
        return _mode == old ? SAME : _mode;
    }

private:
    Type_Id _id;
    Power_Mode _mode;
//    List<void *> _threads;
    Element _link;

    static _Hash _devices;
};

template<typename Component>
class Energy_Aware
{
protected:
    Energy_Aware() {}

public:
    static const Power_Mode & power() { return _energized.mode(); }
    static Power_Mode power(const Power_Mode & mode) { return _energized.mode(mode); }

private:
    static Energized _energized;
};

template<typename Component>
Energized Energy_Aware<Component>::_energized(Type<Component>::ID);

__END_SYS

#endif
