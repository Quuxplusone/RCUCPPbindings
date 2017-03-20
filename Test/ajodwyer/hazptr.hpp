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
        domain.objRetire(this);
    }
};

template <typename T>
class hazptr_owner {
    void *domain_;  // points to a Domain
    void *hazptr_;  // points to a Domain::hazard_pointer
    void (*hazptrSet_)(hazptr_owner&, const void *);
    void (*hazptrRelease_)(hazptr_owner&);
  public:
    template<typename Domain>
    explicit hazptr_owner(Domain& d) : domain_(&d) {
        hazptr_ = d.hazptrAcquire();
        hazptrSet_ = +[](hazptr_owner& self, const void *p) {
            return static_cast<Domain*>(self.domain_)->hazptrSet(
                static_cast<typename Domain::hazard_pointer *>(self.hazptr_),
                p
            );
        };
        hazptrRelease_ = +[](hazptr_owner& self) {
            return static_cast<Domain*>(self.domain_)->hazptrRelease(
                static_cast<typename Domain::hazard_pointer *>(self.hazptr_)
            );
        };
    }

    ~hazptr_owner() {
        this->hazptrRelease_(*this);
    }

    // This class is fundamentally moveable, but if we make it moveable, we need to
    // provide for an "empty" or "moved-from" state. Right now it's analogous to
    // std::lock_guard.
    hazptr_owner(const hazptr_owner&) = delete;

    void set(const T *p) noexcept { this->hazptrSet_(*this, p); }
    void reset() noexcept { this->hazptrSet_(*this, nullptr); }

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
        std::swap(this->hazptrSet_, rhs.hazptrSet_);
        std::swap(this->hazptrRelease_, rhs.hazptrRelease_);
    }
};

template <typename T>
inline void swap(hazptr_owner<T>& lhs, hazptr_owner<T>& rhs) noexcept {
    lhs.swap(rhs);
}

} // namespace hazptr
} // namespace std
