#include "util/strings/ref_str.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <string_view>

namespace util {

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

RefStr::RefStr() : value_(nullptr) {}

RefStr::RefStr(std::string_view str) : value_(MakeImpl(str)) {}

std::string_view RefStr::view() const {
  if (!value_) return "";
  return value_->view();
}

RefStr::operator std::string_view() const { return view(); }

}  // namespace util