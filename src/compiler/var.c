#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "var.h"

static int last_var_id = 0;

int new_var_id() {
    return last_var_id++;
}

Var *make_var(char *name, Type *type) {
    Var *var = calloc(sizeof(Var), 1);

    var->name = malloc(strlen(name)+1);
    strcpy(var->name, name);

    var->id = new_var_id();
    var->type = type;
    return var;
}

Var *copy_var(Scope *scope, Var *v) {
    Var *var = malloc(sizeof(Var));
    *var = *v;
    var->name = malloc(strlen(v->name)+1);
    strcpy(var->name, v->name); // just in case
    if (v->type != NULL) {
        var->type = copy_type(scope, v->type);
    }
    // TODO: check for type and members, proxy?
    // copy fn_decl?
    return var;
}

void init_struct_var(Var *var) {
    Type *type = resolve_alias(var->type);
    assert(type->comp == STRUCT);

    var->initialized = 1;
    var->members = malloc(sizeof(Var*)*type->st.nmembers);

    for (int i = 0; i < type->st.nmembers; i++) {
        int l = strlen(var->name)+strlen(type->st.member_names[i])+1;
        char *member_name;
        member_name = malloc((l+1)*sizeof(char));
        sprintf(member_name, "%s.%s", var->name, type->st.member_names[i]);
        member_name[l] = 0;

        var->members[i] = make_var(member_name, type->st.member_types[i]); // TODO
        var->members[i]->initialized = 1; // maybe wrong?
    }
}

VarList *varlist_append(VarList *list, Var *v) {
    VarList *vl = malloc(sizeof(VarList));
    vl->item = v;
    vl->next = list;
    return vl;
}

VarList *varlist_remove(VarList *list, char *name) {
    if (list != NULL) {
        if (!strcmp(list->item->name, name)) {
            return list->next;
        }
        VarList *curr = NULL;
        VarList *last = list;
        while (last->next != NULL) {
            curr = last->next;
            if (!strcmp(curr->item->name, name)) {
                last->next = curr->next;
                break;
            }
            last = curr;
        }
    }
    return list;
}

Var *varlist_find(VarList *list, char *name) {
    Var *v = NULL;
    while (list != NULL) {
        if (!strcmp(list->item->name, name)) {
            v = list->item;
            break;
        }
        list = list->next;
    }
    return v;
}

VarList *reverse_varlist(VarList *list) {
    VarList *tail = list;
    if (tail == NULL) {
        return NULL;
    }
    VarList *head = tail;
    VarList *tmp = head->next;
    head->next = NULL;
    while (tmp != NULL) {
        tail = head;
        head = tmp;
        tmp = tmp->next;
        head->next = tail;
    }
    return head;
}