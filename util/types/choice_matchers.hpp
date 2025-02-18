#ifndef UTIL_CHOICE_MATCHERS_HPP
#define UTIL_CHOICE_MATCHERS_HPP

#include <ostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "util/types/name.hpp"

namespace util {

namespace internal {
template <class ChoiceT>
class ChoiceOfImpl {
 public:
  explicit ChoiceOfImpl(testing::Matcher<ChoiceT> const& matcher)
      : matcher_(matcher) {}

  void DescribeTo(std::ostream* os) const {
    *os << "is choice of type " << TypeName<ChoiceT>() << " that ";
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "is not a choice of type " << TypeName<ChoiceT>() << " that ";
    matcher_.DescribeTo(os);
  }

  template <class T>
  bool MatchAndExplain(T const& value,
                       testing::MatchResultListener* listener) const {
    if (!value.template has<std::decay_t<ChoiceT>>()) {
      *listener << "is not a choice of type " << TypeName<ChoiceT>();
      return false;
    }
    return matcher_.MatchAndExplain(value.template as<std::decay_t<ChoiceT>>(),
                                    listener);
  }

 private:
  testing::Matcher<ChoiceT> matcher_;
};

}  // namespace internal

template <class ChoiceT>
testing::PolymorphicMatcher<internal::ChoiceOfImpl<ChoiceT>> ChoiceOf(
    testing::Matcher<ChoiceT> const& matcher) {
  return testing::MakePolymorphicMatcher(
      internal::ChoiceOfImpl<ChoiceT>(matcher));
}

}  // namespace util
#endif