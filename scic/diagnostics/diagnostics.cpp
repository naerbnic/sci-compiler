#include "scic/diagnostics/diagnostics.hpp"

#include <stdexcept>
#include <unordered_set>

namespace diag {
namespace {

class DiagnosticRegistryImpl : public DiagnosticRegistry {
 public:
  void RegisterDiagnostic(DiagnosticId id) override {
    auto const& [it, inserted] = diagnostics_.insert(id);
    if (!inserted) {
      throw std::logic_error("Diagnostic ID already registered");
    }
  }

  bool IsRegistered(DiagnosticId id) const {
    return diagnostics_.find(id) != diagnostics_.end();
  }

 private:
  std::unordered_set<DiagnosticId> diagnostics_;
};
}  // namespace

DiagnosticRegistry* DiagnosticRegistry::Get() {
  static DiagnosticRegistry* instance = new DiagnosticRegistryImpl();
  return instance;
}

}  // namespace diag