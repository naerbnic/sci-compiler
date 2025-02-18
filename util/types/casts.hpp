#ifndef CASTS_H_
#define CASTS_H_

#include <memory>

namespace casts_impl {

template <class T>
struct PtrTraits {};

template <class T>
struct PtrTraits<T*> {
  using pointer = T*;
  using element_type = T;
  template <class U>
  using rebind = U*;

  template <class U>
  static rebind<U> static_pointer_cast(pointer ptr) {
    return static_cast<U*>(ptr);
  }
};

template <class T>
struct PtrTraits<std::unique_ptr<T>> {
  using pointer = std::unique_ptr<T>;
  using element_type = T;

  template <class U>
  using rebind = std::unique_ptr<U>;

  template <class U>
  static rebind<U> static_pointer_cast(pointer ptr) {
    return std::unique_ptr<U>(static_cast<U*>(ptr.release()));
  }
};

template <class T>
struct PtrTraits<std::shared_ptr<T>> {
  using pointer = std::shared_ptr<T>;
  using element_type = T;

  template <class U>
  using rebind = std::shared_ptr<U>;
};

template <template <class...> class Ptr, class T, class... Args>
struct PtrTraits<Ptr<T, Args...>> {
  using pointer = Ptr<T, Args...>;
  using element_type = T;

  template <class U>
  using rebind = Ptr<U, Args...>;

  template <class U>
  static rebind<U> static_pointer_cast(pointer ptr) {
    return std::move(ptr).template cast_to<U>();
  }
};

}  // namespace casts_impl

template <class U, class Ptr>
auto down_cast(Ptr ptr) {
  using traits = casts_impl::PtrTraits<Ptr>;
  static_assert(std::convertible_to<U*, typename traits::element_type*>);
  static_assert(std::convertible_to<typename traits::template rebind<U>, Ptr>);
  return traits::template static_pointer_cast<U>(ptr);
}

#endif