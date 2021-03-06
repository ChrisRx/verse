#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "array/array.h"
#include "hashmap/hashmap.h"
#include "scope.h"
#include "parse.h"
#include "package.h"
#include "semantics.h"

static Var **builtin_vars = NULL;

void define_builtin(Var *v) {
    array_push(builtin_vars, v);
}

Var *find_builtin_var(char *name) {
    for (int i = 0; i < array_len(builtin_vars); i++) {
        if (!strcmp(builtin_vars[i]->name, name)) {
            return builtin_vars[i];
        }
    }
    return NULL;
}

static Scope *builtin_types_scope = NULL;
void init_builtin_types() {
    builtin_types_scope = new_scope(NULL);
    init_types(builtin_types_scope);
}

Type **builtin_types() {
    return builtin_types_scope->used_types;
}

static Type **used_types = NULL;

Type **all_used_types() {
    return used_types;
}

Scope *new_scope(Scope *parent) {
    Scope *s = calloc(sizeof(Scope), 1);
    s->parent = parent;
    s->type = parent ? Simple : Root;
    s->package = parent ? parent->package : get_current_package();
    hashmap_init(&s->types);
    return s;
}

Scope *new_fn_scope(Scope *parent) {
    Scope *s = new_scope(parent);
    s->type = Function;
    return s;
}

Scope *new_loop_scope(Scope *parent) {
    Scope *s = new_scope(parent);
    s->type = Loop;
    return s;
}

Scope *closest_loop_scope(Scope *scope) {
    while (scope->parent != NULL && scope->type != Loop) {
        scope = scope->parent;
    }
    return scope;
}

Scope *closest_fn_scope(Scope *scope) {
    while (scope->parent != NULL && scope->type != Function) {
        scope = scope->parent;
    }
    return scope;
}

Type **fn_scope_return_type(Scope *scope) {
    scope = closest_fn_scope(scope);
    if (scope == NULL) {
        return NULL;
    }
    return scope->fn_var->type->resolved->fn.ret;
}

Type *lookup_polymorph(Scope *s, char *name) {
    TypeDef **tmp = NULL;
    for (; s != NULL; s = s->parent) {
        if (s->polymorph) {
            if ((tmp = hashmap_get(&s->polymorph->defs, name))) {
                return (*tmp)->type;
            }
        }
    }
    return NULL;
}

Type *lookup_local_type(Scope *s, char *name) {
    TypeDef **tmp = NULL;
    if (s->polymorph) {
        if ((tmp = hashmap_get(&s->polymorph->defs, name))) {
            return (*tmp)->type;
        }
    }
    if ((tmp = hashmap_get(&s->types, name))) {
        if ((*tmp)->proxy) {
            return lookup_local_type((*tmp)->proxy->scope, name);
        }
        return (*tmp)->type;
    }
    return NULL;
}

Type *lookup_type(Scope *s, char *name) {
    for (; s != NULL; s = s->parent) {
        Type *t = lookup_local_type(s, name);
        if (t != NULL) {
            return t;
        }
    }
    return lookup_local_type(builtin_types_scope, name);
}

int check_type(Type *a, Type *b);

void register_type(Type *t) {
    if (t->name) {
        TypeDef *tmp = find_type_definition(t);
        if (tmp && tmp->type == t) {
            return;
        }
    }
    for (int i = 0; i < array_len(used_types); i++) {
        if (used_types[i]->id == t->id) {
            return;
        }
        if (check_type(used_types[i], t)) {
            /*t->id = used_types[i]->id;*/
            *t = *used_types[i];
            return;
        }
    }

    array_push(used_types, t);

    ResolvedType *resolved = t->resolved;
    if (!resolved) {
        return;
    }

    switch (resolved->comp) {
    case STRUCT:
        for (int i = 0; i < array_len(resolved->st.member_types); i++) {
            register_type(resolved->st.member_types[i]);
        }
        break;
    case ENUM:
        register_type(resolved->en.inner);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        register_type(resolved->array.inner);
        break;
    case REF:
        register_type(resolved->ref.inner);
        break;
    case FUNC:
        for (int i = 0; i < array_len(resolved->fn.args); i++) {
            register_type(resolved->fn.args[i]);
        }
        for (int i = 0; i < array_len(resolved->fn.ret); i++) {
            register_type(resolved->fn.ret[i]);
        }
        break;
    case PARAMS:
        for (int i = 0; i < array_len(resolved->params.args); i++) {
            register_type(resolved->params.args[i]);
        }
        register_type(resolved->params.inner);
        break;
    default:
        break;
    }
}

Type *define_polymorph(Scope *s, Type *poly, Type *type, Ast *ast) {
    assert(poly->resolved->comp == POLYDEF);
    assert(s->polymorph != NULL);
    if (lookup_local_type(s, poly->name) != NULL) {
        error(-1, "internal", "Type '%s' already declared within this scope.", poly->name);
    }
    TypeDef *td = malloc(sizeof(TypeDef));
    td->name = poly->name;
    td->type = resolve_polymorph_recursively(type); // may not need this?
    td->ast = ast;
    hashmap_put(&s->polymorph->defs, poly->name, td);
    return make_type(s, poly->name);
}

