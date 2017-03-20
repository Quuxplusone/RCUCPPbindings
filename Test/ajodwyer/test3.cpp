#include <iostream>
#include <unistd.h>
#include "rcu_domain.hpp"
#include "rcu_obj_base.hpp"

// Derived-type approach.

struct foo: public std::rcu_obj_base<foo, void(*)(foo*)> {
    int a;
};

void my_cb(struct foo *fp)
{
    std::cout << "Callback fp->a: " << fp->a << "\n";
}

struct foo foo1;

int main(int argc, char **argv)
{
    std::rcu::rcu_domain rs;

    printf("%zu %zu %zu\n", sizeof(rcu_head), sizeof(std::rcu_obj_base<foo, void(*)(foo*)>), sizeof(foo));

    // First with a normal function.
    foo1.a = 42;
    foo1.retire(my_cb);
    rcu_barrier(); // Drain all callbacks before reusing them!

    // Next with a lambda, but no capture.
    foo1.a = 43;
    foo1.retire([] (struct foo *fp) {
            std::cout << "Lambda callback fp->a: " << fp->a << "\n";
          });
    rcu_barrier();

    std::cout << "Deletion with no rcu_domain\n";
    foo1.a = 44;
    foo1.retire(my_cb);
    rcu_barrier();

    std::cout << "Deletion with rcu_signal rcu_domain\n";
    foo1.a = 45;
    foo1.retire(rs, my_cb);
    rs.barrier();

    return 0;
}
