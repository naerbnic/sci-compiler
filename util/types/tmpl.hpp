#ifndef UTIL_TYPES_TMPL
#define UTIL_TYPES_TMPL

#include <cstddef>
#include <type_traits>

namespace util {

// Forward declarations.
template <template <class...> class Tmpl>
struct TemplateTraits;

namespace internal {

template <class T, template <class...> class Tmpl>
struct IsSpecialization : std::false_type {};

template <template <class...> class Tmpl, class... Args>
struct IsSpecialization<Tmpl<Args...>, Tmpl> : std::true_type {};

template <class T>
struct IsTemplateTraitImpl : std::false_type {};

template <template <class...> class Tmpl>
struct IsTemplateTraitImpl<TemplateTraits<Tmpl>> : std::true_type {};

}  // namespace internal

template <template <class...> class Tmpl>
struct TemplateTraits {
  template <class T>
  static constexpr bool IsSpecialization =
      internal::IsSpecialization<T, Tmpl>::value;

  template <class... Args>
  using Apply = Tmpl<Args...>;
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
};

template <class T>
concept IsTypePack = TemplateTraits<TypePack>::IsSpecialization<T>;

template <class T, class TmplTrait>
concept IsSpecialization =
    IsTemplateTrait<TmplTrait> && TmplTrait::template IsSpecialization<T>;

}  // namespace util

#endif