#include <memory>
#include <type_traits>

namespace std {

/* Control block ­ One per domain */
class hazard_domain;

extern hazard_domain default_hazard_domain;

/** enable_hazard_ptr
 *
 * Base class template for objects protected by hazard pointers.
 */
// I actually can't see any reason to depend on type T here.
// This could be a plain old base class, not a CRTP template?
template <typename T>
class enable_hazard_ptr {
  /* Pointer used in constructing lists of removed objects awaiting
     reclamation, without requiring additional allocation. */
  // This is analogous to struct rcu_head in <urcu.h>.
  // We might also need a single function pointer here, raising the overhead
  // to 16 bytes per enable_hazard_ptr object; that's what <urcu.h> does.
  void *next_removed_;
};

/** hazard_ptr
 *
 * Guard class template for RAII automatic allocation and release of hazard
 * pointers, and interface for user calls to hazard pointer functions.
 */
template<typename T>
class hazard_ptr {
  static_assert(is_base_of<enable_hazard_ptr<T>, T>::value);

public:
  // hazard_ptr is a move-only type.
  hazard_ptr(hazard_ptr&&) noexcept;
  hazard_ptr& operator=(hazard_ptr&&) noexcept;

  hazard_ptr(hazptr_domain& domain = default_hazard_domain);
  ~hazard_ptr();

  void protect(const T *ptr) noexcept;
  void reset() noexcept;

  void swap(hazard_ptr&) noexcept;
};

template<typename T>
void swap(hazard_ptr<T>& a, hazard_ptr<T>& b) noexcept
{
  return a.swap(b);
}

/** hazard_domain
 *
 * Control block for hazard pointers. One per domain.
 */
class hazard_domain {
public:
  // hazard_domain is a non-moveable type.
  hazard_domain(const hazard_domain&) = delete;

  // The user can pass in an allocator used for the hazard pointer arrays themselves.
  // We can store this allocator type-erasedly, since allocation is rare enough that
  // we don't care about its cost.
  template<typename Allocator = std::allocator<void*>>
  constexpr hazard_domain(Allocator alloc = {});

  ~hazard_domain();

  // Equivalent to d(ptr), except deferred until there are no hazard pointers
  // remaining in this domain that still refer to ptr.
  template<typename T, typename Deleter = default_delete<T>>
  void retire(T *ptr, Deleter d = {});
};

} // namespace std
