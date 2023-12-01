#define protected public
#include "co.h"
#undef protected
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include <vector>
#include <new>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <sys/time.h>
#include "list.h"
#include "alog.h"

#include "context.h"

namespace photon
{
    static std::condition_variable idle_sleep;
    static int default_idle_sleeper(uint64_t usec)
    {
        static std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        auto us = ((int64_t&)usec) < 0 ?
            std::chrono::microseconds::max() :
            std::chrono::microseconds(usec);
        idle_sleep.wait_for(lock, us);
        return 0;
    }
    typedef int (*IdleSleeper)(uint64_t usec);
    IdleSleeper idle_sleeper = &default_idle_sleeper;
    void set_idle_sleeper(IdleSleeper sleeper)
    {
        idle_sleeper = sleeper ? sleeper : &default_idle_sleeper;
    }
    IdleSleeper get_idle_sleeper()
    {
        return idle_sleeper;
    }

    class Stack
    {
    public:
        template<typename F>
        void init(void* ptr, F ret2func)
        {
            _ptr = ptr;
            push(0);
            push(ret2func);
#if !defined(NEW) && !defined(CACSTH)
            push(ret2func);
            // make room for rbx, rbp, r12~r15
            (uint64_t*&)_ptr -= 6;
#endif
        }
        void** pointer_ref()
        {
            return &_ptr;
        }
        void push(uint64_t x)
        {
            *--(uint64_t*&)_ptr = x;
        }
        template<typename T>
        void push(const T& x)
        {
            push((uint64_t)x);
        }
        uint64_t pop()
        {
            return *((uint64_t*&)_ptr)++;
        }
        uint64_t& operator[](int i)
        {
            return static_cast<uint64_t*>(_ptr)[i];
        }
        void* _ptr;
    };

    struct thread;
    typedef intrusive_list<thread> thread_list;
    struct thread : public intrusive_list_node<thread>
    {
        Stack stack;
        uint64_t ret_addr;
        states state = states::READY;
        int error_number = 0;
        int idx;                            /* index in the sleep queue array */
        int flags = 0;
        int reserved;
        bool joinable = false;
        bool shutting_down = false;         // the thread should cancel what is doing, and quit
                                            // current job ASAP; not allowed to sleep or block more
                                            // than 10ms, otherwise -1 will be returned and errno == EPERM

        thread_list* waitq = nullptr;       /* the q if WAITING in a queue */

        thread_entry start;
        void* arg;
        void* retval;
        void* go() { return retval = start(arg); }
        char* buf;

        uint64_t ts_wakeup = 0;             /* Wakeup time when thread is sleeping */
        condition_variable cond;            /* used for join, or timer REUSE */

        int set_error_number()
        {
            if (error_number)
            {
                errno = error_number;
                error_number = 0;
                return -1;
            }
            return 0;
        }

        void dequeue_ready()
        {
            if (waitq) {
                waitq->erase(this);
                waitq = nullptr;
            } else {
                assert(this->single());
            }
            state = states::READY;
            CURRENT->insert_tail(this);
        }

        bool operator < (const thread &rhs)
        {
            return this->ts_wakeup < rhs.ts_wakeup;
        }

        void dispose()
        {
            delete [] buf;
        }
    };
    static_assert(offsetof(thread, stack) == 16, "...");
    static_assert(offsetof(thread, ret_addr) == 24, "...");

    struct join_handle: public thread {};

    class SleepQueue
    {
    public:
        std::vector<thread *> q;

        [[gnu::always_inline]]
        thread* front() const
        {
            return q.empty() ? nullptr : q.front();
        }

        [[gnu::always_inline]]
        bool empty() const
        {
            return q.empty();
        }

        [[gnu::always_inline]]
        int push(thread *obj)
        {
            q.push_back(obj);
            obj->idx = q.size() - 1;
            up(obj->idx);
            return 0;
        }

        [[gnu::always_inline]]
        thread* pop_front()
        {
            auto ret = q[0];
            q[0] = q.back();
            q[0]->idx = 0;
            q.pop_back();
            down(0);
            return ret;
        }

        [[gnu::always_inline]]
        int pop(thread *obj)
        {
            if (obj->idx == -1) return -1;
            if ((size_t)obj->idx == q.size() - 1){
                q.pop_back();
                obj->idx = -1;
                return 0;
            }

            auto id = obj->idx;
            q[obj->idx] = q.back();
            q[id]->idx = id;
            q.pop_back();
            if (!up(id)) down(id);
            obj->idx = -1;

            return 0;
        }

