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

class hp_domain_folly
{
    class hazptr_record
    {
        std::atomic<const void *> hazptr_;
        hazptr_record *next_;
        std::atomic<bool> active_;

        friend class hp_domain_folly;
      public:
        hazptr_record() : hazptr_(nullptr), next_(nullptr), active_(false) {}
        void set(const void *p) noexcept { hazptr_.store(p); }
        const void *get() const noexcept { return hazptr_.load(); }
        void release() noexcept { set(nullptr); active_.store(false); }
    };

  public:
    using hazard_pointer = hazptr_record;

    constexpr explicit hp_domain_folly() noexcept = default;
    hp_domain_folly(const hp_domain_folly&) = delete;
    ~hp_domain_folly();

    hazard_pointer *acquire();
    void release(hazard_pointer *) noexcept;
    void set(hazard_pointer *rec, const void *p) noexcept { rec->set(p); }

    void retire(hazptr_head *);

  private:
    void tryBulkReclaim();
    void bulkReclaim();
    int pushRetired(hazptr_head *head, hazptr_head *tail, int count);

    std::atomic<hazard_pointer *> hazptrs_ = {nullptr};
    std::atomic<hazptr_head *> retired_ = {nullptr};
    std::atomic<int> hcount_ = {0};
    std::atomic<int> rcount_ = {0};
};
