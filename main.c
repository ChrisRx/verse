#include "src/compiler.h"
#include "src/util.h"

static int _indent = 0;

void emit(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void indent() {
    for (int i = 0; i < _indent; i++) {
        printf("    ");
    }
}

void label(char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void emit_string_comparison(Ast *ast) {
    if (ast->op == OP_NEQUALS) {
        printf("!");
    } else if (ast->op != OP_EQUALS) {
        error("Comparison of type '%s' is not valid for type 'string'.", op_to_str(ast->op));
    }
    if (ast->left->type == AST_STRING && ast->right->type == AST_STRING) {
        printf("%d", strcmp(ast->left->sval, ast->right->sval) ? 0 : 1);
        return;
    }
    if (ast->left->type == AST_STRING) {
        printf("streq_lit(");
        compile(ast->right);
        printf(",\"");
        print_quoted_string(ast->left->sval);
        printf("\",%d)", escaped_strlen(ast->left->sval));
    } else if (ast->right->type == AST_STRING) {
        printf("streq_lit(");
        compile(ast->left);
        printf(",\"");
        print_quoted_string(ast->right->sval);
        printf("\",%d)", escaped_strlen(ast->right->sval));
    } else {
        printf("streq(");
        compile(ast->left);
        printf(",");
        compile(ast->right);
        printf(")");
    }
}

void emit_comparison(Ast *ast) {
    if (var_type(ast->left)->base == STRING_T) {
        emit_string_comparison(ast);
        return;
    }
    printf("(");
    compile(ast->left);
    printf(" %s ", op_to_str(ast->op));
    compile(ast->right);
    printf(")");
    /*printf(" ? 1 : 0)");*/
}

void emit_string_binop(Ast *ast) {
    switch (ast->right->type) {
    case AST_IDENTIFIER:
    case AST_TEMP_VAR:
        printf("append_string(");
        compile(ast->left);
        printf(",");
        compile(ast->right);
        printf(")");
        break;
    case AST_STRING:
        printf("append_string_lit(");
        compile(ast->left);
        printf(",\"");
        print_quoted_string(ast->right->sval);
        printf("\",%d)", (int) escaped_strlen(ast->right->sval));
        break;
    default:
        error("Couldn't do the string binop?");
    }
}

void emit_uop(Ast *ast) {
    if (ast->op == OP_NOT) {
        printf("!");
    } else {
        error("Unkown unary operator '%s' (%s).", op_to_str(ast->op), ast->op);
    }
    compile(ast->right);
}

void emit_binop(Ast *ast) {
    if (ast->op == OP_ASSIGN) {
        if (is_dynamic(var_type(ast->left))) {
            if (ast->left->var->initialized) {
                compile(ast->right);
                printf(";\n");
                indent();
                printf("SWAP(_vs_%s,_tmp%d)", ast->left->var->name, ast->right->tmpvar->id);
            } else {
                printf("_vs_%s = ", ast->left->var->name);
                compile(ast->right);
                ast->left->var->initialized = 1;
                if (ast->right->type == AST_TEMP_VAR) {
                    ast->right->tmpvar->consumed = 1;
                }
            }
        } else {
            printf("_vs_%s = ", ast->left->var->name);
            compile(ast->right);
        }
        return;
    }
    if (is_comparison(ast->op)) {
        emit_comparison(ast);
        return;
    } else if (var_type(ast->left)->base == STRING_T) {
        emit_string_binop(ast);
        return;
    } else if (ast->op == OP_OR) {
        printf("(");
        compile(ast->left);
        printf(") || (");
        compile(ast->right);
        printf(")");
        return;
    } else if (ast->op == OP_AND) {
        printf("(");
        compile(ast->left);
        printf(") && ("); // does this short-circuit?
        compile(ast->right);
        printf(")");
        return;
    }
    printf("(");
    compile(ast->left);
    printf(" %s ", op_to_str(ast->op));
    compile(ast->right);
    printf(")");
}

void emit_tmpvar(Ast *ast) {
    if (ast->expr->type == AST_STRING) {
        printf("(_tmp%d = init_string(", ast->tmpvar->id);
        printf("\"");
        print_quoted_string(ast->expr->sval);
        printf("\"");
        printf("))");
    } else if (ast->expr->type == AST_IDENTIFIER) {
        printf("(_tmp%d = copy_string(_vs_%s))", ast->tmpvar->id, ast->expr->var->name);
    } else if (ast->expr->type == AST_BINOP && var_type(ast->expr)->base == STRING_T) {
        /*printf("_tmp%d = ", ast->tmpvar->id);*/
        emit_string_binop(ast->expr);
    } else if (ast->expr->type == AST_CALL) {
        printf("(_tmp%d = ", ast->tmpvar->id);
        compile(ast->expr);
        printf(")");
    } else {
        error("idk tmpvar");
    }
}

void emit_type(Type *type) {
    switch (type->base) {
    case INT_T:
        printf("int ");
        break;
    case BOOL_T:
        printf("unsigned char ");
        break;
    case STRING_T:
        printf("struct string_type *");
        break;
    case FN_T:
        printf("fn_type ");
        break;
    case VOID_T:
        printf("void ");
        break;
    default:
        error("wtf type");
    }
}

void emit_decl(Ast *ast) {
    if (ast->decl_var->type->base == FN_T) {
        emit_type(ast->decl_var->type->ret);
        printf("(*_vs_%s)(", ast->decl_var->name);
        for (int i = 0; i < ast->decl_var->type->nargs; i++) {
            emit_type(ast->decl_var->type->args[i]);
            if (i < ast->decl_var->type->nargs - 1) {
                printf(",");
            }
        }
        printf(")");
    } else {
        emit_type(ast->decl_var->type);
        printf("_vs_%s", ast->decl_var->name);
    }
    if (ast->init != NULL) {
        printf(" = ");
        compile(ast->init);
        if (ast->init->type == AST_TEMP_VAR) {
            ast->init->tmpvar->consumed = 1;
        }
        ast->decl_var->initialized = 1;
    }
}

void emit_func_decl(Ast *fn) {
    emit_type(fn->fn_decl_var->type->ret);
    printf("_vs_%s(", fn->fn_decl_var->name);
    for (int i = 0; i < fn->fn_decl_var->type->nargs; i++) {
        emit_type(fn->fn_decl_args[i]->type);
        printf("_vs_%s", fn->fn_decl_args[i]->name);
        if (i < fn->fn_decl_var->type->nargs - 1) {
            printf(",");
        }
    }
    printf(") ");
    compile(fn->fn_body);
}

void compile(Ast *ast) {
    switch (ast->type) {
    case AST_INTEGER:
        printf("%d", ast->ival);
        break;
    case AST_BOOL:
        printf("%d", ast->ival);
        break;
    case AST_STRING:
        printf("\"");
        print_quoted_string(ast->sval);
        printf("\"");
        break;
    case AST_UOP:
        emit_uop(ast);
        break;
    case AST_BINOP:
        emit_binop(ast);
        break;
    case AST_TEMP_VAR:
        emit_tmpvar(ast);
        break;
    case AST_IDENTIFIER:
        printf("_vs_%s", ast->var->name);
        break;
    case AST_RETURN:
        if (ast->ret_expr != NULL) {
            if (ast->ret_expr->type == AST_TEMP_VAR) {
                ast->ret_expr->tmpvar->consumed = 1;
            }
            emit_type(var_type(ast->ret_expr));
            printf("_ret = ");
            compile(ast->ret_expr); // need to do something with tmpvar instead
            printf(";");
        }
        printf("\n");
        emit_free_locals(ast->fn_scope);
        indent();
        printf("return");
        if (ast->ret_expr != NULL) {
            printf(" _ret");
        }
        break;
    case AST_DECL:
        emit_decl(ast);
        break;
    case AST_FUNC_DECL: 
        break;
    case AST_ANON_FUNC_DECL: 
        printf("_vs_%s", ast->fn_decl_var->name);
        /*emit_func_decl(ast);*/
        break;
    case AST_EXTERN_FUNC_DECL: 
        break;
    case AST_CALL:
        if (!ast->fn_var->ext) {
            printf("_vs_");
        }
        printf("%s(", ast->fn_var->name);
        for (int i = 0; i < ast->nargs; i++) {
            compile(ast->args[i]);
            if (ast->args[i]->type == AST_TEMP_VAR && is_dynamic(var_type(ast->args[i]))) {
                ast->args[i]->tmpvar->consumed = 1;
            }
            if (i != ast->nargs-1) {
                printf(",");
            }
        }
        printf(")");
        break;
    case AST_BLOCK:
        for (int i = 0; i < ast->num_statements; i++) {
            indent();
            compile(ast->statements[i]);
            if (ast->statements[i]->type != AST_CONDITIONAL && ast->statements[i]->type != AST_FUNC_DECL && ast->statements[i]->type != AST_ANON_FUNC_DECL && ast->statements[i]->type != AST_EXTERN_FUNC_DECL) {
                printf(";\n");
            }
        }
        break;
    case AST_SCOPE:
        emit_scope_start(ast);
        compile(ast->body);
        emit_scope_end(ast);
        break;
    case AST_CONDITIONAL:
        printf("if (");
        compile(ast->condition);
        /*printf(") == 1) {\n");*/
        printf(") {\n");
        _indent++;
        compile(ast->if_body);
        _indent--;
        if (ast->else_body != NULL) {
            indent();
            printf("} else {\n");
            _indent++;
            compile(ast->else_body);
            _indent--;
        }
        indent();
        printf("}\n");
        break;
    default:
        error("No idea how to deal with this.");
    }
}

void emit_scope_start(Ast *scope) {
    printf("{\n");
    _indent++;
    VarList *list = scope->locals;
    while (list != NULL) {
        if (list->item->temp) {
            indent();
            switch (list->item->type->base) {
            case INT_T:
                printf("int ");
                break;
            case BOOL_T:
                printf("unsigned char ");
                break;
            case STRING_T:
                printf("struct string_type *");
                break;
            case FN_T:
                printf("void *");
            default:
                error("wtf");
            }
            printf("_tmp%d = NULL;\n", list->item->id);
        }
        list = list->next;
    }
}

void emit_free(Var *var) {
    if ((!var->temp && !var->initialized) || (var->temp && var->consumed)) {
        return;
    }
    switch (var->type->base) {
    case STRING_T:
        if (var->temp) {
            indent();
            printf("if (_tmp%d != NULL) {\n", var->id); // maybe skip these
            indent();
            printf("    free(_tmp%d->bytes);\n", var->id);
            indent();
            printf("    free(_tmp%d);\n", var->id);
            indent();
            printf("}\n");
        } else {
            indent();
            printf("if (_vs_%s != NULL) {\n", var->name);
            indent();
            printf("    free(_vs_%s->bytes);\n", var->name);
            indent();
            printf("    free(_vs_%s);\n", var->name);
            indent();
            printf("}\n");
        }
        break;
    case INT_T:
    case BOOL_T:
    default:
        break;
    }
}

void emit_free_locals(Ast *scope) {
    while (scope->locals != NULL) {
        emit_free(scope->locals->item);
        scope->locals = scope->locals->next;
    }
}

void emit_scope_end(Ast *scope) {
    emit_free_locals(scope);
    _indent--;
    indent();
    printf("}\n");
}

void emit_forward_decl(Var *fn) {
    if (fn->ext) {
        printf("extern ");
    }
    emit_type(fn->type->ret);
    if (!fn->ext) {
        printf("_vs_");
    }
    printf("%s(", fn->name);
    for (int i = 0; i < fn->type->nargs; i++) {
        emit_type(fn->type->args[i]);
        /*printf("_vs_%s", fn->type->args[i]->name);*/
        printf("a%d", i);
        if (i < fn->type->nargs - 1) {
            printf(",");
        }
    }
    printf(");\n");
}

int main(int argc, char **argv) {
    int just_ast = 0;
    if (argc > 1 && !strcmp(argv[1], "-a")) {
        just_ast = 1;
    }
    Ast *root = generate_ast();
    root = parse_semantics(root, root);
    if (just_ast) {
        print_ast(root);
    } else {
        printf("#include \"prelude.c\"\n");
        _indent = 0;
        AstList *fnlist = get_global_funcs();
        while (fnlist != NULL) {
            emit_forward_decl(fnlist->item->fn_decl_var);
            fnlist = fnlist->next;
        }
        fnlist = get_global_funcs();
        while (fnlist != NULL) {
            emit_func_decl(fnlist->item);
            fnlist = fnlist->next;
        }
        compile(root->body);
        printf("int main(int argc, char** argv) {\n");
        /*emit_scope_start(root);*/
        /*indent();*/
        /*printf("    int exit_code = _fn_main();\n"); // TODO argc, argv*/
        /*emit_free_locals(root);*/
        /*indent();*/
        printf("    return _vs_main();\n");
        /*_indent--;*/
        /*indent();*/
        printf("}");
    }
    return 0;
}
