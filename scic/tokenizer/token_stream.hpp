#ifndef TOKENIZER_TOKEN_STREAM_HPP
#define TOKENIZER_TOKEN_STREAM_HPP

#include <deque>
#include <iterator>
#include <optional>
#include <utility>

#include "scic/tokenizer/token.hpp"

namespace tokenizer {

class TokenStream {
 public:
  void PushToken(Token token) { curr_tokens_.push_front(std::move(token)); }

  template <class C>
  void PushTokens(C&& tokens) {
    curr_tokens_.insert(
        curr_tokens_.begin(),
        std::make_move_iterator(std::begin(std::forward<C>(tokens))),
        std::make_move_iterator(std::end(std::forward<C>(tokens))));
  }

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

}  // namespace tokenizer

#endif