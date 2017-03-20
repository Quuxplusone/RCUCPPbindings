#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include "rcu_domain.hpp"

// All RCU-protected data structures must derive from std::rcu::enable_retire_on_this,
// which derives privately from ::rcu_head.

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

        void retire(std::rcu::rcu_domain& rd, D d = {})
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

        void retire(std::rcu::rcu_domain& rd, D = {})
        {
            rd.retire(static_cast<rcu_head *>(this), trampoline);
        }
    };
} // namespace rcu
} // namespace std
