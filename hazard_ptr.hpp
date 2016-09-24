#include <atomic>
#include <functional>
#include <memory>
#include <experimental/memory_resource>

/* Control block ­ One per domain */
class haz_ptr_control_block;

extern haz_ptr_control_block default_haz_ptr_control_block;

/** haz_ptr_obj
 *
 * Base class template for objects protected by hazard pointers.
 */
template <typename T, typename Allocator = std::allocator<T>>
class haz_ptr_obj {
  /* Pointer used in constructing lists of removed objects awaiting
     reclamation, without requiring additional allocation. */
  haz_ptr_obj *next_removed_;

  /* Pointer to allocator to be used to reclaim object. */
  Allocator *alloc_;
};

/** haz_ptr_guard
 *
 * Guard class template for RAII automatic allocation and release of hazard
 * pointers, and interface for user calls to hazard pointer functions.
 */
template<typename T, typename Allocator = std::allocator<T>>
class haz_ptr_guard {
public:
  enum tc_policy { cache, nocache };

  haz_ptr_guard(haz_ptr_guard&) = delete;
  haz_ptr_guard(haz_ptr_guard&&) = delete;
  haz_ptr_guard& operator=(haz_ptr_guard&) = delete;
  haz_ptr_guard& operator=(haz_ptr_guard&&) = delete;

  haz_ptr_guard(tc_policy tc = tc_policy::cache,
                haz_ptr_control_block *control_block
                  = &default_haz_ptr_control_block
                );

  ~haz_ptr_guard();

  bool protect(const T *ptr, const std::atomic<T *>& src) noexcept;
  void set(const T *ptr) noexcept;
  void clear() noexcept;

  static void swap(haz_ptr_guard& a, haz_ptr_guard& b) noexcept;
};

/** haz_ptr_control_block
 *
 * Control block for hazard pointers. One per domain.
 */
class haz_ptr_control_block {
public:
  enum rem_policy { priv, shared };

  haz_ptr_control_block(haz_ptr_control_block&) = delete;
  haz_ptr_control_block(haz_ptr_control_block&&) = delete;
  haz_ptr_control_block& operator=(haz_ptr_control_block&) = delete;
  haz_ptr_control_block& operator=(haz_ptr_control_block&&) = delete;

  constexpr haz_ptr_control_block(std::pmr::memory_resource *);

  ~haz_ptr_control_block();

  template<typename T, typename Allocator = std::allocator<T>>
  void reclaim(haz_ptr_obj<T, Allocator> *ptr,
               rem_policy rem = rem_policy::priv);
};
