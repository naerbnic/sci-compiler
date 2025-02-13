#ifndef UTIL_TYPES_TMPL
#define UTIL_TYPES_TMPL

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace util {

// Forward declarations.
template <template <class...> class Tmpl>
struct TemplateTraits;

template <class... Args>
struct TypePack;

namespace internal {

template <class T, template <class...> class Tmpl>
struct SpecializationTraits : std::false_type {};

template <template <class...> class Tmpl, class... Args>
struct SpecializationTraits<Tmpl<Args...>, Tmpl> : std::true_type {
  using ArgsPack = TypePack<Args...>;
};

template <class T>
struct IsTemplateTraitImpl : std::false_type {};

template <template <class...> class Tmpl>
struct IsTemplateTraitImpl<TemplateTraits<Tmpl>> : std::true_type {};

}  // namespace internal

template <template <class...> class Tmpl>
struct TemplateTraits {
  // Is true if the class T is a specialization of the template.
  template <class T>
  static constexpr bool IsSpecialization =
      internal::SpecializationTraits<T, Tmpl>::value;

  // Applies the given type arguments to the template, giving a new type.
  template <class... Args>
  using Apply = Tmpl<Args...>;

  // If T is a specialization of the template, this is the type pack of the
  // parameters.
  template <class T>
    requires IsSpecialization<T>
  using SpecializationArgs =
      typename internal::SpecializationTraits<T, Tmpl>::ArgsPack;
};

template <class T>
concept IsTemplateTrait = internal::IsTemplateTraitImpl<T>::value;

template <class... Args>
struct TypePack {
  // The number of args in the arg pack.
  static constexpr std::size_t Size = sizeof...(Args);

  // Applies the arguments to the given template.
  template <template <class...> class Tmpl>
  using ApplyTemplate = Tmpl<Args...>;

  // Applies the arguments to the given instance of TemplateTraits.
  template <IsTemplateTrait TmplTrait>
  using ApplyTemplateTrait = TmplTrait::template Apply<Args...>;

  template <template <class> class Mod>
  using Transform = TypePack<Mod<Args>...>;

  template <std::size_t Idx>
  using TypeAt = std::tuple_element_t<Idx, std::tuple<Args...>>;
};

template <class T>
concept IsTypePack = TemplateTraits<TypePack>::IsSpecialization<T>;

template <class T, class TmplTrait>
concept IsSpecialization =
    IsTemplateTrait<TmplTrait> && TmplTrait::template IsSpecialization<T>;

}  // namespace util

#endif