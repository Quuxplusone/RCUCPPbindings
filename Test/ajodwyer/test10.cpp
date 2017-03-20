#include "hp-folly.hpp"
#include "test10.h"
#include <cassert>
#include <cstdio>

int main()
{
    hp_domain_folly domain;
    LockFreeLIFO<int> lst(domain);
    lst.push(1);
    lst.push(2);
    int result = lst.pop();
    assert(result == 2);
    result = lst.pop();
    assert(result == 1);
    try {
        result = lst.pop();
        assert(false);
    } catch (const std::out_of_range&) {
        result = 99;
    }
    assert(result == 99);
    puts("Success!");
}
