
#include "test11.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

std::atomic<bool> stop = {false};
std::atomic<int> reads = {0};
std::atomic<int> adds = {0};
std::atomic<int> removes = {0};

int rand_not_21_or_51()
{
    int x = rand() % 100;
    return (x == 21 || x == 51) ? 99 : x;
}

void reader(SWMRListSet<int>& s)
{
    bool contains_21 = false;
    bool contains_51 = false;
    while (!stop) {
        if (rand() % 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        bool b = s.contains(rand() % 100);
        ++reads;
        b = s.contains(1000 + rand() % 100); ++reads; assert(!b);
        b = s.contains(21); ++reads; if (contains_21) assert(b); contains_21 = b;
        b = s.contains(51); ++reads; if (contains_51) assert(b); contains_51 = b;
    }
}

void writer(SWMRListSet<int>& s)
{
    while (!stop) {
        if (rand() % 2) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(1));
        }
        s.add(rand() % 100);
        ++adds;
        s.remove(rand_not_21_or_51());
        ++removes;
        if (rand() % 2) {
            s.remove(rand_not_21_or_51());
            ++removes;
        }
    }
}

int main()
{
    std::hazptr::hazptr_domain domain;
    SWMRListSet<int> s(domain);
    std::thread w([&s]{ writer(s); });
    std::thread r[1000];
    for (int i=0; i < 1000; ++i) {
        r[i] = std::thread([&s]{ reader(s); });
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stop = true;
    for (int i=0; i < 1000; ++i) {
        r[i].join();
    }
    w.join();
    puts("Success!");
    printf("%d adds, %d removes, %d reads\n", adds.load(), removes.load(), reads.load());
}
