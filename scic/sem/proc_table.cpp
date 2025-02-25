#include "scic/sem/proc_table.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/codegen/code_generator.hpp"
#include "scic/sem/common.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {
namespace {

class ProcedureImpl : public Procedure {
 public:
  ProcedureImpl(NameToken name, codegen::PtrRef ptr_ref)
      : name_(std::move(name)), ptr_ref_(std::move(ptr_ref)) {}

  NameToken const& token_name() const override { return name_; }
  util::RefStr const& name() const override { return name_.value(); }
  codegen::PtrRef* ptr_ref() const override { return &ptr_ref_; }

 private:
  NameToken name_;
  mutable codegen::PtrRef ptr_ref_;
};

class ProcTableImpl : public ProcTable {
 public:
  explicit ProcTableImpl(
      std::vector<std::unique_ptr<ProcedureImpl>> procedures,
      std::map<std::string_view, ProcedureImpl*, std::less<>> name_table)
      : procedures_(std::move(procedures)),
        name_table_(std::move(name_table)) {}

  util::Seq<Procedure const&> procedures() const override {
    return util::Seq<Procedure const&>::Deref(procedures_);
  }

  Procedure const* LookupByName(std::string_view name) const override {
    auto it = name_table_.find(name);
    if (it == name_table_.end()) {
      return nullptr;
    }
    return it->second;
  }

 private:
  std::vector<std::unique_ptr<ProcedureImpl>> procedures_;
  std::map<std::string_view, ProcedureImpl*, std::less<>> name_table_;
};

class ProcTableBuilderImpl : public ProcTableBuilder {
 public:
  explicit ProcTableBuilderImpl(codegen::CodeGenerator* codegen)
      : codegen_(codegen) {}

  absl::Status AddProcedure(NameToken name) override {
    auto ptr_ref = codegen_->CreatePtrRef();
    auto new_proc = std::make_unique<ProcedureImpl>(name, std::move(ptr_ref));
    name_table_.emplace(new_proc->name(), new_proc.get());
    procedures_.push_back(std::move(new_proc));
    return absl::OkStatus();
  }

  absl::StatusOr<std::unique_ptr<ProcTable>> Build() override {
    return std::make_unique<ProcTableImpl>(std::move(procedures_),
                                           std::move(name_table_));
  }

 private:
  absl::Nonnull<codegen::CodeGenerator*> codegen_;
  std::vector<std::unique_ptr<ProcedureImpl>> procedures_;
  std::map<std::string_view, ProcedureImpl*, std::less<>> name_table_;
};
}  // namespace

std::unique_ptr<ProcTableBuilder> ProcTableBuilder::Create(
    codegen::CodeGenerator* codegen) {
  return std::make_unique<ProcTableBuilderImpl>(codegen);
}

}  // namespace sem