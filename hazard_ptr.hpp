#include <atomic>
#include <functional>
#include <memory>
#include <experimental/memory_resource>

namespace std {

/* Control block ­ One per domain */
class hazard_domain;

extern hazard_domain default_hazard_domain;

/** enable_hazard_ptr
 *
 * Base class template for objects protected by hazard pointers.
 */
template <typename T, typename Allocator = std::allocator<T>>
class enable_hazard_ptr {
  /* Pointer used in constructing lists of removed objects awaiting
     reclamation, without requiring additional allocation. */
  enable_hazard_ptr *next_removed_;

  /* Pointer to allocator to be used to reclaim object. */
  Allocator *alloc_;
};

/** hazard_ptr
 *
 * Guard class template for RAII automatic allocation and release of hazard
 * pointers, and interface for user calls to hazard pointer functions.
 */
template<typename T, typename Allocator = std::allocator<T>>
class hazard_ptr {
public:
  enum tc_policy { cache, nocache };

  hazard_ptr(const hazard_ptr&) = delete;
  hazard_ptr(hazard_ptr&&) = delete;
  hazard_ptr& operator=(const hazard_ptr&) = delete;
  hazard_ptr& operator=(hazard_ptr&&) = delete;

  hazard_ptr(tc_policy tc = tc_policy::cache, hazard_domain *control_block = &default_hazard_domain);
  ~hazard_ptr();

  bool protect(const T *ptr, const std::atomic<T *>& src) noexcept;
  void set(const T *ptr) noexcept;
  void clear() noexcept;

  void swap(hazard_ptr& other) noexcept;
};

void swap(hazard_ptr& a, hazard_ptr& b) noexcept
{
  return a.swap(b);
}

/** hazard_domain
 *
 * Control block for hazard pointers. One per domain.
 */
class hazard_domain {
public:
  enum rem_policy { priv, shared };

  hazard_domain(const hazard_domain&) = delete;
  hazard_domain(hazard_domain&&) = delete;
  hazard_domain& operator=(const hazard_domain&) = delete;
  hazard_domain& operator=(hazard_domain&&) = delete;

  constexpr hazard_domain(std::pmr::memory_resource *);

  ~hazard_domain();

  template<typename T, typename Allocator>
  void reclaim(enable_hazard_ptr<T, Allocator> *ptr,
               rem_policy rem = rem_policy::priv);
};

} // namespace std
