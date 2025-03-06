#ifndef PARSERS_TOKEN_SOURCE_HPP
#define PARSERS_TOKEN_SOURCE_HPP

#include "absl/container/inlined_vector.h"
#include "scic/text/text_range.hpp"
#include "util/types/sequence.hpp"

namespace tokens {
// The source of a token.
//
// Because of how Defines work, a token can come from a sequence of sources.
// There's the location of the original token in the file, and the location of
// the token or tokens that replaced it using defines in the preprocessor.
// This class tracks that sequence of sources.
class TokenSource {
 public:
  TokenSource() = default;
  TokenSource(text::TextRange source);

  TokenSource(TokenSource const&) = default;
  TokenSource(TokenSource&&) = default;

  TokenSource& operator=(TokenSource const&) = default;
  TokenSource& operator=(TokenSource&&) = default;

  // Adds a new source to the end of the list.
  //
  // This indicates that the existing token was substituted into the given
  // context.
  void AddSource(text::TextRange source);

  // Returns the sources of this token. The first element is the source of the
  // actual token contents. The last is the final substituted location.
  //
  // The source borrows from this object, and should not be used after this
  // object is destroyed or moved.
  util::Seq<text::TextRange const&> sources() const;

 private:
  // The sources of this token, in the order they were applied. The first
  // element is the source of the actual token contents. The last is the final
  // substituted location.
  absl::InlinedVector<text::TextRange, 4> sources_;
};
}  // namespace tokens

#endif