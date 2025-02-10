#include "scic/parsers/list_tree/parser.hpp"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/tokenizer/token.hpp"
#include "scic/tokenizer/token_stream.hpp"
#include "util/status/status_macros.hpp"

namespace parsers::list_tree {
namespace {
using ::tokenizer::Token;
using ::tokenizer::TokenStream;

// Returns the name of the expression, if it is a token identifier with no
// trailer.
std::optional<std::string_view> GetExprPlainIdent(Expr const& expr) {
  auto* token_expr = expr.AsTokenExpr();
  if (!token_expr) {
    return std::nullopt;
  }
  auto* ident = token_expr->token().AsIdent();
  if (!ident || ident->trailer != Token::Ident::None) {
    return std::nullopt;
  }
  return ident->name;
}

class Parser {
 public:
  Parser(
      std::unique_ptr<TokenStream> token_stream,
      absl::btree_map<std::string, std::vector<Token>> const& initial_defines,
      IncludeContext const* include_context)
      : token_stream_(std::move(token_stream)),
        curr_defines_(std::move(initial_defines)),
        include_context_(include_context) {
    for (auto const& [key, value] : initial_defines) {
      curr_defines_.insert_or_assign(key, value);
    }
  }

  absl::StatusOr<std::optional<Token>> GetNextToken() {
    if (!pushed_tokens_.empty()) {
      auto token = std::move(pushed_tokens_.back());
      pushed_tokens_.pop_back();
      return token;
    }
    while (true) {
      auto token = token_stream_->NextToken();
      if (!token) {
        if (!preproc_stack_.empty()) {
          return absl::InvalidArgumentError(
              "Unexpected end of input when "
              "preprocessor directive still active.");
        }
        return std::nullopt;
      }

      auto* preproc = token->AsPreProcessor();
      if (preproc) {
        RETURN_IF_ERROR(HandlePreProcessorToken(*preproc));
        continue;
      }

      if (!preproc_stack_.empty() && !preproc_stack_.back().producing_tokens) {
        // Skip tokens until the next preprocessor directive.
        continue;
      }

      auto* ident = token->AsIdent();
      if (ident && ident->trailer == Token::Ident::None) {
        // It's possible this could be defined. If so, we need to replace it
        // in the stream with the defined tokens.
        auto define = curr_defines_.find(ident->name);
        if (define != curr_defines_.end()) {
          token_stream_->PushTokens(define->second);
          continue;
        }
      }

      return token;
    }
  }

  absl::Status HandlePreProcessorToken(Token::PreProcessor const& preproc) {
    enum class StackChange {
      // Push a new frame
      Push,

      // Go to next clause of current frame
      Next,

      // Pop the current frame
      Pop,
    };

    enum class Condition {
      // True if an identifier is defined.
      Def,

      // True if an identifier is not defined.
      NotDef,

      // True if a static expression is nonzero (after define resolution).
      Cond,

      // Always taken, if possible.
      Always,
    };

    StackChange stack_change;
    Condition condition;
    switch (preproc.type) {
      // Cases that start a new preprocessor frame.
      case tokenizer::Token::PPT_IFDEF:
        stack_change = StackChange::Push;
        condition = Condition::Def;
        break;
      case tokenizer::Token::PPT_IFNDEF:
        stack_change = StackChange::Push;
        condition = Condition::NotDef;
        break;
      case tokenizer::Token::PPT_IF:
        stack_change = StackChange::Push;
        condition = Condition::Cond;
        break;

        // Cases that keep a current preprocessor frame.
      case tokenizer::Token::PPT_ELIFDEF:
        stack_change = StackChange::Next;
        condition = Condition::Def;
        break;
      case tokenizer::Token::PPT_ELIFNDEF:
        stack_change = StackChange::Next;
        condition = Condition::NotDef;
        break;
      case tokenizer::Token::PPT_ELIF:
        stack_change = StackChange::Next;
        condition = Condition::Cond;
        break;
      case tokenizer::Token::PPT_ELSE:
        stack_change = StackChange::Next;
        condition = Condition::Always;
        break;

        // Cases that pop a preprocessing frame.
      case tokenizer::Token::PPT_ENDIF:
        stack_change = StackChange::Pop;
        condition = Condition::Always;
        break;
    }

    switch (stack_change) {
      case StackChange::Push:
        // If we're currently not generating tokens in the current frame, we
        // need to push, but disable the frame.
        if (!preproc_stack_.empty() &&
            !preproc_stack_.back().producing_tokens) {
          preproc_stack_.push_back({
              .producing_tokens = false,
              // Pretend a previous case was triggered, so no clauses are
              // ever triggered.
              .case_triggered = true,
          });
        } else {
          preproc_stack_.push_back({
              .producing_tokens = false,
              .case_triggered = false,
          });
        }
        break;
      case StackChange::Next:
        // If we were producing tokens in the previous clause, we don't want
        // to now.
        if (preproc_stack_.empty()) {
          return absl::InvalidArgumentError(
              "Unexpected #elif or #else without #if.");
        }
        preproc_stack_.back().producing_tokens = false;
        break;
      case StackChange::Pop:
        // If we popped, the previous frame should already have the state
        // needed. Just return to skip any conditionals.
        preproc_stack_.pop_back();
        return absl::OkStatus();
    }

    // If we're here, we should have at least one frame on the stack, with
    // its producing_tokens set to false.

    if (preproc_stack_.back().case_triggered) {
      // We've already triggered a case in this frame, so we should not
      // do any further checks.
      return absl::OkStatus();
    }

    // Check the conditional to see if we should produce tokens for the next
    // clause.

    bool condition_result;
    switch (condition) {
      case Condition::Def: {
        ASSIGN_OR_RETURN(condition_result, IsDef(preproc.lineTokens));
        break;
      }
      case Condition::NotDef: {
        ASSIGN_OR_RETURN(auto is_def, IsDef(preproc.lineTokens));
        condition_result = !is_def;
        break;
      }
      case Condition::Cond: {
        ASSIGN_OR_RETURN(condition_result, IsTrue(preproc.lineTokens));
        break;
      }
      case Condition::Always:
        condition_result = true;
        break;
    }

    if (condition_result) {
      preproc_stack_.back().producing_tokens = true;
      preproc_stack_.back().case_triggered = true;
    }

    return absl::OkStatus();
  }