int define_polydef_alias(Scope *scope, Type *t, Ast *ast) {
    int count = 0;
    ResolvedType *r = t->resolved;
    switch (r->comp) {
    case POLYDEF:
        define_type(scope, t->name, get_any_type(), ast);
        count++;
        break;
    case REF:
        count += define_polydef_alias(scope, r->ref.inner, ast);
        break;
    case ARRAY:
    case STATIC_ARRAY:
        count += define_polydef_alias(scope, r->array.inner, ast);
        break;
    case STRUCT:
        for (int i = 0; i < array_len(r->st.member_types); i++) {
            if (is_polydef(r->st.member_types[i])) {
                count += define_polydef_alias(scope, r->st.member_types[i], ast);
            }
        }
        break;
    case FUNC:
        for (int i = 0; i < array_len(r->fn.args); i++) {
            if (is_polydef(r->fn.args[i])) {
                count += define_polydef_alias(scope, r->fn.args[i], ast);
            }
        }
        for (int i = 0; i < array_len(r->fn.ret); i++) {
            if (is_polydef(r->fn.ret[i])) {
                count += define_polydef_alias(scope, r->fn.ret[i], ast);
            }
        }
        break;
    case PARAMS:
        for (int i = 0; i < array_len(r->params.args); i++) {
            if (is_polydef(r->params.args[i])) {
                count += define_polydef_alias(scope, r->params.args[i], ast);
            }
        }
        if (is_polydef(r->params.inner)) {
            count += define_polydef_alias(scope, r->params.inner, ast);
        }
        break;
    default:
        break;
    }
    return count;
}

Type *define_type(Scope *s, char *name, Type *type, Ast *ast) {
    if (lookup_local_type(s, name) != NULL) {
        error(-1, "internal", "Type '%s' already declared within this scope.", name);
    }

    Type *named = make_type(s, name);
    type = resolve_type(type);
    assert(type->resolved);
    named->resolved = type->resolved;
    if (!(type->resolved && type->resolved->comp == STRUCT && type->resolved->st.generic)) {
        register_type(named);
    }
    TypeDef *td = malloc(sizeof(TypeDef));
    td->name = name;
    td->type = named;
    td->ast = ast;
    td->proxy = NULL;
    hashmap_put(&s->types, name, td);
    return named;
}

Type *add_proxy_type(Scope *s, Package *src, TypeDef *orig) {
    assert(orig->type->resolved);
    TypeDef *td = malloc(sizeof(TypeDef));
    td->name = orig->name;
    td->type = orig->type;
    td->proxy = src;
    hashmap_put(&s->types, td->name, td);
    return td->type;
}

int local_type_name_conflict(Scope *scope, char *name) {
    return lookup_local_type(scope, name) != NULL;
}

TypeDef *find_type_definition(Type *t) {
    assert(t->name);
    Scope *scope = t->scope;
    TypeDef **tmp = NULL;
    while (scope) {
        // eh?
        if (scope->polymorph) {
            if ((tmp = hashmap_get(&scope->polymorph->defs, t->name))) {
                return *tmp;
            }
        }
        if ((tmp = hashmap_get(&scope->types, t->name))) {
            return *tmp;
        }
        scope = scope->parent;
    }
    if ((tmp = hashmap_get(&builtin_types_scope->types, t->name))) {
        return *tmp;
    }
    return NULL;
}

Var *lookup_local_var(Scope *scope, char *name) {
    for (int i = 0; i < array_len(scope->vars); i++) {
        Var *v = scope->vars[i];
        if (!strcmp(v->name, name)) {
            return v;
        }
    }
    return NULL;
}

static Var *_lookup_var(Scope *scope, char *name) {
    Var *v = lookup_local_var(scope, name);
    int in_function = scope->type == Function;
    while (v == NULL && scope->parent != NULL) {
        v = lookup_var(scope->parent, name);
        if (v != NULL) {
            if (!in_function || v->constant) {
                return v;
            }
            v = NULL;
        }
        scope = scope->parent;
        in_function = in_function || scope->type == Function;
    }
    return v;
}

Var *lookup_var(Scope *scope, char *name) {
    Var *v = _lookup_var(scope, name);
    if (v) {
        return v;
    }
    // package global
    while (scope->parent != NULL) {
        scope = scope->parent;
    }
    v = lookup_local_var(scope, name);
    if (v) {
        return v;
    }
    for (int i = 0; i < array_len(builtin_vars); i++) {
        if (!strcmp(builtin_vars[i]->name, name)) {
            return builtin_vars[i];
        }
    }
    return NULL;
}

TempVar *allocate_ast_temp_var(Scope *scope, struct Ast *ast) {
    return make_temp_var(scope, ast->var_type, ast->id);
}

TempVar *make_temp_var(Scope *scope, Type *t, int id) {
    Var *v = make_var("", t);
    v->temp = 1;
    array_push(scope->vars, v);

    TempVar *tv = malloc(sizeof(TempVar));
    tv->var = v;
    tv->ast_id = id;
    array_push(scope->temp_vars, tv);
    return tv;
}

Var *find_temp_var(Scope *scope, struct Ast *ast) {
    /*for (int i = 0; i < array_len(scope->temp_vars); i++) {*/
    for (int i = array_len(scope->temp_vars)-1; i >= 0; --i) {
        if (scope->temp_vars[i]->ast_id == ast->id) {
            return scope->temp_vars[i]->var;
        }
    }
    return NULL;
}

void remove_temp_var_by_id(Scope *scope, int id) {
    {
        int found = -1;
        for (int i = 0; i < array_len(scope->temp_vars); i++) {
            if (scope->temp_vars[i]->var->id == id) {
                found = i;
                break;
            }
        }
        if (found != -1) {
            for (int i = found; i < array_len(scope->temp_vars)-1; i++) {
                scope->temp_vars[i] = scope->temp_vars[i+1];
            }
        }
    }

    {
        int found = -1;
        for (int i = 0; i < array_len(scope->vars); i++) {
            if (scope->vars[i]->id == id) {
                found = i;
                break;
            }
        }
        if (found != -1) {
            for (int i = found; i < array_len(scope->vars)-1; i++) {
                scope->vars[i] = scope->vars[i+1];
            }
        }
    }
}
