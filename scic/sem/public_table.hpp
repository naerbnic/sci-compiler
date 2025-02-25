#ifndef SEM_PUBLIC_TABLE_HPP
#define SEM_PUBLIC_TABLE_HPP

#include <cstddef>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "scic/sem/object_table.hpp"
#include "scic/sem/proc_table.hpp"
#include "util/types/choice.hpp"
#include "util/types/sequence.hpp"

namespace sem {

class PublicTable {
 public:
  class Entry {
   public:
    virtual ~Entry() = default;

    virtual std::size_t index() const = 0;
    virtual util::Choice<Procedure const*, Object const*> value() const = 0;
  };
  virtual ~PublicTable() = default;

  virtual util::Seq<Entry const&> entries() const = 0;
  virtual Entry const* LookupByIndex(std::size_t index) const = 0;
};

class PublicTableBuilder {
 public:
  static std::unique_ptr<PublicTableBuilder> Create();

  virtual ~PublicTableBuilder() = default;

  virtual absl::Status AddProcedure(std::size_t index,
                                    Procedure const* proc) = 0;
  virtual absl::Status AddObject(std::size_t index, Object const* object) = 0;
  virtual absl::StatusOr<std::unique_ptr<PublicTable>> Build() = 0;
};

}  // namespace sem
#endif