#include <iostream>
#include "rcu_domain.hpp"
#include "rcu_head_container_of.hpp"

// container_of() approach, encapsulated into rcu_head_container_of class.

struct foo {
    int a;
    rcu_head rh;
};

void my_cb(rcu_head *rhp)
{
    foo *fp = std::rcu_head_container_of<foo>::enclosing_class(rhp);
    std::cout << "Callback fp->a: " << fp->a << "\n";
}

foo foo1 = { 42 };

int main(int argc, char **argv)
{
    std::rcu_head_container_of<foo>::set_field(&foo::rh);

    // First with a normal function.
    call_rcu(&foo1.rh, my_cb);
    rcu_barrier(); // Drain all callbacks before reusing them!

    // Next with a lambda, but no capture.
    foo1.a = 43;
    call_rcu(&foo1.rh, [] (rcu_head *rhp) {
        foo *fp = std::rcu_head_container_of<foo>::enclosing_class(rhp);
        std::cout << "Lambda callback fp->a: " << fp->a << "\n";
    });
    rcu_barrier();

    return 0;
}
