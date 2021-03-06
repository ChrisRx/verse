#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "scope.h"

Ast *first_pass(Scope *scope, Ast *ast);
Ast *check_semantics(Scope *scope, Ast *ast);
Ast **get_global_funcs();
void add_global_fn_decl(Ast *ast);

#endif
