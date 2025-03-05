#ifndef SEM_EXTERN_TABLE_HPP
#define SEM_EXTERN_TABLE_HPP

#include <memory>
#include <optional>
#include <string_view>

#include "scic/sem/common.hpp"
#include "scic/status/status.hpp"
#include "util/strings/ref_str.hpp"
#include "util/types/sequence.hpp"

namespace sem {

class ExternTable {
 public:
  class Extern {
   public:
    virtual ~Extern() = default;

    virtual NameToken const& token_name() const = 0;
    virtual util::RefStr const& name() const = 0;
    virtual std::optional<ScriptNum> script_num() const = 0;
    virtual PublicIndex index() const = 0;
  };

  virtual ~ExternTable() = default;

  virtual util::Seq<Extern const&> externs() const = 0;
  virtual Extern const* LookupByName(std::string_view name) const = 0;
};

class ExternTableBuilder {
 public:
  static std::unique_ptr<ExternTableBuilder> Create();

  virtual ~ExternTableBuilder() = default;

  virtual status::Status AddExtern(NameToken name,
                                   std::optional<ScriptNum> script_num,
                                   PublicIndex index) = 0;
  virtual status::StatusOr<std::unique_ptr<ExternTable>> Build() = 0;
};

}  // namespace sem

#endif