#include "scic/parsers/list_tree/parser.hpp"

#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "scic/parsers/include_context.hpp"
#include "scic/parsers/list_tree/ast.hpp"
#include "scic/status/status.hpp"
#include "scic/text/text_range.hpp"
#include "scic/tokens/token.hpp"
#include "scic/tokens/token_readers.hpp"
#include "scic/tokens/token_stream.hpp"
#include "util/status/status_macros.hpp"

namespace parsers::list_tree {
namespace {

using ::tokens::Token;
using ::tokens::TokenStream;

// Returns the name of the expression, if it is a token identifier with no
// trailer.
std::optional<std::string_view> GetExprPlainIdent(Expr const& expr) {
  if (!expr.has<TokenExpr>()) {
    return std::nullopt;
  }
  auto* ident = expr.as<TokenExpr>().token().AsIdent();
  if (!ident || ident->trailer != Token::Ident::None) {
    return std::nullopt;
  }
  return ident->name;
}

class ProcessedTokenStream {
 public:
  explicit ProcessedTokenStream(
      std::unique_ptr<TokenStream> token_stream,
      absl::btree_map<std::string, std::vector<Token>>* defines)
      : token_stream_(std::move(token_stream)), defines_(std::move(defines)) {}

  status::StatusOr<std::optional<Token>> GetNextToken() {
    if (!pushed_tokens_.empty()) {
      auto token = std::move(pushed_tokens_.back());
      pushed_tokens_.pop_back();
      return token;
    }
    while (true) {
      auto token = token_stream_->NextToken();
      if (!token) {
        if (!preproc_stack_.empty()) {
          return status::InvalidArgumentError(
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
        auto define = defines_->find(ident->name);
        if (define != defines_->end()) {
          token_stream_->PushTokens(define->second, token->text_range());
          continue;
        }
      }

      return token;
    }
  }

  void PushToken(Token token) { pushed_tokens_.push_back(std::move(token)); }

  void PushRawTokens(std::vector<Token> tokens) {
    token_stream_->PushTokens(std::move(tokens));
  }

  void SetDefinition(std::string_view name, std::vector<Token> tokens) {
    (*defines_)[name] = std::move(tokens);
  }

 private:
  status::Status HandlePreProcessorToken(Token::PreProcessor const& preproc) {
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
      case tokens::Token::PPT_IFDEF:
        stack_change = StackChange::Push;
        condition = Condition::Def;
        break;
      case tokens::Token::PPT_IFNDEF:
        stack_change = StackChange::Push;
        condition = Condition::NotDef;
        break;
      case tokens::Token::PPT_IF:
        stack_change = StackChange::Push;
        condition = Condition::Cond;
        break;

        // Cases that keep a current preprocessor frame.
      case tokens::Token::PPT_ELIFDEF:
        stack_change = StackChange::Next;
        condition = Condition::Def;
        break;
      case tokens::Token::PPT_ELIFNDEF:
        stack_change = StackChange::Next;
        condition = Condition::NotDef;
        break;
      case tokens::Token::PPT_ELIF:
        stack_change = StackChange::Next;
        condition = Condition::Cond;
        break;
      case tokens::Token::PPT_ELSE:
        stack_change = StackChange::Next;
        condition = Condition::Always;
        break;
        // Cases that pop a preprocessing frame.
      case tokens::Token::PPT_ENDIF:
        stack_change = StackChange::Pop;
        condition = Condition::Always;
        break;
      default:
        throw std::runtime_error(absl::StrFormat(
            "Unexpected preprocessor directive: %d", preproc.type));
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
          return status::InvalidArgumentError(
              "Unexpected #elif or #else without #if.");
        }
        preproc_stack_.back().producing_tokens = false;
        break;
      case StackChange::Pop:
        // If we popped, the previous frame should already have the state
        // needed. Just return to skip any conditionals.
        preproc_stack_.pop_back();
        return status::OkStatus();
    }

    // If we're here, we should have at least one frame on the stack, with
    // its producing_tokens set to false.

    if (preproc_stack_.back().case_triggered) {
      // We've already triggered a case in this frame, so we should not
      // do any further checks.
      return status::OkStatus();
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
      default:
        throw std::runtime_error(
            absl::StrFormat("Unexpected condition type: %d", condition));
    }

    if (condition_result) {
      preproc_stack_.back().producing_tokens = true;
      preproc_stack_.back().case_triggered = true;
    }

    return status::OkStatus();
  }

  status::StatusOr<bool> IsDef(absl::Span<Token const> tokens) {
    if (tokens.size() != 1) {
      // We need exactly one token.
      return status::InvalidArgumentError(absl::StrFormat(
          "Expected a single identifier for preprocessor condition. Got %d",
          tokens.size()));
    }

    auto* ident = tokens[0].AsIdent();

    if (!ident) {
      return status::InvalidArgumentError(absl::StrFormat(
          "Expected an identifier for preprocessor condition. Got %v",
          tokens[0]));
    }

    return defines_->contains(ident->name);
  }

  status::StatusOr<bool> IsTrue(absl::Span<Token const> tokens) {
    if (tokens.size() != 1) {
      // We need exactly one token.
      return status::InvalidArgumentError(
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
          return status::InvalidArgumentError(
              "Infinite loop in preprocessor condition.");
        }
        previous_defs.insert(ident->name);

        auto define = defines_->find(ident->name);
        if (define == defines_->end()) {
          return status::InvalidArgumentError(
              "Undefined identifier in preprocessor condition.");
        }

        if (define->second.size() != 1) {
          return status::InvalidArgumentError(
              "Expected a single token for preprocessor condition.");
        }

        token = &define->second[0];
        continue;
      }
    }
  }

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
  absl::btree_map<std::string, std::vector<Token>>* defines_;
};

class ParserImpl {
 public:
  ParserImpl(ProcessedTokenStream token_stream,
             IncludeContext const* include_context)
      : token_stream_(std::move(token_stream)),
        include_context_(include_context) {}