        __attribute__((always_inline))
        void update_node(int idx, thread *&obj)
        {
            q[idx] = obj;
            q[idx]->idx = idx;
        }

        // compare m_nodes[idx] with parent node.
        [[gnu::always_inline]]
        bool up(int idx)
        {
            auto tmp = q[idx];
            bool ret = false;
            while (idx != 0){
                auto cmpIdx = (idx - 1) >> 1;
                if (*tmp < *q[cmpIdx]) {
                    update_node(idx, q[cmpIdx]);
                    idx = cmpIdx;
                    ret = true;
                    continue;
                }
                break;
            }
            if (ret) update_node(idx, tmp);
            return ret;
        }

        // compare m_nodes[idx] with child node.
        [[gnu::always_inline]]
        bool down(int idx)
        {
            auto tmp = q[idx];
            size_t cmpIdx = (idx << 1) + 1;
            bool ret = false;
            while (cmpIdx < q.size()) {
                if (cmpIdx + 1 < q.size() && *q[cmpIdx + 1] < *q[cmpIdx]) cmpIdx++;
                if (*q[cmpIdx] < *tmp){
                    update_node(idx, q[cmpIdx]);
                    idx = cmpIdx;
                    cmpIdx = (idx << 1) + 1;
                    ret = true;
                    continue;
                }
                break;
            }
            if (ret) update_node(idx, tmp);
            return ret;
        }
    };

    thread* CURRENT = new thread;
    static SleepQueue sleepq;

    static void thread_die(thread* th)
    {
    //    LOG_DEBUG(th, th->buf);
        th->dispose();
    }

    extern void photon_die_and_jmp_to_context(thread* dying_th, void** dest_context,
        void(*th_die)(thread*)) asm ("_photon_die_and_jmp_to_context");
    __attribute__((preserve_none))
    extern void photon_die_and_jmp_to_context_new(thread* dying_th, thread* dest_context,
        void(*th_die)(thread*)) asm ("_photon_die_and_jmp_to_context_new");
    static int do_idle_sleep(uint64_t usec);
    static int resume_sleepers();
    static inline void switch_context(thread* from, states new_state, thread* to);

    extern void photon_switch_context(void**, void**) asm ("_photon_switch_context");
    inline void switch_context(thread* from, states new_state, thread* to)
    {
        from->state = new_state;
        to->state   = states::RUNNING;
        photon_switch_context(from->stack.pointer_ref(), to->stack.pointer_ref());
    }

    __attribute__((preserve_none))
    extern void photon_switch_context_new(thread*, thread*) asm ("_photon_switch_context_new");
    inline void switch_context_new(thread* from, states new_state, thread* to)
    {
        from->state = new_state;
        to->state   = states::RUNNING;
        // photon_switch_context_new(from, to);
        __asm__ volatile (R"(
            mov 0x18(%1), %%rcx
            lea 1f(%%rip), %%rdx
            mov %%rsp, 0x10(%0)
            mov %%rdx, 0x18(%0)
            mov 0x10(%1), %%rsp
            jmp *%%rcx
            1:
        )" : : "r"(from), "r"(to));
        __asm__ volatile ("" : : :
            "rax", "rbx", "rcx", "rdx", "rsi", "rdi",
            "rbp", "r8", "r9", "r10", "r11", "r12",
            "r13", "r14", "r15");
    }

