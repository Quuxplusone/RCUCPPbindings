#pragma once

#include <cstddef>
#include <memory>
#include <utility>

// Derived-type approach.  All RCU-protected data structures using this
// approach must derive from std::rcu::enable_retire_on_this, which in turn derives
// from std::rcu_head.  No idea what happens in case of multiple inheritance.

namespace std {
namespace rcu {
    template<typename T, typename D = default_delete<T>, bool E = is_empty<D>::value>
    class enable_retire_on_this: private rcu_head {
        D deleter;
    public:
        static void trampoline(rcu_head *rhp)
        {
            auto rhdp = static_cast<enable_retire_on_this *>(rhp);
            auto obj = static_cast<T *>(rhdp);
            rhdp->deleter(obj);
        }

        void retire(D d = {})
        {
            deleter = std::move(d);
            ::call_rcu(static_cast<rcu_head *>(this), trampoline);
        }

        void retire(rcu_domain &rd, D d = {})
        {
            deleter = std::move(d);
            rd.retire(static_cast<rcu_head *>(this), trampoline);
        }
    };


    // Specialization for when D is an empty type.

    template<typename T, typename D>
    class enable_retire_on_this<T,D,true>: private rcu_head {
    public:
        static void trampoline(rcu_head *rhp)
        {
            auto rhdp = static_cast<enable_retire_on_this *>(rhp);
            auto obj = static_cast<T *>(rhdp);
            D()(obj);
        }

        void retire(D = {})
        {
            ::call_rcu(static_cast<rcu_head *>(this), trampoline);
        }

        void retire(rcu_domain &rd, D = {})
        {
            rd.retire(static_cast<rcu_head *>(this), trampoline);
        }
    };
}} // namespace std::rcu
