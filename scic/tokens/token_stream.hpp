#ifndef TOKENIZER_TOKEN_STREAM_HPP
#define TOKENIZER_TOKEN_STREAM_HPP

#include <deque>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"

namespace tokens {

class TokenStream {
 public:
  void PushToken(Token token) { curr_tokens_.push_front(std::move(token)); }

  template <class C>
  void PushTokens(C&& tokens,
                  std::optional<text::TextRange> destination = std::nullopt) {
    auto transformed = std::views::transform(
        std::ranges::subrange(
            std::make_move_iterator(std::begin(std::forward<C>(tokens))),
            std::make_move_iterator(std::end(std::forward<C>(tokens)))),
        [destination = std::move(destination)](Token token) {
          if (destination) {
            return token.AddSource(*destination);
          } else {
            return token;
          }
        });
    curr_tokens_.insert(curr_tokens_.begin(), transformed.begin(),
                        transformed.end());
  }

  bool HasNext() const { return !curr_tokens_.empty(); }

  std::optional<Token> NextToken() {
    if (curr_tokens_.empty()) {
      return std::nullopt;
    }
    Token token = std::move(curr_tokens_.front());
    curr_tokens_.pop_front();
    return token;
  }

 private:
  std::deque<Token> curr_tokens_;
};

}  // namespace tokens

#endif