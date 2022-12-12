// EPOS Component Framework - Component Stub

// Stub selectively binds the Handle either to the component's scenario Adapter or to its Proxy.
// Proxies are used in the kernel mode or when a component is subject to the Remote Aspect program.

#ifndef __stub_h
#define __stub_h

#include "adapter.h"
#include "proxy.h"

__BEGIN_SYS

template<typename Component, bool remote>
class Stub: public Adapter<Component>
{
public:
    template<typename ... Tn>
    Stub(const Tn & ... an): Adapter<Component>(an ...) {}

    // Dereferencing handles for constructors that take pointers to system objects
    template<typename ... Tn>
    Stub(const Stub<Segment, remote> & cs, const Stub<Segment, remote> & ds, const Tn & ... an): Proxy<Component>(cs.id().unit(), ds.id().unit(), an ...) {}
    template<typename ... Tn>
    Stub(const Stub<Task, remote> & t, const Tn & ... an): Proxy<Component>(t.id().unit(), an ...) {}

    ~Stub() {}

    static Stub * share(const Id & id) { return reinterpret_cast<Stub *>(Adapter<Component>::share(id)); }
    static Stub * share(Adapter<Component> * adapter) { return reinterpret_cast<Stub *>(Adapter<Component>::share(adapter)); }
};

template<typename Component>
class Stub<Component, true>: public Proxy<Component>
{
public:
    template<typename ... Tn>
    Stub(const Tn & ... an): Proxy<Component>(an ...) {}

    // Dereferencing handles for constructors that take pointers to system objects
    template<typename ... Tn>
    Stub(const Stub<Segment, true> & cs, const Stub<Segment, true> & ds, const Tn & ... an): Proxy<Component>(cs.id().unit(), ds.id().unit(), an ...) {}
    template<typename ... Tn>
    Stub(const Stub<Task, true> & t, const Tn & ... an): Proxy<Component>(t.id().unit(), an ...) {}

    ~Stub() {}

    static Stub * share(const Id & id) { return reinterpret_cast<Stub *>(Proxy<Component>::share(id)); }
    static Stub * share(Adapter<Component> * proxy) { return reinterpret_cast<Stub *>(Proxy<Component>::share(proxy->id())); }
};

__END_SYS

#endif
