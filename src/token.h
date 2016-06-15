#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "types.h"

enum {
    TOK_STR,
    TOK_CHAR,
    TOK_INT,
    TOK_FLOAT,
    TOK_BOOL,
    TOK_COLON,
    TOK_SEMI,
    TOK_REF,
    TOK_ID,
    TOK_POLYTYPE,
    TOK_FN,
    TOK_TYPE,
    TOK_OP,
    TOK_OPASSIGN,
    TOK_UOP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_STARTBIND,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LSQUARE,
    TOK_RSQUARE,
    TOK_IF,
    TOK_ELSE,
    TOK_RETURN,
    TOK_EXTERN,
    TOK_STRUCT,
    TOK_HOLD,
    TOK_RELEASE,
    TOK_WHILE,
    TOK_FOR,
    TOK_IN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_DIRECTIVE,
    TOK_ELLIPSIS,
    TOK_ENUM,
    TOK_USE
};

enum {
    OP_PLUS,
    OP_MINUS,
    OP_MUL,
    OP_DIV,
    OP_XOR,
    OP_BINAND,
    OP_BINOR,
    OP_ASSIGN,
    OP_AND,
    OP_OR,
    OP_EQUALS,
    OP_NEQUALS,
    OP_NOT,
    OP_DOT,
    OP_GT,
    OP_GTE,
    OP_LT,
    OP_LTE,
    OP_REF,
    OP_DEREF,
    OP_CAST
};

typedef struct Tok {
    int type;
    union {
        int ival;
        double fval;
        char *sval;
        int op;
    };
} Tok;

typedef struct TokList {
    Tok *item;
    struct TokList *next;
} TokList;

TokList *toklist_append(TokList *list, Tok *t);
TokList *reverse_toklist(TokList *list);

#define UNWIND_SET TokList *__consumed = NULL
#define NEXT_TOKEN_UNWINDABLE ((__consumed = toklist_append(__consumed, next_token()))->item)
#define UNWIND_TOKENS \
  for (; __consumed != NULL; __consumed = __consumed->next) unget_token(__consumed->item)

void skip_spaces();
int is_id_char(char c);
int type_id(char *buf);

Tok *make_token(int t);
Tok *next_token();
Tok *peek_token();

void unget_token(Tok *tok);

double read_decimal(char c);
Tok *read_number(char c);
char *read_string();
Tok *read_identifier(char c);
Tok *check_reserved(char *buf);

int priority_of(Tok *t);
const char *to_string(Tok *t);
const char *token_type(int type);
const char *op_to_str(int op);
int is_comparison(int op);
int valid_unary_op(int op);

void set_file_source(char *name, FILE *f);
void pop_file_source();

Tok *expect(int type);
int expect_line_break();
int expect_line_break_or_semicolon();

int lineno();
char *current_file_name();

#endif
