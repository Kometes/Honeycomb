// Honeycomb, Copyright (C) 2015 NewGamePlus Inc.  Distributed under the Boost Software License v1.0.

#include "Honey/Thread/Thread.h"
#include "Honey/Thread/Lock/Unique.h"

/** \cond */
namespace honey
{

namespace thread { namespace platform
{
    bool LocalStore::init()
    {
        verify(!pthread_key_create(&_key, nullptr));
        return true;
    }

    LocalStore& LocalStore::create(Thread& thread)
    {
        LocalStore& local = *new LocalStore();
        local.thread = &thread;
        verify(!pthread_setspecific(_key, &local));
        return local;
    }

    void LocalStore::destroy()
    {
        delete_(&inst());
        pthread_setspecific(_key, nullptr);
    }

    LocalStore& LocalStore::inst()
    {
        static auto _ = init(); mt_unused(_);
        
        LocalStore* local = static_cast<LocalStore*>(pthread_getspecific(_key));
        if (!local)
        {
            //externally created thread (eg. main)
            create(*Thread::createExt());
            local = static_cast<LocalStore*>(pthread_getspecific(_key));
        }
        assert(local, "Thread local data not created");
        return *local;
    }

    pthread_key_t LocalStore::_key;
} }

namespace platform
{

Thread::Thread(bool external, int stackSize) :
    _id(threadIdInvalid),
    _stackSize(stackSize),
    _lock(new honey::Mutex)
{
    if (external)
    {
        _handle = pthread_self();
        _id = pthread_mach_thread_np(_handle);
    }
}

//subclass calls move assign
Thread::Thread(Thread&&)                        : _id(threadIdInvalid) {}
Thread::~Thread()                               { finalize(); }

void Thread::finalize()
{
    if (_id != threadIdInvalid) pthread_detach(_handle);
}

Thread& Thread::operator=(Thread&& rhs)
{
    finalize();
    _handle = move(rhs._handle);
    _id = move(rhs._id); rhs._id = threadIdInvalid;
    _stackSize = move(rhs._stackSize);
    _lock = move(rhs._lock);
    return *this;
}

Thread& Thread::current()                       { return *LocalStore::inst().thread; }

void Thread::start()
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, _stackSize);
    verify(!pthread_create(&_handle, &attr, &entry, static_cast<honey::Thread*>(this)));
    pthread_attr_destroy(&attr);
    //wait for entry() to set thread id
    while (_id == threadIdInvalid) { honey::Mutex::Scoped _(*_lock); }
}

void Thread::join()
{
    pthread_join(_handle, nullptr);
    _id = threadIdInvalid;
}

int Thread::priorityNormal()                    { static const int val = (priorityMin() + priorityMax()) / 2; return val; }
int Thread::priorityMin()                       { static const int val = sched_get_priority_min(SCHED_OTHER); return val; }
int Thread::priorityMax()                       { static const int val = sched_get_priority_max(SCHED_OTHER); return val; }
    
void Thread::setPriority(int priority)
{
    sched_param param;
    param.sched_priority = priority;
    verify(!pthread_setschedparam(_handle, SCHED_OTHER, &param));
}
    
int Thread::getPriority() const
{
    int policy;
    sched_param param;
    verify(!pthread_getschedparam(_handle, &policy, &param));
    return param.sched_priority;
}

void* Thread::entry(void* arg)
{
    honey::Thread* thread = static_cast<honey::Thread*>(arg);
    assert(thread);
    thread->_id = pthread_mach_thread_np(pthread_self());
    
    LocalStore::create(*thread);
    thread->entry();
    LocalStore::destroy();
    
    return nullptr;
}

int Thread::concurrency_priv()                  { return pthread_getconcurrency(); }

Thread* Thread::createExt()                     { return new honey::Thread(true, 0); }

} }
/** \endcond */



