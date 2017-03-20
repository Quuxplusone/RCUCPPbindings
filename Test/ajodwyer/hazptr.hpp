#pragma once

#include <atomic>
#include <memory>

/** Definition of ::hazptr_head, analogous to ::rcu_head */
struct hazptr_head {
  void (*reclaim_)(hazptr_head *);
  hazptr_head *next_;
};

namespace std {
namespace hazptr {

class hazptr_domain
{
    class hazard_pointer
    {
        std::atomic<const void *> hazptr_;
        hazard_pointer *next_;
        std::atomic<bool> active_;

        friend class hazptr_domain;
      public:
        hazard_pointer() : hazptr_(nullptr), next_(nullptr), active_(false) {}
        void set(const void *p) noexcept { hazptr_.store(p); }
        const void *get() const noexcept { return hazptr_.load(); }
        void release() noexcept { set(nullptr); active_.store(false); }
    };

  public:
    constexpr explicit hazptr_domain() noexcept = default;
    hazptr_domain(const hazptr_domain&) = delete;
    ~hazptr_domain();

    void retire(hazptr_head *);

  private:
    template<class> friend class hazptr_owner;

    hazard_pointer *acquire();
    void release(hazard_pointer *hp) noexcept { hp->release(); }
    void set(hazard_pointer *hp, const void *p) noexcept { hp->set(p); }

    void tryBulkReclaim();
    void bulkReclaim();
    int pushRetired(hazptr_head *head, hazptr_head *tail, int count);

    std::atomic<hazard_pointer *> hazptrs_ = {nullptr};
    std::atomic<hazptr_head *> retired_ = {nullptr};
    std::atomic<int> hcount_ = {0};
    std::atomic<int> rcount_ = {0};
};

// All hazptr-protected data structures must derive from std::hazptr::enable_retire_on_this,
// which derives privately from ::hazptr_head.

template <typename T, typename Deleter = std::default_delete<T>>
class enable_retire_on_this : private hazptr_head {
    Deleter deleter_;
  public:
    void retire(
        hazptr_domain& domain,
        Deleter deleter = {}
    ) {
        deleter_ = std::move(deleter);
        reclaim_ = [](hazptr_head *p) {
            auto hobp = static_cast<enable_retire_on_this*>(p);
            auto obj = static_cast<T*>(hobp);
            hobp->deleter_(obj);
        };
        domain.retire(this);
    }
};

template <typename T = void>
class hazptr_owner {
    hazptr_domain *domain_;
    hazptr_domain::hazard_pointer *hazptr_;

    template<class> friend class hazptr_owner;  // for move-assignment
  public:
    constexpr explicit hazptr_owner() noexcept : domain_(nullptr), hazptr_(nullptr) {}

    explicit hazptr_owner(hazptr_domain& d) : domain_(&d) {
        hazptr_ = domain_->acquire();
    }

    hazptr_owner(const hazptr_owner&) = delete;
    hazptr_owner& operator= (const hazptr_owner&) = delete;

    hazptr_owner(hazptr_owner&& rhs) : domain_(rhs.domain_), hazptr_(rhs.hazptr_) {
        rhs.domain_ = nullptr;
        rhs.hazptr_ = nullptr;
    }
    hazptr_owner& operator= (hazptr_owner&& rhs) {
        // Explicitly protect against self-assignment, so as to avoid leaks.
        if (this != &rhs) {
            if (domain_) {
                domain_->release(hazptr_);
            }
            domain_ = rhs.domain_; rhs.domain_ = nullptr;
            hazptr_ = rhs.hazptr_; rhs.hazptr_ = nullptr;
        }
        return *this;
    }

    template<class U, typename = std::enable_if_t<std::is_convertible<U*,T*>::value>>
    hazptr_owner(hazptr_owner<U>&& rhs) : domain_(rhs.domain_), hazptr_(rhs.hazptr_) {
        rhs.domain_ = nullptr;
        rhs.hazptr_ = nullptr;
    }

    ~hazptr_owner() {
        if (domain_) {
            domain_->release(hazptr_);
        }
    }

    void set(const T *p) { domain_->set(hazptr_, p); }
    void reset() { domain_->set(hazptr_, nullptr); }

    /** Hazard pointer operations */
    /* Returns a protected pointer from the source */
    template <typename Atomic>
    T *get_protected(const Atomic& src) {
        T *p = src.load();
        while (!try_protect(p, src)) {
            // spin
        }
        return p;
    }

    /* Return true if successful in protecting ptr if src == ptr after
     * setting the hazard pointer.  Otherwise sets ptr to src. */
    template <typename A>
    bool try_protect(T*& ptr, const A& src) {
        this->set(ptr);
        T* p = src.load();
        if (p == ptr) {
            return true;
        } else {
            ptr = p;
            this->reset();
            return false;
        }
    }

    /* Swap ownership of hazard pointers between hazptr_owner-s. */
    /* Note: The owned hazard pointers remain unmodified during the swap
     * and continue to protect the respective objects that they were
     * protecting before the swap, if any. */
    void swap(hazptr_owner& rhs) noexcept {
        std::swap(this->domain_, rhs.domain_);
        std::swap(this->hazptr_, rhs.hazptr_);
    }
};

template <typename T>
inline void swap(hazptr_owner<T>& lhs, hazptr_owner<T>& rhs) noexcept {
    lhs.swap(rhs);
}

} // namespace hazptr
} // namespace std
