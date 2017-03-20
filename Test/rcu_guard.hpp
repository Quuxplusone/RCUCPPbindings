#pragma once

#include "rcu_domain.hpp"

class rcu_guard {
public:
    rcu_guard() noexcept
    {
        this->rd = nullptr;
        ::rcu_read_lock();
    }

    explicit rcu_guard(std::rcu::rcu_domain& d) : rd(&d)
    {
        rd->read_lock();
    }

    rcu_guard(const rcu_guard&) = delete;
    rcu_guard& operator=(const rcu_guard&) = delete;

    ~rcu_guard() noexcept
    {
        if (this->rd)
            this->rd->read_unlock();
        else
            ::rcu_read_unlock();
    }

private:
    std::rcu::rcu_domain *rd;
};
