#ifndef UTIL_TYPES_STRONG_TYPES_HPP
#define UTIL_TYPES_STRONG_TYPES_HPP

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace util {

namespace internal::strong_types {

/**
 * @class InheritIf
 * @brief A utility class used for conditional inheritance.
 *
 * This class template is used to conditionally inherit from a base class
 * depending on a boolean template parameter. If the boolean parameter is true,
 * the class inherits from the specified base class; otherwise, it does not.
 *
 * @tparam Condition A boolean template parameter that determines whether to
 * inherit from the base class.
 * @tparam Base The base class to inherit from if Condition is true.
 */
template <bool Condition, class Base>
class InheritIf;

template <class Base>
class InheritIf<true, Base> : public Base {
 protected:
  ~InheritIf() = default;
};
template <class Base>
class InheritIf<false, Base> {
 protected:
  ~InheritIf() = default;
};

template <class Tag>
concept IsTag = requires {
  typename Tag::Value;
  { Tag::is_const } -> std::convertible_to<bool>;
};

template <class Tag>
concept IsTagWithView = IsTag<Tag> && requires(Tag::ViewStorage const& storage,
                                               Tag::Value const& value) {
  // Value type must be default convertible to the view type.
  requires std::convertible_to<typename Tag::Value, typename Tag::View>;

  // A new value must be constructible from the view type.
  requires std::constructible_from<typename Tag::View, typename Tag::Value>;

  // The view storage must be able to be copied.
  requires std::copy_constructible<typename Tag::ViewStorage>;

  // Can generate the storage from a value const reference.
  {
    Tag::ValueToStorage(value)
  } -> std::convertible_to<typename Tag::ViewStorage>;

  // Can create a view from a storage const reference.
  { Tag::StorageToView(storage) } -> std::convertible_to<typename Tag::View>;
};

template <class Tag>
  requires IsTag<Tag>
struct TagInfo {
  constexpr static bool is_const = Tag::is_const;
  constexpr static bool is_copyable =
      std::copy_constructible<typename Tag::Value> &&
      std::is_copy_assignable_v<typename Tag::Value>;
  constexpr static bool is_movable =
      std::move_constructible<typename Tag::Value> &&
      std::is_move_assignable_v<typename Tag::Value>;
  constexpr static bool is_default_constructable =
      std::is_default_constructible_v<typename Tag::Value>;
  constexpr static bool has_view = IsTagWithView<Tag>;
};

}  // namespace internal::strong_types

// Forward declaration for views.
//
// This has to happen after the definition of IsTagWithView, but before the
// definition of ViewExtValueBase.
template <class Tag>
  requires internal::strong_types::IsTagWithView<Tag>
class StrongView;

namespace internal::strong_types {

// A base class that is conditionally inherited if the tag has a view.
template <class Tag>
class ViewExtValueBaseImpl {
 protected:
  ~ViewExtValueBaseImpl() = default;

 public:
  using View = StrongView<Tag>;
};

template <class Tag>
using ViewExtValueBase =
    InheritIf<TagInfo<Tag>::has_view, ViewExtValueBaseImpl<Tag>>;

}  // namespace internal::strong_types

// A tag type where the view contains a reference.
template <class T>
struct ReferenceTag {
  using Value = T;
  using View = T const&;
  using ViewStorage = T const*;

  static constexpr bool is_const = false;

  static constexpr ViewStorage ValueToStorage(Value const& value) {
    return &value;
  }

  static constexpr View StorageToView(ViewStorage storage) { return *storage; }
};

// This is a base class using the CRTP to establish a strong type value. It
// wraps a simple data type, and ensures that the type has a separate type
// identity from other types.
//
// Each independent type should define a Tag type. This is used to provide a
// spec of the type, such as the contained value, and the view type.
//
// These are the fields that the Tag type must define:
//
// - type Value: The type of the value that is stored in the strong type.
// - bool is_const: A boolean that says if the values are constant. If true,
//   this disabled the ability to mutate the value.
//
// In addition, you can add a View type if you add the following fields to the
// Tag type:
//
// - type View: The type of the view that is returned from the view.
// - type ViewStorage: The type of the storage that is used to store the view.
//   This may be different from the view type.
// - static ViewStorage ValueToStorage(Value const& value): A function that
//   creates a view storage from a value.
// - static View StorageToView(ViewStorage storage): A function that creates a
//   view from the view storage.
template <class Tag>
  requires internal::strong_types::IsTag<Tag>
class StrongValue : public internal::strong_types::ViewExtValueBase<Tag> {
  using Value = typename Tag::Value;

 public:
  static constexpr StrongValue Create(Value value) {
    return StrongValue(std::move(value));
  }

  constexpr StrongValue(StrongValue const&) = default;
  constexpr StrongValue(StrongValue&&) = default;

  constexpr ~StrongValue() = default;

  constexpr StrongValue& operator=(StrongValue const&) = default;
  constexpr StrongValue& operator=(StrongValue&&) = default;

  constexpr Value const& value() const& { return value_; }

  // These versions of value() are disabled if is_const is true.
  constexpr Value& value() &
    requires(!Tag::is_const)
  {
    return value_;
  }

  constexpr Value&& value() &&
    requires(!Tag::is_const)
  {
    return std::move(value_);
  }

  constexpr bool operator==(StrongValue const& other) const
    requires std::equality_comparable<Value>
  {
    return value_ == other.value_;
  }

  constexpr auto operator<=>(StrongValue const& other) const
    requires std::three_way_comparable<Value>
  {
    return value_ <=> other.value_;
  }

 private:
  template <class Tag2>
    requires internal::strong_types::IsTagWithView<Tag2>
  friend class StrongView;

  constexpr explicit StrongValue(Value value) : value_(std::move(value)) {}
  Value value_;
};

template <class Tag>
  requires internal::strong_types::IsTagWithView<Tag>
class StrongView {
  using View = typename Tag::View;

 public:
  static constexpr StrongView<Tag> Create(View value) {
    return StrongView<Tag>::Create(std::move(value));
  }

  StrongView(StrongValue<Tag> const& value)
      : view_(Tag::ValueToStorage(value.value())) {}

  StrongView(StrongView const&) = default;
  StrongView(StrongView&&) = default;

  ~StrongView() = default;

  StrongView& operator=(StrongView const&) = default;
  StrongView& operator=(StrongView&&) = default;

  constexpr View value() const& { return Tag::StorageToView(view_); }

  constexpr bool operator==(StrongValue<Tag> const& other) const
    requires std::equality_comparable_with<typename Tag::View,
                                           typename Tag::Value>
  {
    return view_ == other.value_;
  }

  constexpr auto operator<=>(StrongValue<Tag> const& other) const
    requires std::three_way_comparable_with<typename Tag::View,
                                            typename Tag::Value>
  {
    return view_ <=> other.value_;
  }

  constexpr bool operator==(StrongView<Tag> const& other) const
    requires std::equality_comparable<typename Tag::View>
  {
    return view_ == other.value_;
  }

  constexpr auto operator<=>(StrongView<Tag> const& other) const
    requires std::three_way_comparable<typename Tag::View>
  {
    return view_ <=> other.value_;
  }

 private:
  Tag::ViewStorage view_;
};
}  // namespace util

template <class Tag>
class std::hash<util::StrongValue<Tag>> {
 public:
  std::size_t operator()(util::StrongValue<Tag> const& value) const {
    return std::hash<typename Tag::Value>{}(value.value());
  }
};

template <class Tag>
class std::hash<util::StrongView<Tag>> {
 public:
  std::size_t operator()(util::StrongView<Tag> const& view) const {
    return std::hash<typename Tag::View>{}(view.value());
  }
};

#endif