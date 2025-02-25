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
// All types returned can be correctly handled by adding the correct reference
// extension to T, and allow for reference collapsing. The one exception is
// if you want the type T by value, such as for a function return by value. In
// that case, using std::remove_cvref_t<T> will give the correct type.

template <class T>
class AnyStorage {
 public:
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

  constexpr T const& value() const& { return value_; }
  constexpr T& value() & { return value_; }
  constexpr T const&& value() const&& { return value_; }
  constexpr T&& value() && { return value_; }

 private:
  T value_;
};

// Specialization for LValue References
//
// Note that constness can be implied through the type of T.
template <class T>
class AnyStorage<T&> {
 public:
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

  constexpr T const& value() const& { return *value_; }
  constexpr T& value() & { return *value_; }
  constexpr T const& value() const&& { *value_; }
  constexpr T& value() && { return *value_; }

 private:
  T* value_;
};

template <class T>
class AnyStorage<T&&> {
 public:
  // No default constructor for references.
  static_assert(!std::is_default_constructible_v<T&&>);

  // Create a reference to a value.
  //
  // Since this is converting the storage of the value, we can ignore the
  // conversion to a pointer.
  //
  // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
  constexpr explicit AnyStorage(T&& value) : value_(&value) {}

  // To be compatible with the other constructors, we need to have a way to
  // create a reference using std::in_place_t.
  //
  // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
  constexpr AnyStorage(std::in_place_t, T&& value) : value_(&value) {}

  constexpr AnyStorage(AnyStorage const& other) = default;
  constexpr AnyStorage(AnyStorage&& other) = default;
  constexpr AnyStorage& operator=(AnyStorage const& other) = default;
  constexpr AnyStorage& operator=(AnyStorage&& other) = default;

  constexpr T const& value() const& { return *value_; }
  constexpr T& value() & { return *value_; }
  constexpr T const&& value() const&& { return std::move(*value_); }
  constexpr T&& value() && { return std::move(*value_); }

 private:
  T* value_;
};

}  // namespace util

#endif