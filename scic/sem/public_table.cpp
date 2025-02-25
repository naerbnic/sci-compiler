#include "scic/sem/public_table.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/sem/object_table.hpp"
#include "scic/sem/proc_table.hpp"
#include "util/types/choice.hpp"
#include "util/types/sequence.hpp"

namespace sem {
namespace {

class PublicTableEntryImpl : public PublicTable::Entry {
 public:
  PublicTableEntryImpl(std::size_t index, Procedure const* proc)
      : index_(index), value_(proc) {}
  PublicTableEntryImpl(std::size_t index, Object const* object)
      : index_(index), value_(object) {}

  std::size_t index() const override { return index_; }
  util::Choice<Procedure const*, Object const*> value() const override {
    return value_;
  }

 private:
  std::size_t index_;
  util::Choice<Procedure const*, Object const*> value_;
};

class PublicTableImpl : public PublicTable {
 public:
  PublicTableImpl(std::vector<std::unique_ptr<PublicTableEntryImpl>> entries,
                  std::map<std::size_t, PublicTableEntryImpl const*> index_map)
      : entries_(std::move(entries)), index_map_(std::move(index_map)) {}

  util::Seq<Entry const&> entries() const override {
    return util::Seq<Entry const&>::Deref(entries_);
  }
  Entry const* LookupByIndex(std::size_t index) const override {
    auto it = index_map_.find(index);
    if (it == index_map_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::vector<std::unique_ptr<PublicTableEntryImpl>> entries_;
  std::map<std::size_t, PublicTableEntryImpl const*> index_map_;
};

class PublicTableBuilderImpl : public PublicTableBuilder {
 public:
  absl::Status AddProcedure(std::size_t index, Procedure const* proc) override {
    return AddEntry(std::make_unique<PublicTableEntryImpl>(index, proc));
  }

  absl::Status AddObject(std::size_t index, Object const* object) override {
    return AddEntry(std::make_unique<PublicTableEntryImpl>(index, object));
  }

  absl::StatusOr<std::unique_ptr<PublicTable>> Build() override {
    return std::make_unique<PublicTableImpl>(std::move(entries_),
                                             std::move(index_map_));
  }

 private:
  absl::Status AddEntry(std::unique_ptr<PublicTableEntryImpl> entry) {
    auto index = entry->index();
    if (index_map_.contains(index)) {
      return absl::InvalidArgumentError("Duplicate index");
    }
    index_map_.emplace(index, entry.get());
    entries_.emplace_back(std::move(entry));
    return absl::OkStatus();
  }
  std::vector<std::unique_ptr<PublicTableEntryImpl>> entries_;
  std::map<std::size_t, PublicTableEntryImpl const*> index_map_;
};
}  // namespace

std::unique_ptr<PublicTableBuilder> PublicTableBuilder::Create() {
  // return std::make_unique<PublicTableBuilderImpl>();
  return nullptr;
}

}  // namespace sem