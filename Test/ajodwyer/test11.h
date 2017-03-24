#pragma once

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

#include <string.h>
template<class T>
struct DebugRelease {
    void operator()(T *t) const {
        memset(t, 0xAB, sizeof *t);
        delete t;
    }
};

/** Set implemented as an ordered singly-linked list.
 *
 *  A single writer thread may add or remove elements. Multiple reader
 *  threads may search the set concurrently with each other and with
 *  the writer's operations.
 */
template <typename T>
class SWMRListSet
{
    struct Node : public std::hazptr::enable_retire_on_this<Node, DebugRelease<Node>>
    {
        T elem_;
        std::atomic<Node *> next_;

        explicit Node(T e, Node *n) : elem_(e), next_(n) {}
    };

    std::atomic<Node *> head_ = {nullptr};
    std::hazptr::hazptr_domain *domain_;

    /* Used by the single writer; therefore we can assume no concurrent modifications are happening */
    void locate_lower_bound(const T& v, std::atomic<Node*>*& prev) const
    {
        Node *curr = prev->load();
        while (curr != nullptr && curr->elem_ < v) {
            prev = &curr->next_;
            curr = prev->load();
        }
    }

  public:
    explicit SWMRListSet(std::hazptr::hazptr_domain& d) : domain_(&d) {}

    ~SWMRListSet() {
        Node *next;
        for (Node *p = head_.load(); p; p = next) {
            next = p->next_.load();
            delete p;
        }
    }

    /* Used by the single writer; therefore we can assume no concurrent modifications are happening */
    bool add(T v) {
        auto *prev = &head_;
        locate_lower_bound(v, prev);
        Node *curr = prev->load();
        if (curr && curr->elem_ == v) {
            return false;
        }
        prev->store(new Node(std::move(v), curr));
        return true;
    }

    /* Used by the single writer; therefore we can assume no concurrent modifications are happening */
    bool remove(const T& v) {
        auto *prev = &head_;
        locate_lower_bound(v, prev);
        Node *curr = prev->load();
        if (!curr || curr->elem_ != v) {
            return false;
        }
        Node *curr_next = curr->next_.load();
        prev->store(curr_next);  // Patch up the actual list...
        curr->next_.store(nullptr);  // ...and only then null out the removed node.
        curr->retire(*domain_);
        return true;
    }

    /* Used by readers; therefore we must admit the possibility of concurrent modifications */
    bool contains(const T& val) const {
        /* Acquire two hazard pointers for hand-over-hand traversal. */
        std::hazptr::hazptr_owner<Node> hptr_prev(*domain_);
        std::hazptr::hazptr_owner<Node> hptr_curr(*domain_);
        while (true) {
            const std::atomic<Node *> *prev = &head_;
            Node *curr = prev->load();
            while (true) {
                if (curr == nullptr) {
                    return false;  // reached the end of the list
                }
                // If curr has been removed from the list (but can't yet have been reclaimed
                // because hptr_curr protects it), then *prev will point to some node later
                // in the list than curr (i.e. not curr), and this try_protect will fail.
                // If the node before curr has been removed from the list (but can't yet have
                // been reclaimed because hptr_prev protects it, or because it is "head"),
                // then *prev will be nullptr (i.e. not curr), and this try_protect will fail.
                // In either case, our list walk has led us away from the "real" list, and
                // we must restart our walk from the beginning.
                if (!hptr_curr.try_protect(curr, *prev)) {
                    break;
                }
                Node *next = curr->next_.load();  // NULL because curr has been removed
                if (next == nullptr) {
                    // If curr has just been removed, then next may be nullptr; but
                    // in that case *prev will already have been patched to point to the
                    // next unremoved node (i.e. not curr).
                    // If that's happened, then we must restart our walk.
                    if (prev->load() != curr) {
                        break;
                    }
                }
                if (curr->elem_ == val) {
                    return true;
                } else if (!(curr->elem_ < val)) {
                    return false;  // because the list is sorted
                }
                prev = &curr->next_;
                curr = next;
                swap(hptr_curr, hptr_prev);   // now hptr_prev protects the element containing *prev
            }
        }
    }
};
