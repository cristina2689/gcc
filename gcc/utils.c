#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "predict.h"
#include "dumpfile.h"
#include "stringpool.h"
#include "gimple-iterator.h"
#include "tree-ssa-alias.h"
#include "tree-ssanames.h"
#include "tree-ssa-operands.h"
#include "tree-phinodes.h"
#include "tree-ssa-address.h"
#include "gimple-ssa.h"
#include "tree-ssa-propagate.h"
#include "ssa-iterators.h"
#include "tree-ssanames.h"
#include "print-tree.h"


#include "gimple-pretty-print.h"

#include "utils.h"

tree get_inner_ref (tree var) {
    HOST_WIDE_INT bitpos, bitsize;
    machine_mode mode;
    tree offset;

    int reversep, unsignedp, volatilep = 0;

    /* get_inner_reference checks if var is COMPONENT_REF */
    return get_inner_reference(var, &bitsize, &bitpos,
                &offset, &mode, &unsignedp, &reversep,
                &volatilep, false);
}

void print_ssa_operands() {
    tree var;
    ssa_op_iter iter;
    gimple_stmt_iterator gsi;
    basic_block bb;

    fprintf (stdout, "Print all SSA operands\n");
    FOR_ALL_BB_FN(bb, cfun) {
        for (gsi = gsi_start_bb (bb);  !(gsi_end_p (gsi)); gsi_next(&gsi)) {
            gimple *stmt = gsi_stmt(gsi);
            FOR_EACH_SSA_TREE_OPERAND (var, stmt, iter, SSA_OP_ALL_OPERANDS) {
                print_gimple_stmt(stdout, stmt,1, 1);
                print_generic_expr (stdout, var, TDF_SLIM);
                printf("\n");
                tree ref = get_inner_ref (var);
                if (ref)
                    printf ("Inner reference: %s\n", get_name(ref));
            }
        }
    }
}

void check_non_ssa_var () {
    unsigned int i;
    tree var;

    FOR_EACH_LOCAL_DECL(cfun, i, var) {
        if (!SSA_VAR_P(var))
            printf("SSA_VAR_P invalidated on %s in fct %s \n",
                    get_name(var), function_name(cfun));
    }
}

void check_reference_node() {

    unsigned int i;
    tree var;

    FOR_EACH_LOCAL_DECL(cfun, i, var) {
        printf ("Local name var: %s\n", get_name(var));
        if (TREE_CODE (var) == SSA_NAME) {
            printf ("We have a badass over here: %s\n", get_name(var));
        }
    }
    for (unsigned int i = 0; i < num_ssa_names; i++) {
        if (ssa_name(i) == NULL_TREE) {
            printf ("THis is not waht we need %d - ssa_var is null \n", i );
            continue;
        }
        if (ssa_name(i) && SSA_NAME_VAR(ssa_name(i)) &&
                TREE_CODE (SSA_NAME_VAR (ssa_name(i))) == SSA_NAME) {
            printf ("Parent is SSA_NAME\n");
        }
    }
}

bool check_ssa_name_in_var_decl_list (tree ssa_node) {
    tree var;
    unsigned i;

    FILE *f = fopen ("ssa_vars", "a");
    FOR_EACH_LOCAL_DECL (cfun, i, var) {
        if (SSA_NAME_VAR (ssa_node) == var) {
            fprintf (f, "SSA_NAME_VAR of this %s\n", get_name(var));
            print_node_brief (f, "", ssa_node, TDF_SLIM);
            fclose (f);
            return true;
        }
    }
    fclose (f);
    return false;
}

void check_ssa_uses (tree ssa_node) {
    use_operand_p use_op;
    imm_use_iterator imm_iter;

    FILE *ff = fopen ("varuse", "a");

    FOR_EACH_IMM_USE_FAST (use_op, imm_iter, ssa_node) {
        gimple * use_stmt = USE_STMT(use_op);
        fprintf (ff, " var: %s\n", get_name (ssa_node));
        print_gimple_stmt (ff, use_stmt, 0, TDF_SLIM); 
    }
    fclose (ff);
}

