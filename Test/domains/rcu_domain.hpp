#pragma once

#include <type_traits>

extern "C" struct rcu_head;

namespace std {
namespace rcu {
    // See Lawrence Crowl's P0260 "C++ Concurrent Queues" for this API's rationale.
    // Basically, there is an implicit concept RcuDomain that is satisfied by any
    // domain providing these functions; rcu_domain_base is a reification of that
    // concept into a classical polymorphic class; rcu_domain_wrapper<D> is a
    // classical polymorphic class derived from rcu_domain_base which implements
    // the same semantics as class D (which must satisfy the RcuDomain concept).
    // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0260r0.html#Binary
    //
    class rcu_domain_base {
    public:
	rcu_domain_base() noexcept = default;
	rcu_domain_base(const rcu_domain_base&) = delete;
	virtual ~rcu_domain_base() = default;

	virtual bool register_thread_needed() const noexcept = 0;
	virtual void register_thread() = 0;
	virtual void unregister_thread() = 0;
	virtual void thread_offline() noexcept = 0;
	virtual void thread_online() noexcept = 0;

	virtual bool quiescent_state_needed() const noexcept = 0;
	virtual void quiescent_state() noexcept = 0;

	virtual void read_lock() noexcept = 0;
	virtual void read_unlock() noexcept = 0;

	virtual void retire(rcu_head *rhp, void (*cbf)(rcu_head *rhp)) = 0;

	virtual void synchronize() noexcept = 0;
	virtual void barrier() noexcept = 0;
    };

    namespace detail {
        #define DETECT(id, expression) \
            template<class D, class = void> struct id : std::false_type {}; \
            template<class D> struct id<D, decltype(expression, void())> : std::true_type {}
        DETECT(has_qsn, std::declval<D&>().quiescent_state_needed());
        DETECT(has_qs, std::declval<D&>().quiescent_state());
        DETECT(has_rtn, std::declval<D&>().register_thread_needed());
        DETECT(has_rt, std::declval<D&>().register_thread());
        DETECT(has_urt, std::declval<D&>().unregister_thread());
        DETECT(has_ton, std::declval<D&>().thread_online());
        DETECT(has_toff, std::declval<D&>().thread_offline());
        #undef DETECT

        // quiescent_state_needed defaults to "false"
        template<class D, std::enable_if_t<has_qsn<D>::value,bool> =true> bool quiescent_state_needed(const D *d) { return d->quiescent_state_needed(); }
        template<class D, std::enable_if_t<!has_qsn<D>::value,bool> =true> bool quiescent_state_needed(const D *d) { return false; }

        // quiescent_state defaults to a no-op
        template<class D, std::enable_if_t<has_qs<D>::value,bool> =true> void quiescent_state(D *d) { d->quiescent_state(); }
        template<class D, std::enable_if_t<!has_qs<D>::value,bool> =true> void quiescent_state(D *d) {}

        // register_thread_needed defaults to "false"
        template<class D, std::enable_if_t<has_rtn<D>::value,bool> =true> bool register_thread_needed(const D *d) { return d->register_thread_needed(); }
        template<class D, std::enable_if_t<!has_rtn<D>::value,bool> =true> bool register_thread_needed(const D *d) { return false; }

        // register_thread and unregister_thread must be provided as a pair
        template<class D, std::enable_if_t<has_rt<D>::value && has_rt<D>::value,bool> =true> void register_thread(D *d) { d->register_thread(); }
        template<class D, std::enable_if_t<has_rt<D>::value && has_rt<D>::value,bool> =true> void unregister_thread(D *d) { d->unregister_thread(); }
        template<class D, std::enable_if_t<!has_rt<D>::value && !has_urt<D>::value,bool> =true> void register_thread(D *d) {}
        template<class D, std::enable_if_t<!has_rt<D>::value && !has_urt<D>::value,bool> =true> void unregister_thread(D *d) {}

        // thread_online and thread_offline must be provided as a pair
        template<class D, std::enable_if_t<has_ton<D>::value && has_toff<D>::value,bool> =true> void thread_online(D *d) { d->thread_online(); }
        template<class D, std::enable_if_t<has_ton<D>::value && has_toff<D>::value,bool> =true> void thread_offline(D *d) { d->thread_offline(); }
        template<class D, std::enable_if_t<!has_ton<D>::value && !has_toff<D>::value,bool> =true> void thread_online(D *d) {}
        template<class D, std::enable_if_t<!has_ton<D>::value && !has_toff<D>::value,bool> =true> void thread_offline(D *d) {}
    } // namespace detail

    template<class Domain>
    class rcu_domain_wrapper : public virtual rcu_domain_base {
	Domain *d;

    public:
	rcu_domain_wrapper(Domain& d) noexcept : d(&d) {}

	bool register_thread_needed() const noexcept override { return detail::register_thread_needed<Domain>(d); }
	void register_thread() override { detail::register_thread<Domain>(d); }
	void unregister_thread() override { detail::unregister_thread<Domain>(d); }
	void thread_offline() noexcept override { detail::thread_offline<Domain>(d); }
	void thread_online() noexcept override { detail::thread_online<Domain>(d); }

	bool quiescent_state_needed() const noexcept override { return detail::quiescent_state_needed<Domain>(d); }
	void quiescent_state() noexcept override { detail::quiescent_state<Domain>(d); }

	void read_lock() noexcept override { d->read_lock(); }
	void read_unlock() noexcept override { d->read_unlock(); }

	void retire(rcu_head *rhp, void (*cbf)(rcu_head *rhp)) override { d->retire(rhp, cbf); }

	void synchronize() noexcept override { d->synchronize(); }
	void barrier() noexcept override { d->barrier(); }
    };
} // namespace rcu
} // namespace std