  status::StatusOr<std::optional<Expr>> ParseExpr() {
    ASSIGN_OR_RETURN(auto token, token_stream_.GetNextToken());
    if (!token) {
      return std::nullopt;
    }

    auto* punct = token->AsPunct();
    if (!punct) {
      return Expr(TokenExpr(std::move(token).value()));
    }

    ListExpr::Kind kind;
    Token::PunctType close_punct;
    switch (punct->type) {
      case Token::PCT_LPAREN:
        kind = ListExpr::Kind::PARENS;
        close_punct = Token::PCT_RPAREN;
        break;

      case Token::PCT_LBRACKET:
        kind = ListExpr::Kind::BRACKETS;
        close_punct = Token::PCT_RBRACKET;
        break;

      default:
        return Expr(TokenExpr(std::move(token).value()));
    }

    ASSIGN_OR_RETURN(auto list_expr,
                     ParseList(kind, close_punct, std::move(token).value()));
    return Expr(std::move(list_expr));
  }

  status::StatusOr<ListExpr> ParseList(ListExpr::Kind kind,
                                       Token::PunctType close_punct,
                                       Token start_paren) {
    // We've already read the opening parenthesis.
    std::vector<Expr> elements;

    while (true) {
      ASSIGN_OR_RETURN(auto next_token, token_stream_.GetNextToken());
      if (!next_token) {
        return status::InvalidArgumentError("Unexpected end of list.");
      }

      auto* punct = next_token->AsPunct();
      if (punct && punct->type == close_punct) {
        return ListExpr(kind, std::move(start_paren),
                        std::move(next_token).value(), std::move(elements));
      }

      token_stream_.PushToken(std::move(next_token).value());

      ASSIGN_OR_RETURN(auto next_expr, ParseExpr());
      if (!next_expr) {
        return status::InvalidArgumentError("Unexpected end of list.");
      }
      elements.push_back(std::move(next_expr).value());
    }
  }

  status::StatusOr<std::optional<Expr>> TopLevelExpr() {
    while (true) {
      ASSIGN_OR_RETURN(auto expr, ParseExpr());
      if (!expr) {
        return std::nullopt;
      }

      // We handle two kinds of top-level expressions: An include, or a define.
      if (!expr->has<ListExpr>()) {
        // This isn't a list.
        return expr;
      }

      auto const& elements = expr->as<ListExpr>().elements();

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

  status::Status HandleDefine(absl::Span<Expr const> rest_elements) {
    if (rest_elements.size() < 2) {
      return status::InvalidArgumentError(
          "A define needs a symbol and a sequence of values.");
    }

    auto name = GetExprPlainIdent(rest_elements[0]);
    if (!name) {
      return status::InvalidArgumentError(absl::StrFormat(
          "First element of define must be an identifier. Got %v",
          rest_elements[0]));
    }

    auto values = rest_elements.subspan(1);

    std::vector<Token> value_tokens;

    for (auto const& value_expr : values) {
      value_expr.WriteTokens(&value_tokens);
    }

    token_stream_.SetDefinition(*name, std::move(value_tokens));
    return status::OkStatus();
  }

  status::Status HandleInclude(absl::Span<Expr const> rest_elements) {
    // This could cause preprocessor desyncs, if we include a file that
    // has misformatted preprocessor directives. If we want to handle this,
    // we will have to track the source of each define clause.
    if (rest_elements.size() != 1) {
      return status::InvalidArgumentError(
          "Include requires a single argument.");
    }

    // An include argument can be a string or an identifier.
    auto const& incl_value = rest_elements[0];
    if (!incl_value.has<TokenExpr>()) {
      return status::InvalidArgumentError(
          "Include argument must be either a string or symbol.");
    }

    auto const& token_expr = incl_value.as<TokenExpr>();

    std::string_view include_path;
    if (auto* string = token_expr.token().AsString()) {
      include_path = string->decodedString;
    } else if (auto* ident = token_expr.token().AsIdent()) {
      include_path = ident->name;
    } else {
      return status::InvalidArgumentError(
          "Include argument must be either a string or symbol.");
    }
    ASSIGN_OR_RETURN(auto include_text,
                     include_context_->LoadTextFromIncludePath(include_path));
    ASSIGN_OR_RETURN(auto tokens,
                     tokens::TokenizeText(std::move(include_text)));

    token_stream_.PushRawTokens(std::move(tokens));

    return status::OkStatus();
  }

 private:
  ProcessedTokenStream token_stream_;
  IncludeContext const* include_context_;
};

}  // namespace

void Parser::AddDefine(std::string_view name, std::vector<Token> tokens) {
  defines_[name] = std::move(tokens);
}

status::StatusOr<std::vector<Expr>> Parser::ParseTree(
    std::vector<Token> tokens) {
  auto token_stream = std::make_unique<TokenStream>();
  token_stream->PushTokens(std::move(tokens));

  ParserImpl parser(ProcessedTokenStream(std::move(token_stream), &defines_),
                    include_context_);

  std::vector<Expr> exprs;

  while (true) {
    ASSIGN_OR_RETURN(auto expr, parser.TopLevelExpr());
    if (!expr) {
      return exprs;
    }
    exprs.push_back(std::move(*expr));
  }
}

}  // namespace parsers::list_tree