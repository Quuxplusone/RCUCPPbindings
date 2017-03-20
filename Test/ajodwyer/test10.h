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
#include <stdexcept>
#include <utility>

template <typename T>
class LockFreeLIFO {
    class Node : public std::hazptr::enable_retire_on_this<Node> {
        friend LockFreeLIFO;
        T value_;
        Node *next_;
        Node(T v, Node *n) : value_(v), next_(n) {}
    };

  public:
    LockFreeLIFO(std::hazptr::hazptr_domain& d) : head_(nullptr), domain_(&d) {}
    LockFreeLIFO(const LockFreeLIFO&) = delete;
    ~LockFreeLIFO() = default;

    void push(T val) {
        Node *pnode = new Node(val, head_.load());
        while (!head_.compare_exchange_weak(pnode->next_, pnode)) {
            // spin
        }
    }

    T pop() {
        std::hazptr::hazptr_owner<Node> hptr(*domain_);
        Node *pnode = head_.load();
        while (true) {
            if (pnode == nullptr) {
                throw std::out_of_range("This LockFreeLIFO is empty");
            }
            if (!hptr.try_protect(pnode, head_)) {
                continue;
            }
            Node *next = pnode->next_;
            if (head_.compare_exchange_weak(pnode, next)) {
                break;
            }
        }
        hptr.reset();
        T val = std::move(pnode->value_);
        pnode->retire(*domain_);
        return val;
    }

  private:
    std::atomic<Node*> head_;
    std::hazptr::hazptr_domain *domain_;
};
