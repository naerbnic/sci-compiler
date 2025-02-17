#ifndef PNODE_HPP
#define PNODE_HPP

#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "scic/anode.hpp"
#include "scic/symbol.hpp"

// Parse node types.  The same as symbol types, but with some additions.
enum pn_t {
  PN_END = 128,  // end of input
  PN_KEYWORD,    // keyword
  PN_DEFINE,     // definition
  PN_IDENT,      // unknown identifier
  PN_LABEL,      // label
  PN_GLOBAL,     // global variable
  PN_LOCAL,      // local variable
  PN_TMP,        // temporary variable
  PN_PARM,       // parameter
  PN_PROC,       // procedure
  PN_EXTERN,     // external procedure/object
  PN_ASSIGN,     // assignment
  PN_NARY,       // nary arithmetic operator
  PN_BINARY,     // binary operator
  PN_UNARY,      // unary arithmetic operator
  PN_COMP,       // comparison operator
  PN_NUM,        // number
  PN_STRING,     // string
  PN_CLASS,      // class
  PN_OBJ,        // object
  PN_SELECT,     // object selector
  PN_WORD,       // word

  PN_METHOD,   // method
  PN_KMETHOD,  // kernal method
  PN_LPROP,    // property
  PN_ELIST,    // expression list
  PN_EXPR,     // expression
  PN_INDEX,    // indexed variable
  PN_SEND,     // send to object
  PN_CALL,
  PN_LINK,
  PN_FOR,
  PN_WHILE,
  PN_REPEAT,
  PN_BREAK,
  PN_BREAKIF,
  PN_CONT,
  PN_CONTIF,
  PN_IF,
  PN_COND,
  PN_SWITCH,
  PN_ELSE,
  PN_INCDEC,
  PN_RETURN,
  PN_SUPER,
  PN_REST,
  PN_PROP,
  PN_METH,
  PN_ADDROF,  // address of operator (@)
  PN_MSG,
  PN_SWITCHTO
};

struct PNode {
  // Node for building a parse tree.

  using ChildVector = std::vector<std::unique_ptr<PNode>>;
  using ChildSpan = std::span<std::unique_ptr<PNode> const>;

  PNode(pn_t t);

  PNode* addChild(std::unique_ptr<PNode> node);
  PNode* first_child() const {
    return children.empty() ? nullptr : children[0].get();
  }

  PNode* child_at(int i) const {
    return ((std::size_t)i) < children.size() ? children[i].get() : nullptr;
  }

  ChildSpan rest() const { return rest_at(1); }
  ChildSpan rest_at(std::size_t i) const {
    return ChildSpan(children).subspan(i);
  }

  PNode* newChild(pn_t t) {
    auto node = std::make_unique<PNode>(t);
    auto ptr = node.get();
    children.push_back(std::move(node));
    return ptr;
  }

  // Children
  ChildVector children;
  Symbol* sym;  // symbol associated with node
  ANText* str;  // string associated with node; Only if type is PN_STRING.
  // FIXME: This is sometimes redudnant with the sym ptr. Check to see what is
  // needed
  int val;      // node value
  pn_t type;    // type of node
  int lineNum;  //	line number in current source file
};

#endif