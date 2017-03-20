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

class hazptr_domain_base {
public:
    using hazard_pointer = void;

    hazptr_domain_base() noexcept = default;
    hazptr_domain_base(const hazptr_domain_base&) = delete;
    virtual ~hazptr_domain_base() = default;

    virtual hazard_pointer *acquire() = 0;
    virtual void release(hazard_pointer *) = 0;
    virtual void set(hazard_pointer *, const void *) noexcept = 0;

    virtual void retire(hazptr_head *) = 0;
};

template<class Domain>
class hazptr_domain_wrapper : public hazptr_domain_base {
    Domain *d;
    using wrapped_hazard_pointer = typename Domain::hazard_pointer;
    static auto W(void *hp) noexcept {
        return static_cast<wrapped_hazard_pointer *>(hp);
    }
public:
    hazptr_domain_wrapper(Domain& d) noexcept : d(&d) {}

    hazard_pointer *acquire() override { return (void *)d->acquire(); }
    void release(hazard_pointer *hp) override { d->release(W(hp)); }
    void set(hazard_pointer *hp, const void *p) noexcept override { d->set(W(hp), p); }

    void retire(hazptr_head *obj) override { d->retire(obj); }
};

/** hazptr_obj_base: Base template for objects protected by hazard pointers. */
template <typename T, typename Deleter = std::default_delete<T>>
class hazptr_obj_base : private hazptr_head {
    Deleter deleter_;
  public:
    template<typename Domain>
    void retire(
        Domain& domain,
        Deleter deleter = {}
    ) {
        deleter_ = std::move(deleter);
        reclaim_ = [](hazptr_head *p) {
            auto hobp = static_cast<hazptr_obj_base*>(p);
            auto obj = static_cast<T*>(hobp);
            hobp->deleter_(obj);
        };
        domain.retire(this);
    }
};

template <typename T>
class hazptr_owner {
    // This type-erasure is expensive. But we could easily avoid the memory allocation if we wanted to.
    std::unique_ptr<hazptr_domain_base> domain_;
    hazptr_domain_base::hazard_pointer *hazptr_;
  public:
    template<typename Domain>
    explicit hazptr_owner(Domain& d) : domain_(new hazptr_domain_wrapper<Domain>(d)) {
        hazptr_ = domain_->acquire();
    }

    ~hazptr_owner() {
        domain_->release(hazptr_);
    }

    // This class is fundamentally moveable, but if we make it moveable, we need to
    // provide for an "empty" or "moved-from" state. Right now it's analogous to
    // std::lock_guard.
    hazptr_owner(const hazptr_owner&) = delete;

    void set(const T *p) noexcept { domain_->set(hazptr_, p); }
    void reset() noexcept { domain_->set(hazptr_, nullptr); }

    /** Hazard pointer operations */
    /* Returns a protected pointer from the source */
    template <typename Atomic>
    T *get_protected(const Atomic& src) noexcept {
        T *p = src.load();
        while (!try_protect(p, src)) {
            // spin
        }
        return p;
    }

    /* Return true if successful in protecting ptr if src == ptr after
     * setting the hazard pointer.  Otherwise sets ptr to src. */
    template <typename A>
    bool try_protect(T*& ptr, const A& src) noexcept {
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
