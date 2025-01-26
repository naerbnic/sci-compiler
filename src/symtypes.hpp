
#ifndef SYMTYPES_H
#define SYMTYPES_H

// Symbol types
enum sym_t {
  S_END = 128,           // end of input
  S_KEYWORD,             // keyword
  S_DEFINE,              // definition
  S_IDENT,               // unknown identifier
  S_LABEL,               // label
  S_GLOBAL,              // global variable
  S_LOCAL,               // local variable
  S_TMP,                 // temporary variable
  S_PARM,                // parameter
  S_PROC,                // procedure
  S_EXTERN,              // external procedure/object
  S_ASSIGN,              // assignment operator
  S_NARY,                // nary arithmetic operator
  S_BINARY,              // binary operator
  S_UNARY,               // unary arithmetic operator
  S_COMP,                // comparison operator
  S_NUM,                 // number
  S_STRING,              // string
  S_CLASS,               // class
  S_OBJ,                 // object
  S_SELECT,              // selector
  S_SELECT_LIT,          // selector literal (e.g. "foo:")
  S_LPROP,               // property referenced as local var
  S_REST,                // &rest keyword
  S_PROP,                // property
  S_METH,                // method
  S_OPEN_P = '(',        // open parenthesis
  S_OPEN_BRACKET = '[',  // open bracket
};

#define S_MSGEND ((sym_t)',')

// Keywords
enum keyword_t {
  K_UNDEFINED,   // not a keyword
  K_SCRIPT,      // 'script'
  K_INCLUDE,     // 'include'
  K_PUBLIC,      // 'public'
  K_EXTERN,      // 'external'
  K_GLOBALDECL,  // 'globaldecl'
  K_GLOBAL,      // 'global'
  K_LOCAL,       // 'local'
  K_TMP,         // '&tmp'
  K_DEFINE,      // 'define'
  K_CLASSDEF,    // 'classdef'
  K_SCRIPTNUM,   // 'script#'
  K_CLASSNUM,    // 'class#'
  K_CLASS,       // 'class'
  K_PROPLIST,    // 'properties'
  K_METHODLIST,  // 'methods'
  K_METHOD,      // 'method'
  K_INSTANCE,    // 'instance'
  K_OF,          // 'of'
  K_ENUM,        // 'enum'
  K_PROC,        // 'procedure'
  K_BREAK,       // 'break'
  K_CONT,        // 'continue'
  K_WHILE,       // 'while'
  K_REPEAT,      // 'repeat'
  K_IF,          // 'if'
  K_ELSE,        // 'else'
  K_COND,        // 'cond'
  K_SWITCH,      // 'switch'
  K_ASSIGN,      // '='
  K_RETURN,      // 'return'
  K_INC,         // '++'
  K_DEC,         // '--'
  K_FOR,         // 'for'
  K_BREAKIF,     // 'breakif'
  K_CONTIF,      // 'contif'
  K_SELECT,      // 'selectors'
  K_SUPER,       // 'super#'
  K_REST,        // '&rest'
  K_PROP,        // 'property'
  K_FILE,        // 'file#'
  K_SWITCHTO     // 'switchto'
};

// Operators
enum op_t {
  N_PLUS,    // +
  N_MUL,     // *
  B_MINUS,   // -
  B_DIV,     // /
  B_SLEFT,   // <<
  B_SRIGHT,  // >>
  N_BITXOR,  // ^
  N_BITAND,  // &
  N_BITOR,   // |
  U_NOT,     // !
  U_NEG,     // -
  C_GT,      // >
  C_GE,      // >=
  C_LT,      // <
  C_LE,      // <=
  C_EQ,      // ==
  C_NE,      // !=
  N_AND,     // and
  N_OR,      // or
  A_EQ,      // =
  A_PLUS,    // +=
  A_MUL,     // *=
  A_MINUS,   // -=
  A_DIV,     // /=
  A_SLEFT,   // <<=
  A_SRIGHT,  // >>=
  A_XOR,     // ^=
  A_AND,     // &=
  A_OR,      // |=
  U_BNOT,    // ~
  B_MOD,     // %
  C_UGT,     // u>
  C_UGE,     // u>=
  C_ULT,     // u<
  C_ULE      // u<=
};

#endif  // SYMTYPES_H