    static void enqueue_wait(thread_list* q, thread* th, uint64_t expire)
    {
        assert(th->waitq == nullptr);
        th->ts_wakeup = expire;
        if (q)
        {
            q->push_back(th);
            th->waitq = q;
        }
    }

#ifdef CACSTH
    __attribute__((preserve_none))
#endif
    static void thread_stub()
    {
        CURRENT->go();
            // LOG_DEBUG("DONE", VALUE(CURRENT));
        CURRENT->cond.notify_all();
        while (CURRENT->single() && !sleepq.empty())
        {
            if (resume_sleepers() == 0)
                do_idle_sleep(-1);
        }

        auto th = CURRENT;
        CURRENT = CURRENT->remove_from_list();
        if (!th->joinable)
        {
            th->state = states::DONE;
            #if defined(CACSTH)
            thread* to = CURRENT;
            CACS_die_switch_defer(th, to, (void(*)(void*))&thread_die);
            #elif defined(NEW)
            thread* to = CURRENT;
            photon_die_and_jmp_to_context_new(th, to, &thread_die);
            #else
            photon_die_and_jmp_to_context(th,
                CURRENT->stack.pointer_ref(), &thread_die);
            #endif
        }
        else
        {
            #if defined(CACSTH)
            thread *to = CURRENT;
            th->state = states::DONE;
            to->state = states::RUNNING;
            CACS(th, to);
            #elif defined(NEW)
            thread *to = CURRENT;
            th->state = states::DONE;
            to->state = states::RUNNING;
            photon_switch_context_new(th, to);
            #else
            switch_context(th, states::DONE, CURRENT);
            #endif
        }
    }

    thread* thread_create(thread_entry start, void* arg, uint64_t stack_size)
    {
        auto ptr = new char[stack_size];
        auto p = ptr + stack_size - sizeof(thread);
        (uint64_t&)p &= ~63;
#ifdef RANDOMIZE_SP
        p -= 16 * (rand() % 32);
#endif
        auto th = new (p) thread;
        th->buf = ptr;
        th->idx = -1;
        th->start = start;
        th->arg = arg;
        // p -= 64;
        th->stack.init(p, &thread_stub);
        th->ret_addr = (uint64_t)&thread_stub;
        th->state = states::READY;
        CURRENT->insert_tail(th);
        return th;
    }

    uint64_t now;
    static inline uint64_t update_now()
    {
        // struct timeval tv;
        // gettimeofday(&tv, NULL);
        // now = tv.tv_sec;
        // now *= 1000 * 1000;
        // return now += tv.tv_usec;
        return now;
    }
    static inline void prefetch_context(thread* from, thread* to)
    {
#ifdef CONTEXT_PREFETCHING
        const int CACHE_LINE_SIZE = 64;
        auto f = *from->stack.pointer_ref();
        __builtin_prefetch(f, 1);
        __builtin_prefetch((char*)f + CACHE_LINE_SIZE, 1);
        auto t = *to->stack.pointer_ref();
        __builtin_prefetch(t, 0);
        __builtin_prefetch((char*)t + CACHE_LINE_SIZE, 0);
#endif
    }

    static int resume_sleepers()
    {
        int count = 0;
        update_now();
        while(true)
        {
            auto th = sleepq.front();
            if (!th || now < th->ts_wakeup)
                break;

//            LOG_DEBUG(VALUE(now), " resuming thread ", VALUE(th), " which is supposed to wake up at ", th->ts_wakeup);
            sleepq.pop_front();
            th->dequeue_ready();
            count++;
        }
        return count;
    }

    states thread_stat(thread* th)
    {
        return th->state;
    }

#if !defined(CACSTH) && !defined(NEW)
    void thread_yield() {
        auto t0 = CURRENT;
        if (unlikely(t0->single())) {
            if (0 == resume_sleepers())
                return;     // no target to yield to
        }

        auto next = CURRENT = t0->next();
        prefetch_context(t0, next);
        switch_context(t0, states::READY, next);
    }
#elif defined(NEW)
    __attribute__((preserve_none))
    void thread_yield() {
        auto from = CURRENT;
        if (unlikely(from->single())) {
            if (resume_sleepers() == 0)
                return;
        }
        auto to = CURRENT = from->next();
        from->state = states::READY;
        to->state   = states::RUNNING;
        photon_switch_context_new(from, to);
        // asm volatile ("");
        // switch_context_new(from, states::RUNNING, to);
    }
#elif defined(CACSTH)
    __attribute__((preserve_none))
    void thread_yield() {
        auto from = CURRENT;
        if (unlikely(from->single())) {
            #ifndef CACSTAIL
            // if (resume_sleepers() == 0)
            #endif
                return;
        }
        auto to = CURRENT = from->next();
        from->state = states::READY;
        to->state   = states::RUNNING;
        #ifndef CACSTAIL
        CACS(from, to);
        #else
        CACS_tail(from, to);
        #endif
    }
#endif