  absl::StatusOr<bool> IsDef(absl::Span<Token const> tokens) {
    if (tokens.size() != 1) {
      // We need exactly one token.
      return absl::InvalidArgumentError(absl::StrFormat(
          "Expected a single identifier for preprocessor condition. Got %d",
          tokens.size()));
    }

    auto* ident = tokens[0].AsIdent();

    if (!ident) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Expected an identifier for preprocessor condition. Got %v",
          tokens[0]));
    }

    return curr_defines_.contains(ident->name);
  }

  absl::StatusOr<bool> IsTrue(absl::Span<Token const> tokens) {
    if (tokens.size() != 1) {
      // We need exactly one token.
      return absl::InvalidArgumentError(
          "Expected a single identifier for preprocessor condition.");
    }

    auto* token = &tokens[0];
    std::set<std::string_view> previous_defs;
    while (true) {
      auto* number = token->AsNumber();
      if (number) {
        return number->value != 0;
      }

      auto* ident = token->AsIdent();
      if (ident) {
        if (previous_defs.contains(ident->name)) {
          return absl::InvalidArgumentError(
              "Infinite loop in preprocessor condition.");
        }
        previous_defs.insert(ident->name);

        auto define = curr_defines_.find(ident->name);
        if (define == curr_defines_.end()) {
          return absl::InvalidArgumentError(
              "Undefined identifier in preprocessor condition.");
        }

        if (define->second.size() != 1) {
          return absl::InvalidArgumentError(
              "Expected a single token for preprocessor condition.");
        }

        token = &define->second[0];
        continue;
      }
    }
  }

  absl::StatusOr<std::optional<Expr>> ParseExpr() {
    ASSIGN_OR_RETURN(auto token, GetNextToken());
    if (!token) {
      return std::nullopt;
    }

    auto* punct = token->AsPunct();
    if (punct && punct->type == Token::PCT_LPAREN) {
      ASSIGN_OR_RETURN(auto list_expr, ParseList(std::move(token).value()));
      return Expr(std::move(list_expr));
    }

    return Expr(TokenExpr(std::move(token).value()));
  }

  absl::StatusOr<ListExpr> ParseList(Token start_paren) {
    // We've already read the opening parenthesis.
    std::vector<Expr> elements;

    while (true) {
      ASSIGN_OR_RETURN(auto next_token, GetNextToken());
      if (!next_token) {
        return absl::InvalidArgumentError("Unexpected end of list.");
      }

      auto* punct = next_token->AsPunct();
      if (punct && punct->type == Token::PCT_RPAREN) {
        return ListExpr(std::move(start_paren), std::move(next_token).value(),
                        std::move(elements));
      }

      pushed_tokens_.push_back(std::move(next_token).value());

      ASSIGN_OR_RETURN(auto next_expr, ParseExpr());
      if (!next_expr) {
        return absl::InvalidArgumentError("Unexpected end of list.");
      }
      elements.push_back(std::move(next_expr).value());
    }
  }

  absl::StatusOr<std::optional<Expr>> TopLevelExpr() {
    while (true) {
      ASSIGN_OR_RETURN(auto expr, ParseExpr());
      if (!expr) {
        return std::nullopt;
      }

      // We handle two kinds of top-level expressions: An include, or a define.
      auto* list_expr = expr->AsListExpr();
      if (!list_expr) {
        // This isn't a list.
        return expr;
      }

      auto const& elements = list_expr->elements();

      if (elements.empty()) {
        // This list is empty.
        return expr;
      }

      auto name = GetExprPlainIdent(elements[0]);
      if (!name) {
        return expr;
      }
      if (*name == "include") {
        RETURN_IF_ERROR(HandleInclude(elements.subspan(1)));
        continue;
      } else if (*name == "define") {
        RETURN_IF_ERROR(HandleDefine(elements.subspan(1)));
        continue;
      } else {
        return expr;
      }
    }
  }

  absl::Status HandleDefine(absl::Span<Expr const> rest_elements) {
    if (rest_elements.size() < 2) {
      return absl::InvalidArgumentError(
          "A define needs a symbol and a sequence of values.");
    }

    auto name = GetExprPlainIdent(rest_elements[0]);
    if (!name) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "First element of define must be an identifier. Got %v",
          rest_elements[0]));
    }

    auto values = rest_elements.subspan(1);

    std::vector<Token> value_tokens;

    for (auto const& value_expr : values) {
      value_expr.WriteTokens(&value_tokens);
    }

    curr_defines_.insert_or_assign(*name, std::move(value_tokens));
    return absl::OkStatus();
  }

  absl::Status HandleInclude(absl::Span<Expr const> rest_elements) {
    // This could cause preprocessor desyncs, if we include a file that
    // has misformatted preprocessor directives. If we want to handle this,
    // we will have to track the source of each define clause.
    if (rest_elements.size() != 1) {
      return absl::InvalidArgumentError("Include requires a single argument.");
    }

    // An include argument can be a string or an identifier.
    auto* token_expr = rest_elements[0].AsTokenExpr();
    if (!token_expr) {
      return absl::InvalidArgumentError(
          "Include argument must be either a string or symbol.");
    }

    std::string_view include_path;
    if (auto* string = token_expr->token().AsString()) {
      include_path = string->decodedString;
    } else if (auto* ident = token_expr->token().AsIdent()) {
      include_path = ident->name;
    } else {
      return absl::InvalidArgumentError(
          "Include argument must be either a string or symbol.");
    }
    ASSIGN_OR_RETURN(auto tokens,
                     include_context_->LoadTokensFromInclude(include_path));
    token_stream_->PushTokens(std::move(tokens));

    return absl::OkStatus();
  }

 private:
  // The state of the current preprocessor frame. There is one frame for
  // each layer of context active during the parse.
  struct PreProcessorFrame {
    // Iff true, tokens that we are seeing from the token stream are being
    // produced during parsing.
    bool producing_tokens;

    // If true, then a previous preprocessor directive at this frame has
    // been triggered, such as an #if clause. This allows us to track if
    // we should observe future #else or #elif clauses.
    bool case_triggered;
  };

  // The stack of preprocessor frames. The top of the stack is the back end
  // of the vector. If empty, we will always be producing tokens.
  std::vector<PreProcessorFrame> preproc_stack_;

  // Tokens that have been pushed to be returned again on the next call to
  // GetNextToken().
  std::vector<Token> pushed_tokens_;
  std::unique_ptr<TokenStream> token_stream_;
  absl::btree_map<std::string, std::vector<Token>> curr_defines_;
  IncludeContext const* include_context_;
};

}  // namespace

absl::StatusOr<std::vector<Expr>> ParseListTree(
    std::vector<tokenizer::Token> initial_tokens,
    IncludeContext const* include_context,
    absl::btree_map<std::string, std::vector<tokenizer::Token>> const&
        initial_defines) {
  auto token_stream = std::make_unique<TokenStream>();
  token_stream->PushTokens(std::move(initial_tokens));

  Parser parser(std::move(token_stream), initial_defines, include_context);

  std::vector<Expr> exprs;

  while (true) {
    ASSIGN_OR_RETURN(auto expr, parser.TopLevelExpr());
    if (!expr) {
      return exprs;
    }
    exprs.push_back(std::move(*expr));
  }
}
}  // namespace parser::list_tree