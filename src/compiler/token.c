#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "token.h"
#include "types.h"
#include "util.h"

typedef struct FileStack {
    char *name;
    int line;
    FILE *handle;
    struct FileStack *next;
} FileStack;

static FileStack *source_stack = NULL;
static Tok *_last_token = NULL;

void push_file_source(char *name, FILE *f) {
    FileStack *stack = malloc(sizeof(struct FileStack));
    stack->name = name;
    stack->line = 1;
    stack->handle = f;
    stack->next = source_stack;
    source_stack = stack;
}

void pop_file_source() {
    if (source_stack->handle != stdin) {
        fclose(source_stack->handle);
    }
    source_stack = source_stack->next;
}

char get_char() {
    return fgetc(source_stack->handle);
}

void unget_char(char c) {
    ungetc(c, source_stack->handle);
}

char *read_block_comment(int store) {
    int alloc = 8;
    int len = 0;
    int count = 1;
    char c;
    int start = source_stack->line;
    char *buf = NULL;
    if (store) {
        buf = malloc(alloc);
    }
    while ((c = get_char()) != EOF) {
        if (c == '/') {
            if (store) {
                buf[len++] = c;
                if (len == alloc - 1) {
                    alloc *= 2;
                    buf = realloc(buf, alloc);
                }
            }
            c = get_char();
            if (c == '*') {
                count++;
            }
        } else if (c == '*') {
            c = get_char();
            if (c == '/') {
                count--;
                if (count == 0) {
                    if (store) {
                        buf[len] = '\0';
                    }
                    return buf;
                }
            }
        }
        if (c == '\n') {
            source_stack->line++;
        }
        if (store) {
            buf[len++] = c;
            if (len == alloc - 1) {
                alloc *= 2;
                buf = realloc(buf, alloc);
            }
        }
    }
    error(start, current_file_name(), "Incompleted block comment");
    return NULL;
}

char *read_line_comment(int store) {
    int alloc = 8;
    int len = 0;
    char c;
    char *buf = NULL;
    if (store) {
        buf = malloc(alloc);
    }
    while ((c = get_char()) != EOF) {
        if (c == '\n') {
            source_stack->line++;
            break;
        }
        if (store) {
            buf[len++] = c;
            if (len == alloc - 1) {
                alloc *= 2;
                buf = realloc(buf, alloc);
            }
        }
    }
    if (store) {
        buf[len] = '\0';
    }
    return buf;
}

char read_non_space() {
    char c;
    while ((c = get_char()) != EOF) {
        if (c == '\n') {
            source_stack->line++;
        }
        if (!(c == ' ' || c == '\t' || c == '\r')) {
            break;
        }
    }
    return c;
}

int is_id_char(char c) {
    return isalpha(c) || isdigit(c) || c == '_';
}

Tok *make_token(int t) {
    Tok *tok = malloc(sizeof(Tok));
    tok->type = t;
    tok->line = lineno();
    if (t == TOK_NL) {
        // put this on the previous line
        tok->line--;
    }
    tok->file = current_file_name();
    return tok;
}

int expect_line_break() {
    char c = get_char();
    while (c == ' ') {
        c = get_char();
    }
    if (c == '\n') {
        source_stack->line++;
        return 1;
    }
    unget_char(c);
    return 0;
}
int expect_line_break_or(char expected) {
    if (expect_line_break()) {
        return 1;
    }
    char c = get_char();
    if (c == expected) {
        return 1;
    }
    unget_char(c);
    return 0;
}
int expect_line_break_or_semicolon() {
    return expect_line_break_or(';');
}