    __attribute__((preserve_none))
    inline void _switch_context(void* from, void* to) {
        __asm__ (R"(
            lea 1f(%%rip), %%rax
            mov %%rsp, 16(%0)
            mov %%rax, 24(%0)
            mov 16(%1), %%rsp
            mov 24(%1), %%rax
            // pop %%rax
            jmp *%%rax
            1:
        )" :: "r" (from), "r" (to) : "rax");
    }


    void thread_yield_to(thread* th)
    {
        if (th == nullptr) // yield to any thread
        {
            if (CURRENT->single())
            {
                auto ret = resume_sleepers(); //photon::now will be update
                if (ret == 0)
                    return;     // no target to yield to
            }
            th = CURRENT->next(); // or not update
        }
        else if (th->state != states::READY)
        {
            LOG_ERROR_RETURN(EINVAL, , VALUE(th), " must be READY!");
        }

        auto t0 = CURRENT;
        CURRENT = th;
        prefetch_context(t0, CURRENT);
        #ifdef CACSTH
        t0->state=states::READY;
        th->state=states::RUNNING;
        CACS(t0, th);
        #elif defined(NEW)
        t0->state=states::READY;
        th->state=states::RUNNING;
        photon_switch_context_new(t0, th);
        #else
        switch_context(t0, states::READY, CURRENT);
        #endif
    }

    static int do_idle_sleep(uint64_t usec)
    {
        if (!sleepq.empty())
            if (sleepq.front()->ts_wakeup > now)
                usec = std::min(usec, sleepq.front()->ts_wakeup - now);

        return idle_sleeper(usec);
    }

    // returns 0 if slept well (at lease `useconds`), -1 otherwise
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    static int thread_usleep(uint64_t useconds, thread_list* waitq)
    {
        if (__builtin_expect(useconds == 0, 0)) {
            thread_yield();
            return 0;
        }
        CURRENT->state = states::WAITING;
        auto expire = sat_add(now, useconds);
        while (unlikely(CURRENT->single())) { // if no active threads available
            if (unlikely(resume_sleepers() > 0)) // will update_now() in it
            {
                break;
            }
            if (unlikely(now >= expire)) {
                return 0;
            }
            do_idle_sleep(useconds);
            if (likely(CURRENT->state ==
                       states::READY)) // CURRENT has been woken up during idle
                                       // sleep
            {
                CURRENT->set_error_number();
                return -1;
            }
        }

        auto t0 = CURRENT;
        CURRENT = CURRENT->remove_from_list();
        prefetch_context(t0, CURRENT);
        assert(CURRENT != nullptr);
        enqueue_wait(waitq, t0, expire);
        sleepq.push(t0);
        // LOG_INFO("BEFORE ` `", t0, CURRENT);
#if defined(CACSTH)
    t0->state = states::WAITING;
    CURRENT->state = states::RUNNING;
    CACS(t0, CURRENT);
#elif defined(NEW)
    t0->state = states::WAITING;
    CURRENT->state = states::RUNNING;
    photon_switch_context_new(t0, CURRENT);
#else
    switch_context(t0, states::WAITING, CURRENT);
#endif
    // LOG_INFO("AFTER ` `", t0, CURRENT);
    // CANNOT use t0 because switch context may crash all registers
    return CURRENT->set_error_number();
    }

#if defined(NEW) || defined(CACSTH)
    __attribute__((preserve_none))
#endif
    int thread_usleep(uint64_t useconds)
    {
        if (unlikely(CURRENT->shutting_down && useconds > 10*1000))
        {
            int ret = thread_usleep(10*1000, nullptr);
           if (ret >= 0)
                errno = EPERM ;
            return -1;
        }
        return thread_usleep(useconds, nullptr);
    }

#if defined(NEW) || defined(CACSTH)
    __attribute__((preserve_none))
#endif
    void thread_interrupt(thread* th, int error_number)
    {
        if (__builtin_expect(th->state == states::READY, 0)) { // th is already in runing queue
            return;
        }

        if (__builtin_expect(!th || th->state != states::WAITING, 0))
            LOG_ERROR_RETURN(EINVAL, , "invalid parameter");

        if (__builtin_expect(th == CURRENT, 0))
        {   // idle_sleep may run in CURRENT's context, which may be single() and WAITING
            th->state = states::READY;
            th->error_number = error_number;
            return;
        }
        sleepq.pop(th);
        th->dequeue_ready();
        th->error_number = error_number;
    }

