#ifndef UTIL_TYPES_ANY_STORAGE_HPP
#define UTIL_TYPES_ANY_STORAGE_HPP

#include <type_traits>
#include <utility>

namespace util {

// There are many storage types that work fine storing concrete types, such
// as pointers or plain objects, but when we want to work with references,
// the semantics for them do not work. For example, trying to use a
// reference with std::optional<> or std::variant<> will not work.
//
// This class provides a storage slot for any type, including references.
// References are stored as pointers, and the class provides a predictable
// way to access, move, and copy the stored value.
//
// The storage value itself will always copy the contents by value.
//
// This class does not provide common operators, but provides methods that
// do the equivalent to force these operations to be explicit.

template <class T>
class AnyStorage {
 public:
  using Type = T;
  using ConstType = T;
  using RefType = T&;
  using ConstRefType = T const&;
  using MoveType = T&&;
  using ConstMoveType = T const&&;
  using PtrType = T*;
  using ConstPtrType = T const*;

  constexpr AnyStorage() = default;

  // Create a value in place.
  template <class... Args>
  constexpr AnyStorage(std::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  explicit AnyStorage(T&& other) : value_(std::move(other)){};

  constexpr AnyStorage(AnyStorage const& other) = default;
  constexpr AnyStorage(AnyStorage&& other) = default;
  constexpr AnyStorage& operator=(AnyStorage const& other) = default;
  constexpr AnyStorage& operator=(AnyStorage&& other) = default;

  constexpr ConstRefType value() const& { return value_; }
  constexpr RefType value() & { return value_; }
  constexpr MoveType value() && { return std::move(value_); }

 private:
  T value_;
};

template <class T>
class AnyStorage<T&> {
 public:
  // With a reference type, the actual type is the reference. When thinking
  // about ref, moving, and the like, we are actually moving the reference,
  // not the reference contents.
  //
  // Because of this, _all_ derived types are references, aside from
  // pointers.
  using Type = T&;
  using ConstType = T const&;
  using RefType = T&;
  using ConstRefType = T const&;
  using MoveType = T&;
  using ConstMoveType = T const&;
  using PtrType = T*;
  using ConstPtrType = T const*;

  // No default constructor for references.
  static_assert(!std::is_default_constructible_v<T&>);

  // Create a reference to a value.
  constexpr explicit AnyStorage(T& value) : value_(&value) {}

  // To be compatible with the other constructors, we need to have a way to
  // create a reference using std::in_place_t.
  constexpr AnyStorage(std::in_place_t, T& value) : value_(&value) {}

  constexpr AnyStorage(AnyStorage const& other) = default;
  constexpr AnyStorage(AnyStorage&& other) = default;
  constexpr AnyStorage& operator=(AnyStorage const& other) = default;
  constexpr AnyStorage& operator=(AnyStorage&& other) = default;

  constexpr ConstRefType value() const& { return *value_; }
  constexpr RefType value() & { return *value_; }
  constexpr MoveType value() && { return std::move(*value_); }

 private:
  T* value_;
};

}  // namespace util

#endif