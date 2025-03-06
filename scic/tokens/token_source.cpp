#include "scic/tokens/token_source.hpp"

#include "scic/text/text_range.hpp"
#include "util/types/sequence.hpp"

namespace tokens {

TokenSource::TokenSource(text::TextRange source) { sources_.push_back(source); }
void TokenSource::AddSource(text::TextRange source) {
  sources_.push_back(source);
}
util::Seq<text::TextRange const&> TokenSource::sources() const {
  return sources_;
}

}  // namespace tokens