    join_handle* thread_enable_join(thread* th, bool flag)
    {
        th->joinable = flag;
        return (join_handle*)th;
    }

    void thread_join(join_handle* jh)
    {
        auto th = (thread*)jh;
        if (!th->joinable)
            LOG_ERROR_RETURN(ENOSYS, , "join is not enabled for thread ", th);

        if (th->state != states::DONE)
        {
            th->cond.wait_no_lock();
            th->remove_from_list();
        }
        th->dispose();
    }

    int thread_shutdown(thread* th, bool flag)
    {
        if (!th)
            LOG_ERROR_RETURN(EINVAL, -1, "invalid thread");

        th->shutting_down = flag;
        if (th->state == states::WAITING)
            thread_interrupt(th, EPERM);
        return 0;
    }

   // if set, the timer will fire repeatedly, using `timer_entry` return val as the next timeout
    // if the return val is 0, use `default_timedout` as the next timedout instead;
    // if the return val is -1, stop the timer;
    const uint64_t TIMER_FLAG_REPEATING     = 1 << 0;

    // if set, the timer object is ready for reuse via `timer_reset()`, implying REPEATING
    // if `default_timedout` is -1, the created timer does NOT fire a first shot before `timer_reset()`
    const uint64_t TIMER_FLAG_REUSE         = 1 << 1;

