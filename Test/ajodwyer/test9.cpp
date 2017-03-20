#include <atomic>
#include <cassert>
#include <memory>
#include <thread>
#include <vector>

#if USE_RCU_CELL
 #include "rcu.hpp"
 #include "rcu_cell.hpp"
 using std::rcu::cell;
 using std::rcu::snapshot_ptr;
 #define ASSERT_SUCCESSFUL_CLEANUP() do { rcu_barrier(); assert(A::live_objects == 0); } while (0)
#elif USE_HAZPTR_CELL
 #include "hazptr.hpp"
 #include "hazptr_cell.hpp"
 using std::hazptr::cell;
 using std::hazptr::snapshot_ptr;
 #define ASSERT_SUCCESSFUL_CLEANUP() do { hazptr_barrier(); assert(A::live_objects == 0); } while (0)
 void hazptr_barrier()
 {
     auto& dom = get_default_hazptr_domain();
     using D = std::hazptr::hazptr_domain;
     dom.~D();
     new (&dom) D();
 }
#endif

struct A {
    static std::atomic<int> live_objects;
    int value;
    explicit A(int v): value(v) { ++live_objects; }
    A(const A& o): value(o.value) { ++live_objects; }
    A(A&& o): value(o.value) { ++live_objects; }
    A& operator= (const A&) = default;
    A& operator= (A&&) = default;
    ~A() { value = 999; --live_objects; }
};

std::atomic<int> A::live_objects{};

void test_simple()
{
    cell<A> c;
    c.update(std::make_unique<A>(42));
    auto sp1 = c.get_snapshot();
    c.update(std::make_unique<A>(43));
    auto sp2 = c.get_snapshot();

    assert(A::live_objects == 2);
    assert(sp1->value == 42);
    assert(sp2->value == 43);

    sp1 = std::move(sp2);
    assert(sp1->value == 43);
    assert(sp2 == nullptr);
}

void test_outliving()
{
    snapshot_ptr<A> sp = nullptr;
    if (true) {
        cell<A> c(std::make_unique<A>(314));
        sp = c.get_snapshot();
    }
    assert(sp != nullptr);
    assert(sp->value == 314);
}

void test_shared_ptr()
{
    std::shared_ptr<A> shptr;
    if (true) {
        cell<A> c(std::make_unique<A>(314));
        auto sp = c.get_snapshot();
        shptr = std::move(sp);
        assert(shptr);
        assert(shptr->value == 314);
    }
    assert(shptr);
    assert(shptr->value == 314);
    shptr = nullptr;
}

void test_thread_safety()
{
    cell<A> c(std::make_unique<A>(0));
    std::thread t[100];
    for (int i=0; i < 100; ++i) {
        t[i] = std::thread([i, &c]{
            c.update(std::make_unique<A>(i));
            auto sp1 = c.get_snapshot();
            c.update(std::make_unique<A>(100+i));
            auto sp2 = c.get_snapshot();
            sp1 = nullptr;
            sp2 = nullptr;
            c.update(std::make_unique<A>(200+i));
            auto sp3 = c.get_snapshot();
        });
    }
    for (int i=0; i < 100; ++i) {
        t[i].join();
    }
    auto sp = c.get_snapshot();
    assert(sp != nullptr);
    assert(sp->value >= 200);
}

void test_non_race_free_type()
{
    static auto get_next_value = []{
        static std::atomic<int> x(0);
        return ++x;
    };
    static auto the_zero_vector = []{
        return std::make_unique<std::vector<A>>(10, A(0));
    };
    static auto print_vector = [](const auto& c){
        static std::mutex m;
        std::lock_guard<std::mutex> lock(m);  // Just to avoid interleaving output.
        auto sp = c.get_snapshot();
        for (int i=0; i < sp->size(); ++i) {
            printf("%d ", (*sp)[i].value);
        }
        printf("\n");
    };
    cell<std::vector<A>> c(the_zero_vector());
    std::thread t[10];
    for (int i=0; i < 10; ++i) {
        t[i] = std::thread([i, &c]{
            int result = get_next_value();
            if (result == 3) {
                // Zero the whole vector, thread-safely.
                c.update(the_zero_vector());
            } else {
                // Update only my own element of the vector.
                auto sp = c.get_snapshot();
                (*sp)[i] = A(result);
            }
            print_vector(c);
        });
    }
    for (int i=0; i < 10; ++i) {
        t[i].join();
    }
    print_vector(c);
}

int main(int argc, char **argv)
{
    test_simple();
    ASSERT_SUCCESSFUL_CLEANUP();
    test_outliving();
    ASSERT_SUCCESSFUL_CLEANUP();
    test_shared_ptr();
    ASSERT_SUCCESSFUL_CLEANUP();
    test_thread_safety();
    ASSERT_SUCCESSFUL_CLEANUP();
    test_non_race_free_type();
    ASSERT_SUCCESSFUL_CLEANUP();
    return 0;
}
