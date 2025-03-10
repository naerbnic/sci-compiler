#include "scic/diagnostics/diagnostics.hpp"

#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace diag {
namespace {

class DiagnosticRegistryImpl : public DiagnosticRegistry {
 public:
  void RegisterDiagnostic(DiagnosticId id,
                          std::string_view type_name) override {
    auto const& [it, inserted] = diagnostics_.emplace(id, type_name);
    if (!inserted) {
      throw std::logic_error("Diagnostic ID already registered");
    }
  }

  bool IsRegistered(DiagnosticId id) const {
    return diagnostics_.find(id) != diagnostics_.end();
  }

 private:
  std::unordered_map<DiagnosticId, std::string_view> diagnostics_;
};
}  // namespace

DiagnosticRegistry* DiagnosticRegistry::Get() {
  static DiagnosticRegistry* instance = new DiagnosticRegistryImpl();
  return instance;
}

}  // namespace diag