    typedef uint64_t (*timer_entry)(void*);
    struct timer_args
    {
        uint64_t default_timeout;
        timer_entry on_timer;
        uint64_t flags;
        void* arg;
    };
    const uint64_t TIMER_FLAG_WAITING_SHOT = 999;
    // bitmark for telling timing loop that timer has already reset by on_timer
    const uint64_t TIMER_FLAG_RESET = 1<<3;
    void Timer::stub()
    {
        auto timeout = _default_timeout;
        do {
        again:
            _waiting = true;
            _wait_ready.notify_all();
            int ret = thread_usleep(timeout);
            _waiting = false;
            if (ret < 0)
            {
                int e = errno;
                if (e == ECANCELED) {
                    break;
                } else if (e == EAGAIN) {
                    timeout = _reset_timeout;
                    goto again;
                }
                else assert(false);
            }

            timeout = _on_timer.fire();
            if (!timeout)
                timeout = _default_timeout;
        } while(_repeating);
        _th = nullptr;
    }
    void* Timer::_stub(void* _this)
    {
        static_cast<Timer*>(_this)->stub();
        return nullptr;
    }

#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int waitq::wait(uint64_t timeout)
    {
        static_assert(sizeof(q) == sizeof(thread_list), "...");
        int ret = thread_usleep(timeout, (thread_list*)&q);
        if (ret == 0)
        {
            errno = ETIMEDOUT;
            return -1;
        }
        return (errno == ECANCELED) ? 0 : -1;
    }
    void waitq::resume(thread* th)
    {
        assert(th->waitq == (thread_list*)&q);
        if (!th || !q || th->waitq != (thread_list*)&q)
            return;
        // will update q during thread_interrupt()
        thread_interrupt(th, ECANCELED);
    }
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int mutex::lock(uint64_t timeout)
    {
        if (owner == CURRENT)
             LOG_ERROR_RETURN(EINVAL, -1, "recursive locking is not supported");

        while(owner)
        {
            // LOG_INFO("wait(timeout);", VALUE(CURRENT), VALUE(this), VALUE(owner));
            if (wait(timeout) < 0)
            {
                // EINTR means break waiting without holding lock
                // it is normal in OutOfOrder result collected situation, and that is the only
                // place using EINTR to interrupt micro-threads (during getting lock)
                // normally, ETIMEOUT means wait timeout and ECANCELED means resume from sleep
                // LOG_DEBUG("timeout return -1;", VALUE(CURRENT), VALUE(this), VALUE(owner));
                return -1;   // timedout or interrupted
            }
        }

        owner = CURRENT;
        // LOG_INFO(VALUE(CURRENT), VALUE(this));
        return 0;
    }
    void mutex::unlock()
    {
        if (owner != CURRENT)
            return;

        owner = nullptr;
        resume_one();
        // LOG_INFO(VALUE(CURRENT), VALUE(this), VALUE(owner));
    }
    int condition_variable::wait_no_lock(uint64_t timeout)
    {
        return waitq::wait(timeout);
    }
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int condition_variable::wait(scoped_lock& lock, uint64_t timeout)
    {   // current implemention is only for interface compatibility, needs REDO for multi-vcpu
        if (!lock.locked())
            return wait_no_lock(timeout);

        lock.unlock();
        int ret = wait_no_lock(timeout);
        lock.lock();
        return ret;
    }
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int condition_variable::wait(locker<spinlock>& lock, uint64_t timeout)
    {   // current implemention is only for interface compatibility, needs REDO for multi-vcpu
        if (!lock.locked())
            return wait_no_lock(timeout);

        lock.unlock();
        int ret = wait_no_lock(timeout);
        lock.lock();
        return ret;
    }
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int condition_variable::wait(spinlock& lock, uint64_t timeout)
    {   // current implemention is only for interface compatibility, needs REDO for multi-vcpu
        lock.unlock();
        int ret = wait_no_lock(timeout);
        lock.lock();
        return ret;
    }
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int condition_variable::wait(mutex& lock, uint64_t timeout)
    {   // current implemention is only for interface compatibility, needs REDO for multi-vcpu
        lock.unlock();
        int ret = wait_no_lock(timeout);
        lock.lock();
        return ret;
    }
    void condition_variable::notify_one()
    {
        resume_one();
    }
    void condition_variable::notify_all()
    {
        while(q)
            resume_one();
    }
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int semaphore::wait(uint64_t count, uint64_t timeout)
    {
        if (count == 0) return 0;
        while(m_count < count)
        {
            CURRENT->retval = (void*)count;
            int ret = waitq::wait(timeout);
            if (ret < 0) {
                // when timeout, and CURRENT was the first in waitq,
                // we need to try to resume the next one in q
                signal(0);
                return -1;
            }
        }
        m_count -= count;
        return 0;
    }
    int semaphore::signal(uint64_t count)
    {
        m_count += count;
        while(q)
        {
            auto q_front_count = (uint64_t)q->retval;
            if (m_count < q_front_count) break;
            resume_one();
        }
        return 0;
    }
#if defined(NEW) || defined(CACSTH) && !defined(CACSYIELD)
    __attribute__((preserve_none))
#endif
    int rwlock::lock(int mode, uint64_t timeout)
    {
        if (mode != RLOCK && mode != WLOCK)
            LOG_ERROR_RETURN(EINVAL, -1, "mode unknow");
        // backup retval
        void* bkup = CURRENT->retval;
        DEFER(CURRENT->retval = bkup);
        auto mark = (uint64_t)CURRENT->retval;
        // mask mark bits, keep RLOCK WLOCK bit clean
        mark &= ~(RLOCK | WLOCK);
        // mark mode and set as retval
        mark |= mode;
        CURRENT->retval = (void*)(mark);
        bool wait_cond;
        int op;
        if (mode == RLOCK) {
            wait_cond = state<0;
            op = 1;
        } else { // WLOCK
            wait_cond = state;
            op = -1;
        }
        if (q || wait_cond) {
            int ret = wait(timeout);
            if (ret < 0)
                return -1; // break by timeout or interrupt
        }
        state += op;
        return 0;
    }
    int rwlock::unlock()
    {
        assert(state != 0);
        if (state>0)
            state --;
        else
            state ++;
        if (state == 0 && q) {
            if (((uint64_t)q->retval) & WLOCK)
                resume_one();
            else
                while (q && (((uint64_t)q->retval) & RLOCK))
                    resume_one();
        }
        return 0;
    }

    int init()
    {
        CURRENT->idx = -1;
        CURRENT->state = states::RUNNING;
        update_now();
        return 0;
    }
    int fini()
    {
        return 0;
    }

    int spinlock::lock() {
        while (_lock.exchange(true, std::memory_order_acquire)) {
            while (_lock.load(std::memory_order_relaxed)) {
#ifdef __aarch64__
                asm volatile("isb" : : : "memory");
#else
                _mm_pause();
#endif
            }
        }
        return 0;
    }

    void spinlock::unlock() {
        _lock.store(false, std::memory_order_release);
    }

    int spinlock::try_lock() {
        return (!_lock.load(std::memory_order_relaxed) && !_lock.exchange(true, std::memory_order_acquire)) ? 0 : -1;
    }

}