void test_ssa_iterators (gimple *stmt) { // , tree ssa_var) {

    use_operand_p use_op;
    ssa_op_iter iter;
    imm_use_iterator imm_iter;
    tree ssa_var = NULL_TREE;
    FILE *f = fopen("ssa-stuff-use", "a");

    FOR_EACH_SSA_USE_OPERAND (use_op, stmt, iter, SSA_OP_ALL_USES) {
        tree use = USE_FROM_PTR (use_op);
        if (TREE_CODE (use) == SSA_NAME) {
            ssa_var = use;
            gimple *stmt = USE_STMT(use_op);
            print_node_brief (f, " ", use, 1);
            fprintf (f, " --- ");
            print_gimple_stmt (f, stmt, 0, TDF_SLIM);
            check_ssa_name_in_var_decl_list (use);
            check_ssa_uses (use);
            if (ssa_var == NULL_TREE)
                continue;

            FOR_EACH_IMM_USE_FAST (use_op, imm_iter, ssa_var) {
                gimple *use_stmt = USE_STMT(use_op);
                print_gimple_stmt (stdout, use_stmt, 0, TDF_SLIM);
            }
        }
    }
    fclose(f);
}

// return all ssa variables that refer to var
hash_set<tree> *check_ssa_for_var (tree var) {
    hash_set<tree> *ssa_vars = new hash_set<tree>;

    for (unsigned int i = 0; i < num_ssa_names; i++) {
        if (ssa_name(i) && SSA_NAME_VAR (ssa_name(i)) == var) {
            ssa_vars->add (ssa_name(i));
        }
    }
    return ssa_vars;
}

// NOT sure if there's any case in which instrument_derefs
// should be applied recursively on the LHS
bool instrument_derefs (tree t/*, location_t location, bool is_store*/) {

    // Memory accesses 
    switch (TREE_CODE (t)) {
        case ARRAY_REF:
        case COMPONENT_REF:
        case INDIRECT_REF:
        case MEM_REF:
        case VAR_DECL:
        case BIT_FIELD_REF:
            break;
        default:
            return false;
    }
    return true;
}

// Get inner reference and check if it's the ssa_name
bool inner_ref_is_ssa_name (tree expr, tree ssa_name) {
    printf ("Inner ref is ssa_name\n");
    if (expr != NULL_TREE && SSA_NAME_VAR (ssa_name) == expr) {
        return true;
    }
    return false;
}

bool is_safe_access (tree ssa_var) {
    use_operand_p use_op;
    imm_use_iterator imm_iter;

    FOR_EACH_IMM_USE_FAST (use_op, imm_iter, ssa_var) {
        gimple * use_stmt = USE_STMT(use_op);
        tree expr;
        /* TODO APPLY STATIC ANALYSIS HERE */

        /* CASE #1: Load */
        if (gimple_assign_load_p (use_stmt)) {
            continue;
        }
        /* CASE #2: Store */
        if (gimple_store_p (use_stmt)) {
            expr = gimple_assign_lhs (use_stmt);

            if (instrument_derefs (expr/*, gimple_location (use_stmt), is_store*/) ) {
                if (inner_ref_is_ssa_name (expr, ssa_var)) { 
                    //printf("Whatever you have imagined, t'was not true\n");
                    return false;
                }
            }
        }

        // CASE #3: va_arg

        // CASE #4: casts??

        // CASE #5: phi

        // CASE #6: call
    }
    return true;
}

void compute_safe_accesses (hash_set<tree>* safe_vars) {

    // 1. Iterate through all variables
    // 2. Keep a set of candidates in a vector/hashset
    // 3. Reduce the candidates with DFS analysis on instructions

    tree var;
    unsigned int i;

    FOR_EACH_LOCAL_DECL(cfun, i, var) {
        safe_vars->add (var);

        hash_set<tree> *ssa_names = check_ssa_for_var (var);

        /* Get all uses of variables and check if variable violates access */
        for (hash_set<tree>::iterator it = ssa_names->begin();
                it != ssa_names->end(); ++it) {
            if (!is_safe_access (*it)) {
                safe_vars->remove (var);
                break;
            }
        }
        delete ssa_names;
    }
}
