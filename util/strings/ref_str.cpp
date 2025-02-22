#include "util/strings/ref_str.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <string_view>
#include <utility>
#include <variant>

#include "util/types/overload.hpp"

namespace util {

namespace ref_str_literals {
RefStr operator""_rs(char const* str, std::size_t len) {
  return RefStr(std::in_place_type<std::string_view>,
                std::string_view(str, len));
}

}  // namespace ref_str_literals

class RefStr::Impl {
 public:
  static void* operator new(std::size_t size, std::string_view str) {
    auto impl = ::operator new(size + str.size());
    return impl;
  }

  static void operator delete(void* ptr, std::size_t size) {
    ::operator delete(ptr);
  }

  Impl(std::string_view str) : length_(str.size()) {
    std::copy(str.begin(), str.end(), data_);
  }

  std::string_view view() const { return {data_, length_}; }

 private:
  std::size_t length_;
  alignas(char) char data_[];
};

std::shared_ptr<RefStr::Impl> RefStr::MakeImpl(std::string_view str) {
  auto* impl_ptr = new (str) Impl(str);
  return std::shared_ptr<Impl>(impl_ptr);
}

RefStr::RefStr() : value_("") {}

RefStr::RefStr(std::string_view str) : value_(MakeImpl(str)) {}

std::string_view RefStr::view() const {
  return std::visit(
      util::Overload([](std::string_view str) { return str; },
                     [](std::shared_ptr<Impl> impl) { return impl->view(); }),
      value_);
}

RefStr::operator std::string_view() const { return view(); }

}  // namespace util