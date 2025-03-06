#ifndef UTIL_STATUS_STATUS_MATCHERS_HPP_
#define UTIL_STATUS_STATUS_MATCHERS_HPP_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/status/status_macros.hpp"

namespace util::status {

namespace internal {
template <typename T>
struct MatcherCastHelper {
  using Type = T;
  static T const& Cast(T const& value) { return value; }
};

template <typename T, std::size_t N>
struct MatcherCastHelper<T[N]> {
  using Type = const T*;
  static const T* Cast(const T (&value)[N]) { return value; }
};

template <typename T>
using LocalMatcherType = typename MatcherCastHelper<T>::Type;

template <typename T>
auto LocalMatcherCast(T const& value) {
  return MatcherCastHelper<T>::Cast(value);
}

template <class ValueMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(ValueMatcher const& value_matcher)
      : value_matcher_(value_matcher) {}

  template <class StatusType>
  operator testing::Matcher<StatusType>() const {
    return testing::Matcher<StatusType>(new Impl<StatusType>(value_matcher_));
  }

 private:
  template <class StatusType>
  class Impl : public testing::MatcherInterface<StatusType> {
   public:
    using ValueType = typename std::decay_t<StatusType>::value_type;
    explicit Impl(ValueMatcher const& value_matcher)
        : value_matcher_(testing::MatcherCast<ValueType>(value_matcher)) {}

    void DescribeTo(std::ostream* os) const override {
      *os << "is OK and holds a value that ";
      value_matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(std::ostream* os) const override {
      *os << "is not OK and holds a value that ";
      value_matcher_.DescribeTo(os);
    }

    bool MatchAndExplain(
        const StatusType& status_or,
        testing::MatchResultListener* listener) const override {
      if (!status_or.ok()) {
        *listener << "which is not OK: " << status_or.status();
        return false;
      }
      *listener << "whose value ";
      if (!value_matcher_.MatchAndExplain(status_or.value(), listener)) {
        return false;
      }
      return true;
    }

   private:
    testing::Matcher<ValueType> value_matcher_;
  };

  const ValueMatcher value_matcher_;
};

}  // namespace internal

class IsOkImpl {
 public:
  void DescribeTo(std::ostream* os) const { *os << "is OK"; }
  void DescribeNegationTo(std::ostream* os) const { *os << "is not OK"; }
  template <class T>
  bool MatchAndExplain(T const& value,
                       testing::MatchResultListener* listener) const {
    if (!value.ok()) {
      *listener << "which is not OK";
      return false;
    }
    return true;
  }
};

inline testing::PolymorphicMatcher<IsOkImpl> IsOk() {
  return testing::MakePolymorphicMatcher(IsOkImpl());
}

class StatusIsImpl {};

inline testing::PolymorphicMatcher<StatusIsImpl> StatusIs(
    const absl::StatusCode& code,
    testing::Matcher<std::string> message = testing::_) {
  return testing::MakePolymorphicMatcher(StatusIsImpl());
}

template <class ValueMatcher>
internal::IsOkAndHoldsMatcher<internal::LocalMatcherType<ValueMatcher>>
IsOkAndHolds(ValueMatcher const& matcher) {
  return internal::IsOkAndHoldsMatcher<
      internal::LocalMatcherType<ValueMatcher>>(
      internal::LocalMatcherCast(matcher));
}

#define ASSERT_OK(x) ASSERT_THAT(x, ::util::status::IsOk())
#define ASSERT_OK_AND_ASSIGN_IMPL(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                              \
  ASSERT_OK(statusor) << statusor.status();    \
  lhs = std::move(statusor).value()
#define ASSERT_OK_AND_ASSIGN(lhs, rexpr)                                       \
  ASSERT_OK_AND_ASSIGN_IMPL(STATUS_MACROS_CONCAT_(_status_or_value, __LINE__), \
                            lhs, rexpr)

}  // namespace util::status

#endif