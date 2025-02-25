#include "scic/sem/extern_table.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/sem/common.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {
namespace {

class ExternImpl : public ExternTable::Extern {
 public:
  ExternImpl(NameToken name, std::optional<ScriptNum> script_num,
             PublicIndex index)
      : name_(std::move(name)), script_num_(script_num), index_(index) {}

  NameToken const& token_name() const override { return name_; }

  util::RefStr const& name() const override { return name_.value(); }

  std::optional<ScriptNum> script_num() const override { return script_num_; }

  PublicIndex index() const override { return index_; }

 private:
  NameToken name_;
  std::optional<ScriptNum> script_num_;
  PublicIndex index_;
};
;

class ExternTableImpl : public ExternTable {
 public:
  ExternTableImpl(
      std::vector<std::unique_ptr<ExternImpl>> externs,
      std::map<std::string_view, ExternImpl const*, std::less<>> name_map)
      : externs_(std::move(externs)), name_map_(std::move(name_map)) {}

  util::Seq<Extern const&> externs() const override {
    return util::Seq<Extern const&>::Deref(externs_);
  }

  Extern const* LookupByName(std::string_view name) const override {
    auto it = name_map_.find(name);
    if (it == name_map_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::vector<std::unique_ptr<ExternImpl>> externs_;
  std::map<std::string_view, ExternImpl const*, std::less<>> name_map_;
};

class ExternTableBuilderImpl : public ExternTableBuilder {
 public:
  absl::Status AddExtern(NameToken name, std::optional<ScriptNum> script_num,
                         PublicIndex index) override {
    if (name_map_.contains(name.value())) {
      return absl::InvalidArgumentError("Duplicate extern name");
    }

    auto extern_item =
        std::make_unique<ExternImpl>(std::move(name), script_num, index);

    name_map_.emplace(name.value(), extern_item.get());
    externs_.push_back(std::move(extern_item));

    return absl::OkStatus();
  }
  absl::StatusOr<std::unique_ptr<ExternTable>> Build() override {
    return std::make_unique<ExternTableImpl>(std::move(externs_),
                                             std::move(name_map_));
  }

 private:
  std::vector<std::unique_ptr<ExternImpl>> externs_;
  std::map<std::string_view, ExternImpl const*, std::less<>> name_map_;
};

}  // namespace

std::unique_ptr<ExternTableBuilder> ExternTableBuilder::Create() {
  return std::make_unique<ExternTableBuilderImpl>();
}
}  // namespace sem