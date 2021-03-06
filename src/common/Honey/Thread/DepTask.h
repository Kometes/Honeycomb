// Honeycomb, Copyright (C) 2015 NewGamePlus Inc.  Distributed under the Boost Software License v1.0.
#pragma once

#include "Honey/Thread/Future/Util.h"
#include "Honey/Graph/Dep.h"

namespace honey
{

class DepTask;
class DepSched;
/** \cond */
//for weak ptr member var, is_base_of needs a defined class
namespace mt { template<> struct is_base_of<honey::priv::SharedObj_tag, DepTask> : std::true_type {}; }
/** \endcond */

/// Base class of `DepTask_`, can be added to scheduler.  Instances must be created through class `DepTask_`.
class DepTask : public SharedObj<DepTask>, thread::Pool::Task
{
    friend class DepSched;
    
public:
    typedef SharedPtr<DepTask> Ptr;
    typedef function<void ()> Func;
    typedef DepNode<DepTask*> DepNode;
    typedef DepGraph<DepTask::DepNode> DepGraph;

    virtual ~DepTask() {}
    
    /// Get the current task object. Must be called from a task functor.
    static DepTask& current();
    
    /// Check if task is in queue or executing
    bool active() const                             { return _state != State::idle; }
    
    /// Request an interrupt in the executing task's thread. \see Thread::interrupt()
    void interrupt(const Exception::ConstPtr& e = new thread::Interrupted)   { Mutex::Scoped _(_lock); if (_thread) _thread->interrupt(e); }
    /// Check whether an interrupt has been requested for the executing task's thread
    bool interruptRequested()                       { Mutex::Scoped _(_lock); return _thread ? _thread->interruptRequested() : false; }
    
    /// Set task's thread execution scheduling priority. \see Thread::setPriority()
    void setPriority(int priority)                  { Mutex::Scoped _(_lock); _priority = priority; if (_thread) _thread->setPriority(_priority); }
    /// Get task's thread execution scheduling priority. \see Thread::getPriority()
    int getPriority() const                         { return _priority; }
    
    /// Set id used for dependency graph and debug output
    void setId(const Id& id)                        { assert(!_regCount, "Must unregister prior to modifying"); _depNode.setKey(id); }
    const Id& getId() const                         { return _depNode.getKey(); }
    
    /// Get dependency node. Upstream and downstream tasks can be specified through the node.
    /**
      * Out links are 'upstream' tasks that will be completed before this one.
      * In links are 'downstream' tasks that will be completed after this one.
      */ 
    DepNode& deps()                                 { assert(!_regCount, "Must unregister prior to modifying"); return _depNode; }

    /// Get id
    operator const Id&() const                      { return getId(); }
    
    /// Get task info for prepending to a log record
    String info() const;
    
protected:
    enum class State
    {
        idle,           ///< Not active
        queued,         ///< Queued for execution
        depUpWait,      ///< Waiting for upstream tasks (dependency subgraph) to complete
        exec,           ///< Executing functor
        depDownWait     ///< Waiting for downsteam tasks (immediate dependees) to complete
    };
    
    DepTask(const Id& id);

    virtual void exec() = 0;
    virtual void resetFunctor() = 0;
    
    void bindDirty();
    void operator()();
    /// Clean up task after execution
    void finalize_();
    
    virtual void trace(const String& file, int line, const String& msg) const;
    virtual bool traceEnabled() const;

    DepNode             _depNode;
    Mutex               _lock;
    Atomic<State>       _state;
    int                 _regCount;
    DepSched*           _sched;
    WeakPtr<DepTask>    _root;
    int                 _bindId;
    bool                _bindDirty;
    int                 _depUpWaitInit;
    Atomic<int>         _depUpWait;
    int                 _depDownWaitInit;
    Atomic<int>         _depDownWait;
    DepGraph::Vertex*   _vertex;
    bool                _onStack;
    Thread*             _thread;
    int                 _priority;
};

/// Holds a functor and dependency information, enqueue in a scheduler to run the task. \see DepSched
template<class Result>
class DepTask_ : public DepTask
{
public:
    typedef SharedPtr<DepTask_> Ptr;

    /**
      * \param f        functor to execute
      * \param id       used for dependency graph and debug output
      */
    template<class Func>
    DepTask_(Func&& f, const Id& id = idnull)       : DepTask(id), _func(forward<Func>(f)) {}

    /// Get future from which delayed result can be retrieved
    /**
      * \throws future::FutureAlreadyRetrieved      if future() has been called more than once per task execution
      */
    Future<Result> future()                         { return _func.future(); }

    /// Wrapper for DepTask::current()
    static DepTask_& current()                      { return static_cast<DepTask_&>(DepTask::current()); }

private:
    virtual void exec()                             { _func.invoke_delayedReady(); }
    virtual void resetFunctor()                     { _func.setReady(true); }

    PackagedTask<Result ()> _func;
};

/// Scheduler that serializes and parallelizes task execution given a dependency graph of tasks and a pool of threads
/**
  * To run a task, first register it and any dependent tasks with DepSched::reg(), then call DepSched::enqueue(rootTask).
  */
class DepSched
{
    friend class DepTask;
    
public:
    /// Get singleton, uses global future::AsyncSched pool
    static mt_global(DepSched, inst, (future::AsyncSched::inst()));
    
    /**
      * \param pool     Shared ref to thread pool with which all tasks will be enqueued.
      */
    DepSched(thread::Pool& pool);
    
    /// Register a task, linking it into this scheduler's dependency graph
    /**
      * Tasks can be registered with multiple schedulers.
      * \return     false if a task with the same id is already registered
      */
    bool reg(DepTask& task);
    /// Unregister a task.  Returns false if not registered.
    bool unreg(DepTask& task);

    /// Schedule a task for execution.  Returns false if task is already active.
    /**
      * Enqueuing a task performs a `binding`:
      * - the enqueued task becomes a `root` task, and the entire subgraph of upstream tasks (dependencies) are bound to this root
      * - the subgraph of tasks are bound to this scheduler
      *
      * A task can be enqueued again once it is complete. Wait for completion by calling DepTask_::future().get().
      * Be wary of enqueueing tasks that are upstream of other currently active tasks.
      *
      * This method will error if:
      * - `task` is not registered
      * - `task` or any upstream tasks are active
      * - a cyclic dependency is detected
      */
    bool enqueue(DepTask& task);
    
    /// Whether to log task execution flow
    static bool trace;
    
private:
    void bind(DepTask& root);    
    bool enqueue_priv(DepTask& task);
    
    SharedPtr<thread::Pool> _pool;
    Mutex                   _lock;
    vector<DepTask*>        _taskStack;
    DepTask::DepGraph       _depGraph;
    int                     _bindId;
};

inline bool DepTask::traceEnabled() const           { return DepSched::trace; }

}
