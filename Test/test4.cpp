#include "rcu_domain.hpp"
#include "rcu_guard.hpp"

int main()
{
    std::rcu::rcu_domain rs;
    rcu_register_thread();
    {
            rcu_guard rr;
    }
    {
            rcu_guard rrs(rs);
    }
    rcu_unregister_thread();

    return 0;
}
