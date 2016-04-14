#ifndef UTILS_H
#define UTILS_H

void print_ssa_operands();
tree get_inner_ref(tree var);
void check_non_ssa_var();
void check_reference_node();
/* Static analysis, left here for the moment */
bool instrument_derefs(tree t);
bool instrument_derefs(tree t);
bool inner_ref_is_ssa_name(tree expr, tree ssa_name);
bool is_safe_access(tree t);
void compute_safe_accesses(hash_set<tree> *safe_vars);

#endif/* UTILS_H */
