
/*
 * Copyright 2017 Facebook, Inc.
 * Modified by Arthur O'Dwyer.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hazptr.hpp"
#include <atomic>
#include <unordered_set>

static const int SCAN_THRESHOLD = 3;

namespace std {
namespace hazptr {

hazptr_domain::~hazptr_domain()
{
    if (true) {
        hazard_pointer *next;
        for (hazard_pointer *p = hazptrs_.load(); p; p = next) {
            next = p->next_;
            delete p;
        }
    }
    if (true) {
        hazptr_head *next;
        for (hazptr_head *p = retired_.load(); p; p = next) {
            next = p->next_;
            (p->reclaim_)(p);
        }
    }
}

hazptr_domain::hazard_pointer *hazptr_domain::acquire()
{
    hazard_pointer *p;
    for (p = hazptrs_.load(); p; p = p->next_) {
        bool active = p->active_.load();
        if (!active) {
            if (p->active_.compare_exchange_weak(active, true)) {
                return p;
            }
        }
    }
    p = new hazard_pointer;  // may throw std::bad_alloc
    p->active_.store(true);
    while (true) {
        p->next_ = hazptrs_.load();
        if (hazptrs_.compare_exchange_weak(p->next_, p)) {
            break;
        }
    }
    ++hcount_;
    return p;
}

void hazptr_domain::retire(hazptr_head *p)
{
    int rcount = pushRetired(p, p, 1) + 1;
    if (rcount >= SCAN_THRESHOLD * hcount_.load()) {
        tryBulkReclaim();
    }
}

void hazptr_domain::tryBulkReclaim()
{
    while (true) {
        auto number_of_hazard_pointers = hcount_.load();
        auto number_of_retired_items = rcount_.load();
        if (number_of_retired_items < SCAN_THRESHOLD * number_of_hazard_pointers) {
            return;
        }
        if (rcount_.compare_exchange_weak(number_of_retired_items, 0)) {
            bulkReclaim();
            return;
        }
    }
}

void hazptr_domain::bulkReclaim()
{
    auto p = retired_.exchange(nullptr);
    if (p == nullptr) {
        return;  // nothing to do
    }

    std::unordered_set<const void*> hazards;
    for (hazard_pointer *h = hazptrs_.load(); h != nullptr; h = h->next_) {
        hazards.insert(h->hazptr_.load());
    }

    int number_of_unreclaimed_items = 0;
    hazptr_head *first_unreclaimed = nullptr;
    hazptr_head *last_unreclaimed = nullptr;
    while (p) {
        hazptr_head *next = p->next_;
        if (hazards.count(static_cast<const void *>(p)) == 0) {
            (p->reclaim_)(p);
        } else {
            p->next_ = first_unreclaimed;
            first_unreclaimed = p;
            if (last_unreclaimed == nullptr) {
                last_unreclaimed = p;
            }
            ++number_of_unreclaimed_items;
        }
        p = next;
    }
    if (number_of_unreclaimed_items) {
        pushRetired(first_unreclaimed, last_unreclaimed, number_of_unreclaimed_items);
    }
}

int hazptr_domain::pushRetired(hazptr_head *head, hazptr_head *tail, int count)
{
    tail->next_ = retired_.load();
    while (!retired_.compare_exchange_weak(tail->next_, head)) {
        // spin
    }
    return rcount_.fetch_add(count);
}

} // namespace hazptr
} // namespace std
