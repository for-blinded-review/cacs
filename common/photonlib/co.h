#pragma once
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <atomic>
#include "utility.h"
#include "xmmintrin.h"
#include "callback.h"

namespace photon
{
    int init();
    int fini();

    struct thread;
    extern thread* CURRENT;

    enum states
    {
        READY = 0,      // ready to run
        RUNNING = 1,    // CURRENTly running
        WAITING = 2,    // waiting for some events
        DONE = 4,       // finished the whole life-cycle
    };

    typedef void (*defer_func)(void*);
    typedef void* (*thread_entry)(void*);
    const uint64_t DEFAULT_STACK_SIZE = 8 * 1024 * 1024;
    thread* thread_create(thread_entry start, void* arg,
                          uint64_t stack_size = DEFAULT_STACK_SIZE);

    // switching to other threads (without going into sleep queue)
#if defined(NEW) || defined(CACSTH)
    __attribute__((preserve_none))
#endif
    void thread_yield();

    // Threads are join-able *only* through their join_handle.
    // Once join is enabled, the thread will remain existing until being joined.
    // Failing to do so will cause resource leak.
    struct join_handle;
    join_handle* thread_enable_join(thread* th, bool flag = true);
    void thread_join(join_handle* jh);

#if defined(NEW) || defined(CACSTH)
    __attribute__((preserve_none))
#endif
    int thread_usleep(uint64_t useconds);
#if defined(NEW) || defined(CACSTH)
    __attribute__((preserve_none))
#endif
    void thread_interrupt(thread* th, int error_number);
    // switching to a specific thread, which must be RUNNING
    void thread_yield_to(thread* th);

    // suspend CURRENT thread for specified time duration, and switch
    // control to other threads, resuming possible sleepers
    int thread_usleep(uint64_t useconds);
    static inline int thread_sleep(uint64_t seconds)
    {
        const uint64_t max_seconds = ((uint64_t)-1) / 1000 / 1000;
        uint64_t usec = (seconds >= max_seconds ? -1 :
                        seconds * 1000 * 1000);
        return thread_usleep(usec);
    }

    static inline void thread_suspend()
    {
        thread_usleep(-1);
    }

    states thread_stat(thread* th = CURRENT);
    void thread_interrupt(thread* th, int error_number = EINTR);
    static inline void thread_resume(thread* th)
    {
        thread_interrupt(th, 0);
    }

    // if true, the thread `th` should cancel what is doing, and quit
    // current job ASAP (not allowed `th` to sleep or block more than
    // 10ms, otherwise -1 will be returned to `th` and errno == EPERM;
    // if it is currently sleeping or blocking, it is thread_interupt()ed
    // with EPERM)
    int thread_shutdown(thread* th, bool flag = true);

    // the getter and setter of thread-local variable
    // getting and setting local in a timer context will cause undefined behavior!
    void* thread_get_local();
    void  thread_set_local(void* local);

    class spinlock {
    public:
        int lock();
        int try_lock();
        void unlock();
    protected:
        std::atomic_bool _lock = {false};
    };

    class waitq
    {
    public:
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        int wait(uint64_t timeout = -1);
        void resume(thread* th);            // `th` must be waiting in this waitq!
        void resume_one()
        {
            if (q)
                resume(q);
        }
        waitq() = default;
        waitq(const waitq& rhs) = delete;   // not allowed to copy construct
        waitq(waitq&& rhs)
        {
            q = rhs.q;
            rhs.q = nullptr;
        }
        ~waitq()
        {
            assert(q == nullptr);
        }

    protected:
        thread* q = nullptr;                // the first thread in queue, if any
    };

    class mutex : protected waitq
    {
    public:
        void unlock();
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        int lock(uint64_t timeout = -1);        // threads are guaranteed to get the lock
        int try_lock(uint64_t timeout = -1)     // in FIFO order, when there's contention
        {
            return owner ? -1 : lock();
        }
        ~mutex()
        {
            assert(owner == nullptr);
        }

    protected:
        thread* owner = nullptr;
    };

    template<typename M0>
    class locker
    {
    public:
        using M1 = typename std::decay<M0>::type;
        using M  = typename std::remove_pointer<M1>::type;

        // do lock() if `do_lock` > 0, and lock() can NOT fail if `do_lock` > 1
        explicit locker(M* mutex, uint64_t do_lock = 2) : m_mutex(mutex)
        {
            if (do_lock > 0) {
                lock(do_lock > 1);
            } else {
                m_locked = false;
            }
        }
        explicit locker(M& mutex, uint64_t do_lock = 2) : locker(&mutex, do_lock) { }
        locker(locker&& rhs) : m_mutex(rhs.m_mutex)
        {
            m_locked = rhs.m_locked;
            rhs.m_mutex = nullptr;
            rhs.m_locked = false;
        }
        locker(const locker& rhs) = delete;
        int lock(bool must_lock = true)
        {
            int ret; do
            {
                ret = m_mutex->lock();
                m_locked = (ret == 0);
            } while (!m_locked && must_lock);
            return ret;
        }
        int try_lock(){
            auto ret = m_mutex->try_lock();
            m_locked = (ret == 0);
            return ret;
        };
        bool locked()
        {
            return m_locked;
        }
        operator bool()
        {
            return locked();
        }
        void unlock()
        {
            if (m_locked)
            {
                m_mutex->unlock();
                m_locked = false;
            }
        }
        ~locker()
        {
            if (m_locked)
                m_mutex->unlock();
        }
        void operator = (const locker& rhs) = delete;
        void operator = (locker&& rhs) = delete;

        M* m_mutex;
        bool m_locked;
    };

    using scoped_lock = locker<mutex>;