Tok *_next_token(int comment_ok) {
    if (_last_token != NULL) {
        Tok *t = _last_token;
        _last_token = NULL;
        return t;
    }
    char c = read_non_space();
    int start_line = lineno();
    if (c == '/') {
        char d = get_char();
        if (d == '/') {
            char *comm = read_line_comment(comment_ok);
            if (comment_ok) {
                Tok *t = make_token(TOK_COMMENT);
                t->sval = comm;
                t->line = start_line;
                return t;
            } else {
                return _next_token(0);
            }
        } else if (d == '*') {
            char *comm = read_block_comment(comment_ok);
            if (comment_ok) {
                /*errlog("block comment: %s", comm);*/
                Tok *t = make_token(TOK_COMMENT);
                t->sval = comm;
                t->line = start_line;
                return t;
            } else {
                return _next_token(0);
            }
        } else {
            unget_char(d);
        }
    }
    Tok *t = NULL;

    while (c == '\n') {
        if (t == NULL) {
            t = make_token(TOK_NL);
        }
        c = read_non_space();
    }
    if (c == EOF) {
        return t;
    }
    if (t) {
        unget_char(c);
        return t;
    }

    if (isdigit(c)) {
        t = read_number(c);
    } else if (c == '$') {
        c = get_char();
        if (!(isalpha(c) || c == '_')) {
            error(lineno(), current_file_name(), "Unexpected character '%c' following '$'.", c);
        }
        t = read_identifier(c);
        if (t->type != TOK_ID) {
            error(lineno(), current_file_name(), "Bad name for polymorphic type: '%s' is reserved.", t->sval);
        }
        t->type = TOK_POLY;
    } else if (isalpha(c) || c == '_') {
        t = read_identifier(c);
    /*} else if (c == '\'') {*/
        /*t = make_token(TOK_CHAR);*/
        /*c = get_char();*/
        /*if (c == '\\') {*/
            /*c = get_char();*/
        /*}*/
        /*t->ival = c;*/
        /*c = get_char();*/
        /*if (c != '\'') {*/
            /*error(lineno(), current_file_name(), "Invalid character literal sequence.");*/
        /*}*/
    } else if (c == '\"') {
        t = make_token(TOK_STR);
        t->sval = read_string();
    } else if (c == '(') {
        t = make_token(TOK_LPAREN);
    } else if (c == ')') {
        t = make_token(TOK_RPAREN);
    } else if (c == '[') {
        t = make_token(TOK_LSQUARE);
    } else if (c == ']') {
        t = make_token(TOK_RSQUARE);
    } else if (c == '\'') {
        t = make_token(TOK_SQUOTE);
    } else if (c == '#') {
        c = get_char();
        if (c == '{') {
            t = make_token(TOK_STARTBIND);
        } else if (isalpha(c) || c == '_') {
            // TODO refactor this
            t = make_token(TOK_DIRECTIVE);
            int alloc = 8;
            char *buf = malloc(alloc);
            buf[0] = c;
            int len = 1;
            for (;;) {
                c = get_char();
                if (!is_id_char(c)) {
                    unget_char(c);
                    break;
                }
                buf[len++] = c;
                if (len == alloc - 1) {
                    alloc *= 2;
                    buf = realloc(buf, alloc);
                }
            }
            buf[len] = 0;
            t->sval = buf;
        } else {
            error(lineno(), current_file_name(), "Unexpected character sequence '#%c'", c);
        }
    } else if (c == '{') {
        t = make_token(TOK_LBRACE);
    } else if (c == '}') {
        t = make_token(TOK_RBRACE);
    } else if (c == ',') {
        t = make_token(TOK_COMMA);
    } else if (c == '.') {
        if ((c = get_char()) == '.') {
            if ((c = get_char()) == '.') {
                t = make_token(TOK_ELLIPSIS); 
            } else {
                error(lineno(), current_file_name(), "Unexected character '%c' following '..'", c);
            }
        } else {
            unget_char(c);
            t = make_token(TOK_OP);
            t->op = OP_DOT;
        }
    } else if (c == ';') {
        t = make_token(TOK_SEMI);
    } else if (c == ':') {
        if ((c = get_char()) == ':') {
            t = make_token(TOK_DCOLON);
        } else {
            unget_char(c);
            t = make_token(TOK_COLON);
        }
    } else if (c == '+') {
        char n = get_char();
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_PLUS;
    } else if (c == '-') {
        char n = get_char();
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else if (n == '>') {
            t = make_token(TOK_ARROW);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_MINUS;
    } else if (c == '*') {
        char n = get_char();
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_MUL;
    } else if (c == '/') {
        char n = get_char();
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_DIV;
    } else if (c == '%') {
        char n = get_char();
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_MOD;
    } else if (c == '^') {
        char n = get_char();
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = OP_XOR;
    } else if (c == '|') {
        char n = get_char();
        int op = OP_BINOR;
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else if (n == '|') {
            t = make_token(TOK_OP);
            op = OP_OR;
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = op;
    } else if (c == '&') {
        char n = get_char();
        int op = OP_BINAND;
        if (n == '=') {
            t = make_token(TOK_OPASSIGN);
        } else if (n == '&') {
            t = make_token(TOK_OP);
            op = OP_AND;
        } else {
            t = make_token(TOK_OP);
            unget_char(n);
        }
        t->op = op;
    } else if (c == '>') {
        t = make_token(TOK_OP);
        int d = get_char();
        if (d == '=') {
            t->op = OP_GTE;
        } else if (d == '>') {
            t->op = OP_RSHIFT;
        } else {
            unget_char(d);
            t->op = OP_GT;
        }
    } else if (c == '<') {
        t = make_token(TOK_OP);
        int d = get_char();
        if (d == '=') {
            t->op = OP_LTE;
        } else if (d == '<') {
            t->op = OP_LSHIFT;
        } else {
            unget_char(d);
            t->op = OP_LT;
        }
    } else if (c == '!') {
        int d = get_char();
        if (d == '=') {
            t = make_token(TOK_OP);
            t->op = OP_NEQUALS;
        } else {
            unget_char(d);
            t = make_token(TOK_UOP);
            t->op = OP_NOT;
        }
    } else if (c == '=') {
        t = make_token(TOK_OP);
        t->op = OP_ASSIGN;
        int d = get_char();
        if (d == '=') {
            t->op = OP_EQUALS;
        } else {
            unget_char(d);
        }
    }
    if (t == NULL) {
        error(lineno(), current_file_name(), "Unexpected character '%c'.", c);
    }
    return t;
}
Tok *next_token() {
    Tok *t = _next_token(0);
    while (t && t->type == TOK_NL) {
        t = _next_token(0);
    }
    return t;
}
Tok *next_token_or_newline() {
    return _next_token(0);
}

Tok *next_token_or_comment() {
    return _next_token(1);
}

Tok *peek_token() {
    Tok *t = next_token();
    unget_token(t);
    return t;
}

Tok *try_eat_token(TokType t) {
    Tok *tok = next_token();
    if (tok == NULL) {
        error(lineno(), current_file_name(), "Expected token '%s', but got EOF", token_type(t));
    }
    if (tok->type == t) {
        return tok;
    }
    unget_token(tok);
    return NULL;
}

void unget_token(Tok *tok) {
    if (_last_token != NULL) {
        error(-1, "<internal>", "Cannot unget_token() twice in a row.");
    }
    _last_token = tok;
}

double read_decimal(char c) {
    double d = 0;
    double n = 10;

    for (;;) {
        if (!isdigit(c)) {
            unget_char(c);
            break;
        }
        d += (c - '0') / n;
        n *= 10;
        c = get_char();
    }
    return d;
}

int is_hex_digit(char c) {
    return isdigit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

unsigned long long hex_val(char c) {
    if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    }
    return c - '0';
}

Tok *read_hex_number(char c) {
    unsigned long long n = hex_val(c);
    for (;;) {
        char c = get_char();
        if (!is_hex_digit(c)) {
            unget_char(c);
            break;
        }
        n = n * 16 + hex_val(c);
    }
    Tok *t = make_token(TOK_INT);
    t->ival = n;
    return t;
}

Tok *read_octal_number(char c) {
    long n = c - '0';
    for (;;) {
        char c = get_char();
        if (c < '0' || c > '7') {
            unget_char(c);
            break;
        }
        n = n * 8 + (c - '0');
    }
    Tok *t = make_token(TOK_INT);
    t->ival = n;
    return t;
}

Tok *read_number(char c) {
    if (c == '0') {
        c = get_char();
        if (c == 'x') {
            c = get_char();
            if (!is_hex_digit(c)) {
                error(lineno(), current_file_name(), "Invalid hexadecimal literal '0x%c'", c);
            }
            return read_hex_number(c);
        } else if (c == 'o') {
            c = get_char();
            if (c < '0' || c > '7') {
                error(lineno(), current_file_name(), "Invalid octal literal '0o%c'", c);
            }
            return read_octal_number(c);
        }
        unget_char(c);
        c = '0';
    }
    long long n = c - '0';
    for (;;) {
        char c = get_char();
        if (!isdigit(c)) {
            if (c == '.') {
                Tok *t = make_token(TOK_FLOAT);
                c = get_char();
                if (!isdigit(c)) {
                    if (c == '.') { // reading a possible ellipsis?
                        unget_char(c);
                        unget_char('.');
                        break;
                    }
                    error(lineno(), current_file_name(), "Unexpected non-numeric character '%c' while reading float.", c);
                }
                t->fval = n + read_decimal(c);
                return t;
            }
            unget_char(c);
            break;
        }
        n = n * 10 + (c - '0');
    }
    Tok *t = make_token(TOK_INT);
    t->ival = n;
    return t;
}

char *read_string() {
    int alloc = 8;
    char *buf = malloc(alloc);
    int len = 0;
    char c;
    int start = source_stack->line;
    int escape = 0;
    while ((c = get_char()) != EOF) {
        if (c == '\"' && !escape) {
            buf[len] = 0;
            return buf;
        }
        if (escape) {
            // are these right?
            switch (c) {
            case '\'':
                c = 0x27;
                break;
            case '"':
                c = 0x22;
                break;
            case '?':
                c = 0x3f;
                break;
            case '\\':
                c = 0x5c;
                break;
            case 'a':
                c = 0x07;
                break;
            case 'b':
                c = 0x08;
                break;
            case 'f':
                c = 0x0c;
                break;
            case 'n':
                c = 0x0a;
                break;
            case 'r':
                c = 0x0d;
                break;
            case 't':
                c = 0x09;
                break;
            case 'v':
                c = 0x0b;
                break;
            case 'x':
                error(lineno(), current_file_name(), "Oops! I haven't done hex escape sequences in strings yet. Nag me pls.");
            default:
                error(lineno(), current_file_name(), "Unknown escape sequence '\\%c' in string literal.", c);
            }
            escape = 0;
        } else if (c == '\\') {
            escape = 1;
            continue;
        }
        buf[len++] = c;
        if (len == alloc - 1) {
            alloc *= 2;
            buf = realloc(buf, alloc);
        }
    }
    error(start, current_file_name(), "EOF encountered while reading string literal.");
    return NULL;
}

Tok *check_reserved(char *buf) {
    if (!strcmp(buf, "true")) {
        Tok *t = make_token(TOK_BOOL);
        t->ival = 1;
        return t;
    } else if (!strcmp(buf, "false")) {
        Tok *t = make_token(TOK_BOOL);
        t->ival = 0;
        return t;
    } else if (!strcmp(buf, "fn")) {
        return make_token(TOK_FN);
    } else if (!strcmp(buf, "return")) {
        return make_token(TOK_RETURN);
    } else if (!strcmp(buf, "if")) {
        return make_token(TOK_IF);
    } else if (!strcmp(buf, "else")) {
        return make_token(TOK_ELSE);
    } else if (!strcmp(buf, "while")) {
        return make_token(TOK_WHILE);
    } else if (!strcmp(buf, "for")) {
        return make_token(TOK_FOR);
    } else if (!strcmp(buf, "in")) {
        return make_token(TOK_IN);
    } else if (!strcmp(buf, "extern")) {
        return make_token(TOK_EXTERN);
    } else if (!strcmp(buf, "type")) {
        return make_token(TOK_TYPE);
    } else if (!strcmp(buf, "struct")) {
        return make_token(TOK_STRUCT);
    } else if (!strcmp(buf, "new")) {
        return make_token(TOK_NEW);
    } else if (!strcmp(buf, "defer")) {
        return make_token(TOK_DEFER);
    } else if (!strcmp(buf, "break")) {
        return make_token(TOK_BREAK);
    } else if (!strcmp(buf, "continue")) {
        return make_token(TOK_CONTINUE);
    } else if (!strcmp(buf, "enum")) {
        return make_token(TOK_ENUM);
    } else if (!strcmp(buf, "use")) {
        return make_token(TOK_USE);
    } else if (!strcmp(buf, "impl")) {
        return make_token(TOK_IMPL);
    } else if (!strcmp(buf, "as")) {
        Tok *t = make_token(TOK_OP);
        t->op = OP_CAST;
        return t;
    }
    return NULL;
}

Tok *read_identifier(char c) {
    int alloc = 8;
    char *buf = malloc(alloc);
    buf[0] = c;
    int len = 1;
    for (;;) {
        c = get_char();
        if (!is_id_char(c)) {
            unget_char(c);
            break;
        }
        buf[len++] = c;
        if (len == alloc - 1) {
            alloc *= 2;
            buf = realloc(buf, alloc);
        }
    }
    buf[len] = 0;
    Tok *t = check_reserved(buf);
    if (t == NULL) {
        t = make_token(TOK_ID);
        t->sval = buf;
    }
    return t;
}

int valid_unary_op(int op) {
    switch (op) {
    case OP_NOT:
    case OP_PLUS:
    case OP_MINUS:
    case OP_REF:
    case OP_MUL:
    case OP_BINAND:
        return 1;
    }
    return 0;
}

int priority_of(Tok *t) {
    if (t->type == TOK_LPAREN || t->type == TOK_LSQUARE || t->type == TOK_DCOLON) {
        return 15;
    } else if (t->type == TOK_OPASSIGN) {
        return 1;
    } else if (t->type == TOK_OP || t->type == TOK_UOP) {
        switch (t->op) {
        case OP_ASSIGN:
            return 1;
        case OP_OR:
            return 2;
        case OP_AND:
            return 3;
        case OP_BINOR:
            return 4;
        case OP_XOR:
            return 5;
        case OP_BINAND:
            return 6;
        case OP_EQUALS: case OP_NEQUALS:
            return 7;
        case OP_GT: case OP_GTE: case OP_LT: case OP_LTE:
            return 8;
        case OP_LSHIFT: case OP_RSHIFT:
            return 9;
        case OP_PLUS: case OP_MINUS:
            return 10;
        case OP_MUL: case OP_DIV: case OP_MOD:
            return 11;
        case OP_NOT:
            return 12;
        case OP_CAST:
            return 13; // TODO this priority might be wrong
        case OP_REF: case OP_DEREF:
            return 14;
        case OP_DOT:
            return 15;
        default:
            return -1;
        }
    }
    return -1;
}

const char *tok_to_string(Tok *t) {
    if (t == NULL) {
        return "NULL";
    }
    switch (t->type) {
    case TOK_STR: case TOK_ID:
        return t->sval;
    case TOK_POLY: {
        int n = strlen(t->sval) + 2;
        char *c = malloc(sizeof(char) * n);
        snprintf(c, n, "$%s", t->sval);
        return c;
    }
    case TOK_INT: {
        int n = 1;
        if (t->ival < 0) n++;
        while ((n /= 10) > 0) {
            n++;
        }
        char *c = malloc(n);
        snprintf(c, n, "%lld", t->ival);
        return c;
    }
    case TOK_SEMI:
        return ";";
    case TOK_NL:
        return "\\n";
    case TOK_COLON:
        return ":";
    case TOK_SQUOTE:
        return "'";
    case TOK_ARROW:
        return "->";
    case TOK_DCOLON:
        return "::";
    case TOK_COMMA:
        return ",";
    case TOK_LPAREN:
        return "(";
    case TOK_RPAREN:
        return ")";
    case TOK_LSQUARE:
        return "[";
    case TOK_RSQUARE:
        return "]";
    case TOK_LBRACE:
        return "{";
    case TOK_RBRACE:
        return "}";
    case TOK_FN:
        return "fn";
    case TOK_OP:
    case TOK_UOP:
        return op_to_str(t->op);
    case TOK_OPASSIGN: {
        const char *op = op_to_str(t->op);
        char *m = malloc(sizeof(char) * (strlen(op) + 1));
        sprintf(m, "%s=", op);
        m[strlen(op)+1] = 0;
        return m;
    }
    case TOK_RETURN:
        return "return";
    case TOK_STRUCT:
        return "struct";
    case TOK_TYPE:
        return "type";
    case TOK_NEW:
        return "new";
    case TOK_DEFER:
        return "defer";
    case TOK_WHILE:
        return "while";
    case TOK_FOR:
        return "for";
    case TOK_IN:
        return "in";
    case TOK_BREAK:
        return "break";
    case TOK_CONTINUE:
        return "continue";
    case TOK_ELLIPSIS:
        return "...";
    case TOK_ENUM:
        return "enum";
    case TOK_IMPL:
        return "impl";
    case TOK_USE:
        return "use";
    default:
        return NULL;
    }
}

const char *token_type(TokType type) {
    switch (type) {
    case TOK_STR:
        return "STR";
    case TOK_ID:
        return "ID";
    case TOK_POLY:
        return "POLY";
    case TOK_INT:
        return "INT";
    case TOK_SEMI:
        return "SEMI";
    case TOK_NL:
        return "NEWLINE";
    case TOK_COLON:
        return "COLON";
    case TOK_SQUOTE:
        return "SQUOTE";
    case TOK_ARROW:
        return "ARROW";
    case TOK_DCOLON:
        return "DOUBLE COLON";
    case TOK_COMMA:
        return "COMMA";
    case TOK_LPAREN:
        return "LPAREN";
    case TOK_RPAREN:
        return "RPAREN";
    case TOK_LSQUARE:
        return "LSQUARE";
    case TOK_RSQUARE:
        return "RSQUARE";
    case TOK_OPASSIGN:
        return "OPASSIGN";
    case TOK_OP:
        return "OP";
    case TOK_UOP:
        return "UOP";
    case TOK_FN:
        return "FN";
    case TOK_RETURN:
        return "RETURN";
    case TOK_STRUCT:
        return "STRUCT";
    case TOK_TYPE:
        return "TYPE";
    case TOK_NEW:
        return "NEW";
    case TOK_DEFER:
        return "DEFER";
    case TOK_WHILE:
        return "WHILE";
    case TOK_FOR:
        return "FOR";
    case TOK_IN:
        return "IN";
    case TOK_BREAK:
        return "BREAK";
    case TOK_CONTINUE:
        return "CONTINUE";
    case TOK_ELLIPSIS:
        return "ELLIPSIS";
    case TOK_ENUM:
        return "ENUM";
    case TOK_USE:
        return "USE";
    default:
        return "BAD TOKEN";
    }
}

const char *op_to_str(int op) {
    switch (op) {
    case OP_PLUS: return "+";
    case OP_MINUS: return "-";
    case OP_MUL: return "*";
    case OP_DIV: return "/";
    case OP_MOD: return "%";
    case OP_XOR: return "^";
    case OP_BINAND: return "&";
    case OP_BINOR: return "|";
    case OP_ASSIGN: return "=";
    case OP_AND: return "&&";
    case OP_OR: return "||";
    case OP_EQUALS: return "==";
    case OP_NEQUALS: return "!=";
    case OP_NOT: return "!";
    case OP_GT: return ">";
    case OP_GTE: return ">=";
    case OP_LT: return "<";
    case OP_LTE: return "<=";
    case OP_LSHIFT: return "<<";
    case OP_RSHIFT: return ">>";
    case OP_DOT: return ".";
    case OP_REF: return "&";
    case OP_DEREF: return "*";
    case OP_CAST: return "as";
    default:
        return "BAD OP";
    }
}

Tok *expect(int type) {
    Tok *t = _next_token(0);
    if (type != TOK_NL && t->type == TOK_NL) {
        t = _next_token(0);
    }
    if (t == NULL || t->type != type) {
        error(lineno(), current_file_name(), "Expected token '%s', got '%s'.", token_type(type), tok_to_string(t));
    }
    return t;
}

Tok *expect_eol() {
    Tok *t = next_token_or_newline();
    if (t == NULL || !(t->type == TOK_SEMI || t->type == TOK_NL)) {
        error(lineno(), current_file_name(), "Expected end of line, got '%s'.", tok_to_string(t));
    }
    return t;
}

int is_comparison(int op) {
    return op == OP_EQUALS || op == OP_NEQUALS || op == OP_GT ||
        op == OP_GTE || op == OP_LT || op == OP_LTE;
}

int lineno() {
    if (source_stack == NULL) {
        error(-1, "internal", "Trying to get line number with no source file");
    }
    return source_stack->line;
}

char *current_file_name() {
    if (source_stack == NULL) {
        error(-1, "internal", "No file on the source stack!");
    }
    return source_stack->name;
}
