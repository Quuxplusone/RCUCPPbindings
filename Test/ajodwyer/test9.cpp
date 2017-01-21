#include "urcu-signal.hpp"
#include "rcu_unique_ptr.hpp"
#include <assert.h>

int Foo_destructions;

struct Foo {
  int a;

  explicit Foo(int i) : a(i) { }
  ~Foo() { ++Foo_destructions; }
};

void test_retire_on_destruction()
{
    Foo_destructions = 0;
    {
        std::rcu::unique_ptr<Foo> fp = std::rcu::make_unique<Foo>(42);
        assert((*fp).a == 42);
        assert(fp.get()->a == 42);
        assert(fp->a == 42);
        // on destruction of fp, defer a call to "delete fp.get()"
    }
    assert(Foo_destructions == 0);
    rcu_barrier();
    assert(Foo_destructions == 1);
}

void test_retire_on_assignment()
{
    Foo_destructions = 0;
    {
        std::rcu::unique_ptr<Foo> fp = std::rcu::make_unique<Foo>(42);
        assert((*fp).a == 42);
        assert(fp.get()->a == 42);
        assert(fp->a == 42);
        fp = nullptr;  // defer a call to "delete fp.get()"
        assert(Foo_destructions == 0);
        rcu_barrier();
        assert(Foo_destructions == 1);
    }
    rcu_barrier();
    assert(Foo_destructions == 1);
}

int main()
{
    test_retire_on_destruction();
    test_retire_on_assignment();
}