    class condition_variable : protected waitq
    {
    public:
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        int wait(scoped_lock& lock, uint64_t timeout = -1);
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        int wait(locker<photon::spinlock>& lock, uint64_t timeout = -1);
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        int wait(photon::spinlock& lock, uint64_t timeout = -1);
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        int wait(photon::mutex& lock, uint64_t timeout = -1);
        int wait_no_lock(uint64_t timeout = -1);
        void notify_one();
        void notify_all();
    };

    class semaphore : protected waitq   // NOT TESTED YET
    {
    public:
        explicit semaphore(uint64_t count = 0) : m_count(count) { }
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        int wait(uint64_t count, uint64_t timeout = -1);
        int signal(uint64_t count);

    protected:
        uint64_t m_count;
    };

    // to be different to timer flags
    // mark flag should be larger than 999, and not touch lower bits
    // here we selected
    constexpr int RLOCK=0x1000;
    constexpr int WLOCK=0x2000;
    class rwlock : protected waitq
    {
    public:
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
        int lock(int mode, uint64_t timeout = -1);
        int unlock();
    protected:
        int64_t state = 0;
    };

    class scoped_rwlock
    {
    public:
        scoped_rwlock(rwlock &rwlock, int lockmode) : m_locked(false) {
            m_rwlock = &rwlock;
            m_locked = (0 == m_rwlock->lock(lockmode));
        }
        bool locked() {
            return m_locked;
        }
        operator bool() {
            return locked();
        }
        int lock(int mode, bool must_lock = true) {
            int ret; do
            {
                ret = m_rwlock->lock(mode);
                m_locked = (0 == ret);
            } while (!m_locked && must_lock);
            // return 0 for locked else return -1 means failure
            return ret;
        }
        ~scoped_rwlock() {
            // only unlock when it is actually locked
            if (m_locked)
                m_rwlock->unlock();
        }
    protected:
        rwlock* m_rwlock;
        bool m_locked;
    };

    // `usec` is the *maximum* amount of time to sleep
    //  returns 0 if slept well or interrupted by IdleWakeUp() or qlen
    //  returns -1 error occured with in IdleSleeper()
    //  Do NOT invoke photon::usleep() or photon::sleep() in IdleSleeper,
    //  because their implementation depends on IdleSleeper.
    typedef int (*IdleSleeper)(uint64_t usec);
    void set_idle_sleeper(IdleSleeper idle_sleeper);
    IdleSleeper get_idle_sleeper();

    // Saturating addition, primarily for timeout caculation
    __attribute__((always_inline)) inline
    uint64_t sat_add(uint64_t x, uint64_t y)
    {
        register uint64_t z asm ("rax");
        asm("add %2, %1; sbb %0, %0; or %1, %0;" : "=r"(z), "+r"(x) : "r"(y) : "cc");
        return z;
    }

    // Saturating subtract, primarily for timeout caculation
    __attribute__((always_inline)) inline
    uint64_t sat_sub(uint64_t x, uint64_t y)
    {
        register uint64_t z asm ("rax");
        asm("xor %0, %0; subq %2, %1; cmovaeq %1, %0;" : "=r"(z), "+r"(x) ,"+r"(y) : : "cc");
        return z;
    }

    class Timer
    {
    public:
        // the prototype of timer entry function
        // return value will be used as the next timeout,
        // 0 for default_timeout (given in the ctor)
        using Entry = Delegate<uint64_t>;

        // Create a timer object with `default_timedout` in usec, callback function `on_timer`,
        // and callback argument `arg`. The timer object is implemented as a special thread, so
        // it has a `stack_size`, and the `on_timer` is invoked within the thread's context.
        // The timer object is deleted automatically after it is finished.
        Timer(uint64_t default_timeout, Entry on_timer, bool repeating = true,
              uint64_t stack_size = 1024 * 64)
        {
            _on_timer = on_timer;
            _default_timeout = default_timeout;
            _repeating = repeating;
            _th = thread_create(&_stub, this, stack_size);
            thread_enable_join(_th);
            thread_yield_to(_th);
        }
        // reset the timer's timeout
        int reset(uint64_t new_timeout = -1)
        {
            if (!_waiting)
            {
                return -1;
            }
            _reset_timeout = new_timeout;
            // since reset might called in usleep defer calls
            // _th state might same thread as CURRENT
            // check before yield_to, to prevent generating meanless ERROR log
            if (_th != CURRENT) {
                thread_interrupt(_th, EAGAIN);
                thread_yield_to(_th);
            }
            return 0;
        }
        int cancel()
        {
            return reset(-1);
        }
        int stop()
        {
            while (cancel())
                _wait_ready.wait_no_lock();
            return 0;
        }
        ~Timer()
        {
            if (!_th) return;
            stop();
            _repeating = false;
            if (_waiting)
                thread_interrupt(_th, ECANCELED);

            // wait for the worker thread to complete
            thread_join((join_handle*)_th);
        }

    protected:
        thread* _th;
        Entry _on_timer;
        uint64_t _default_timeout;
        uint64_t _reset_timeout;
        bool _repeating;
        bool _waiting = false;
        condition_variable _wait_ready;
        static void* _stub(void* _this);
        void stub();
    };

};

/*
 WITH_LOCK(mutex)
 {
    ...
 }
*/
#define WITH_LOCK(mutex) if (auto __lock__ = scoped_lock(mutex))


#define _TOKEN_CONCAT(a, b) a ## b
#define _TOKEN_CONCAT_(a, b) _TOKEN_CONCAT(a, b)
#define SCOPED_LOCK(x, ...) photon::locker<decltype(x)> \
    _TOKEN_CONCAT_(__locker__, __LINE__) (x, ##__VA_ARGS__)
