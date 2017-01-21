#pragma once

#include <memory>
#include "rcu.hpp"

// Class template std::rcu::unique_ptr<T> is provided in the std namespace, but it
// can be implemented "user-side" if need be; it does not have to be part of an
// initial proposal. Notice that std::rcu::unique_ptr<T> currently does not support
// any deleter types other than default_deleter<T>, and retires its controlled object
// only when the std::rcu::unique_ptr<T> loses its value (e.g. on destruction
// or reassignment). Manually retiring the pointer with a custom behavior is not
// supported (yet?).

namespace std {
namespace rcu {

template<class T>
class unique_ptr {
    template<class T2, class... Args> friend unique_ptr<T2> make_unique(Args&&... args);

    struct pointee : std::rcu::enable_retire_on_this<pointee> {
        T t;
        template<class... Args> pointee(Args&&... args) : t(std::forward<Args>(args)...) {}
    };
    struct retirer {
        void operator()(pointee *p) const { p->retire(); }
    };
    std::unique_ptr<pointee, retirer> ptr;
    unique_ptr(pointee *p): ptr(p) {}  // private constructor
  public:
    unique_ptr() = default;
    T* get() const { return &ptr->t; }
    T& operator*() const { return ptr->t; }
    T* operator->() const { return &ptr->t; }
    unique_ptr& operator=(decltype(nullptr)) { ptr = nullptr; return *this; }
};

template<class T, class... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new typename unique_ptr<T>::pointee(std::forward<Args>(args)...));
};

}} // namespace std::rcu
