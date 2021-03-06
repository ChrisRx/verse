#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "var.h"
#include "types.h"

void indent();
void change_indent(int n);
void codegen_set_output(FILE *f);
int write_bytes(const char *b, ...);

void emit_temp_var(Scope *scope, Ast *ast, int ref);

void emit_string_comparison(Scope *scope, Ast *ast);
void emit_comparison(Scope *scope, Ast *ast);

void emit_dot_op(Scope *scope, Ast *ast);
void emit_uop(Scope *scope, Ast *ast);

void emit_assignment(Scope *scope, Ast *ast);
void emit_string_binop(Scope *scope, Ast *ast);
void emit_binop(Scope *scope, Ast *ast);

void emit_copy(Scope *scope, Ast *ast);

void emit_type(Type *type);
void emit_typeinfo_decl(Scope *scope, Type *t);
void emit_typeinfo_init(Scope *scope, Type *t);
void emit_typeinfo_init_routine(Scope *root_scope, Type **builtins, Type **used_types);
void emit_init_routine(Package **packages, Scope *root_scope, Ast *root, Var *main_var);
void emit_entrypoint();

void compile_unspecified_array(Scope *scope, Ast *ast);
void compile_static_array(Scope *scope, Ast *ast);
void emit_static_array_decl(Scope *scope, Ast *ast);
void emit_static_array_copy(Scope *scope, Type *t, char *dest, char *src);

void emit_decl(Scope *scope, Ast *ast);
void emit_func_decl(Scope *scope, Ast *fn);
void emit_struct_decl(Scope *scope, Type *st);
void emit_var_decl(Scope *scope, Var *v);
void emit_forward_decl(Scope *scope, AstFnDecl *decl);

void emit_structmember(Scope *scope, char *name, Type *st);

void compile_ref(Scope *scope, Ast *ast);

void compile_block(Scope *scope, AstBlock *block);

void emit_any_wrapper(Scope *scope, Ast *ast);

void compile_call_arg(Scope *scope, Ast *ast, int arr);
void compile_fn_call(Scope *scope, Ast *ast);

void compile(Scope *scope, Ast *ast);

void emit_scope_start(Scope *scope);
void emit_free_struct(Scope *scope, char *name, Type *st, int is_ref);
void emit_free(Scope *scope, Var *var);
void emit_free_locals(Scope *scope);
void emit_scope_end(Scope *scope);
void emit_init_scope_end(Scope *scope);
void emit_deferred(Scope *scope);
void recursively_declare_types(int *declared_type_ids, Scope *root_scope, Type *t);

#endif
