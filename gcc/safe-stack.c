#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "hash-set.h"
#include "machmode.h"
#include "vec.h"
#include "double-int.h"
#include "input.h"
#include "alias.h"
#include "symtab.h"
#include "options.h"
#include "wide-int.h"
#include "inchash.h"
#include "tree.h"
#include "fold-const.h"
#include "hash-table.h"
#include "predict.h"
#include "tm.h"
#include "hard-reg-set.h"
#include "function.h"
#include "dominance.h"
#include "cfg.h"
#include "cfganal.h"
#include "basic-block.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "gimple-expr.h"
#include "is-a.h"
#include "gimple.h"
#include "gimplify.h"
#include "gimple-iterator.h"
#include "calls.h"
#include "varasm.h"
#include "stor-layout.h"
#include "tree-iterator.h"
#include "hash-map.h"
#include "plugin-api.h"
#include "ipa-ref.h"
#include "cgraph.h"
#include "stringpool.h"
#include "tree-ssanames.h"
#include "tree-pass.h"
#include "gimple-pretty-print.h"
#include "target.h"
#include "hashtab.h"
#include "rtl.h"
#include "flags.h"
#include "statistics.h"
#include "real.h"
#include "fixed-value.h"
#include "insn-config.h"
#include "expmed.h"
#include "dojump.h"
#include "explow.h"
#include "emit-rtl.h"
#include "stmt.h"
#include "expr.h"
#include "insn-codes.h"
#include "optabs.h"
#include "output.h"
#include "tm_p.h"
#include "langhooks.h"
#include "alloc-pool.h"
#include "cfgloop.h"
#include "gimple-builder.h"
#include "ubsan.h"
#include "params.h"
#include "builtins.h"
#include "dbgcnt.h"
#include "diagnostic.h"
#include "gimple-walk.h"
#include "tree-cfg.h"
#include "tree-ssa-address.h"
#include "print-tree.h"
#include "gimple-ssa.h"
#include "tree-ssa-propagate.h"
#include "hash-map.h"

/* Build mapping between arrays and pointers */
hash_map<tree, tree> *references;

/* TODO - extra error checking, NULL types and so on */
tree insert_pointer_for_array (tree array_size, tree array) {

  basic_block bb = ENTRY_BLOCK_PTR_FOR_FN(cfun)->next_bb;
  basic_block end_bb = EXIT_BLOCK_PTR_FOR_FN(cfun)->prev_bb;
  tree vardecl = build_decl (DECL_SOURCE_LOCATION (current_function_decl),
                                VAR_DECL, get_identifier(get_name (array)),
                                //integer_ptr_type_node);
                                build_pointer_type (TREE_TYPE(TREE_TYPE(array))));
  TREE_ADDRESSABLE (vardecl) = 1;
  DECL_CONTEXT (vardecl) = current_function_decl;
  TREE_USED (vardecl) = 1;
  add_local_decl (cfun, vardecl);
  gimple_stmt_iterator gsi = gsi_start_bb (bb);
  tree fn, size;
  tree fn_ptr = make_ssa_name (vardecl);

  fn = builtin_decl_explicit (BUILT_IN_MALLOC);
  if (fn == NULL_TREE) {
    printf ("[insert_pointer_for_array] Tree node for malloc returned NULL\n");
    return NULL_TREE;
  }
  size = build_int_cst (integer_type_node, tree_to_uhwi (array_size));
  gimple *malloc_stmt = gimple_build_call (fn, 1, size);

  gimple_call_set_lhs (malloc_stmt, fn_ptr);
  gsi_insert_before (&gsi, malloc_stmt, GSI_SAME_STMT);

#if 0
  /* Get location to set for the new stmt */
  gimple *stmt = gsi_stmt (gsi);
  location_t loc = gimple_location (stmt);

  gimple_set_location (malloc_stmt, loc);
  debug_gimple_stmt(malloc_stmt);

  /* test mem_ref() to pointer */
  tree mem_ref = fold_build2 (MEM_REF, TREE_TYPE(TREE_TYPE (fn_ptr)), fn_ptr,
                          build_int_cst (TREE_TYPE(fn_ptr), 4));

  gimple *stmt = gimple_build_assign (mem_ref, build_int_cst (integer_type_node, 5));
  gsi_insert_after (&gsi, stmt, GSI_SAME_STMT);
#endif
  /* Insert free() */
  gimple_stmt_iterator gsi_end = gsi_last_bb (end_bb);
  tree free_fn = builtin_decl_explicit (BUILT_IN_FREE);

  if (free_fn == NULL_TREE) {
    printf ("[insert_pointer_for_array] Free function returned NULL");
  }
  gimple *free_stmt = gimple_build_call (free_fn, 1, fn_ptr);
  gsi_insert_before (&gsi_end, free_stmt, GSI_SAME_STMT);

  return fn_ptr;
}

static void record_array_decl() {
  tree var;
  unsigned int i, nr = 0;

  FOR_EACH_LOCAL_DECL(cfun, i, var) {
   if (!is_global_var(var)) {
       tree var_type = TREE_TYPE(var);
       if (TREE_CODE(var) == VAR_DECL &&
           TREE_CODE(var_type) == ARRAY_TYPE ) {

            tree size = DECL_SIZE_UNIT(var);/* in bytes */
            tree pointer = insert_pointer_for_array (size, var);
            if (pointer != NULL_TREE)
              references->put (var, pointer);
            nr++;
       }
   }
  }
  printf ("--Found %d array type vars\n", nr);
}

bool has_array_ref (tree node, gimple *stmt) {

  if (node && TREE_CODE (node) == ARRAY_REF) {
    printf ("[has_array_ref] Found in statement: ");
    print_gimple_stmt (stdout, stmt, 0, TDF_SLIM);
    return true;
  }
  return false;
}

tree build_mem_ref_on_array (tree node) {

  if (node && TREE_CODE (node) != ARRAY_REF) {
     printf ("[build_mem_ref_on_array] NULL node sent!\n");
     return NULL_TREE;
  }
  tree index = TREE_OPERAND (node, 1); // access index in A[index]

  if (TREE_CODE (index) == INTEGER_CST) {
    return fold_build2 (MEM_REF, TREE_TYPE(TREE_TYPE (node)), node, index);
  }
  printf ("[build_mem_ref_on_array] Not implemented\n");
  return NULL_TREE;
}

tree build_mem_ref_on_pointer (tree array, tree pointer) {

  if (array && TREE_CODE (array) != ARRAY_REF) {
    printf ("[build_mem_ref_on_pointer] NULL node sent!\n");
    return NULL_TREE;
  }
  tree index = TREE_OPERAND (array, 1);
  if (TREE_CODE (index) == INTEGER_CST ) {
    return fold_build2 (MEM_REF, TREE_TYPE (TREE_TYPE (pointer)),
                        pointer, build_int_cst (TREE_TYPE(pointer), tree_to_uhwi(index)));
  }
  printf ("[build_mem_ref_on_pointer] NOT IMPLEMENTED\n");
  return NULL_TREE;
}

tree find_array_ref (tree node) {

  if (node && TREE_CODE (node) == ARRAY_REF) {
    HOST_WIDE_INT bitpos, bitsize;
    tree offset;
    machine_mode mode;

    int reversep, unsignedp, volatilep = 0;
    return get_inner_reference (node, &bitsize, &bitpos, &offset, &mode,
                                &unsignedp, &reversep, &volatilep, false);
  }
  return NULL_TREE;
}

bool replace_array_ref (gimple *stmt, tree node, unsigned int index) {

  if (!node || index > 2 || !has_array_ref (node, stmt))
    return false;
  tree *pointer = references->get (find_array_ref(node));
  if (!pointer) {
    printf ("[replace_array_ref] Null pointer in map\n");
    return false;
  }
  tree mem_ref = build_mem_ref_on_pointer (node, *pointer);
  if (index == 0)
    gimple_assign_set_lhs (stmt, mem_ref);
  if (index == 1)
    gimple_assign_set_rhs1 (stmt, mem_ref);
  if (index == 2)
    gimple_assign_set_rhs2 (stmt, mem_ref);
  update_stmt (stmt);
  printf ("Updated gimple_stmt to: \n");
  print_gimple_stmt (stdout, stmt, 0, TDF_SLIM);
  return true;
}

static void safestack_instrument ()
{
  references = new hash_map<tree, tree> ;
  record_array_decl ();
  basic_block bb;

  FOR_ALL_BB_FN(bb, cfun) {

    gimple_stmt_iterator gsi;

    for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)) {
      gimple *stmt = gsi_stmt (gsi);

      switch (gimple_code(stmt)) {
        case GIMPLE_ASSIGN : {
              replace_array_ref (stmt, gimple_assign_lhs (stmt), 0);
              replace_array_ref (stmt, gimple_assign_rhs1 (stmt), 1);
              replace_array_ref (stmt, gimple_assign_rhs2 (stmt), 2);
#if 0
              print_node_brief (stdout, " ", lhsop, 1);
              print_node_brief (stdout, " ", rhsop1, 1);
              print_node_brief (stdout, " ", rhsop2, 1);
              printf ("\n");

              /* Test ssa replace uses by */
              if (print_stmt_ssa_name (lhsop, rhsop1, rhsop2)) {
                replace_uses_by (lhsop, replace_var);
                print_gimple_stmt (stdout, stmt, 0, TDF_SLIM);
                printf ("\n");
              }
#endif
         break;
        }
        case GIMPLE_BIND: { // Never found
              print_gimple_stmt (stdout, stmt, 0, TDF_SLIM);
              printf ("GIMPLE_BIND - not implemented \n");
              break;
        }
        case GIMPLE_CALL: {
              vec <tree> args = vNULL;
              gcall *call = as_a <gcall *> (gsi_stmt(gsi));
              unsigned nargs = gimple_call_num_args(call);
              /* Update arguments to call */
              for (unsigned int i = 0; i < nargs; i++) {
                tree node = gimple_call_arg (call, i);
                    if (node && TREE_CODE (node) == ARRAY_REF) {
                      tree ref = find_array_ref (node);
                      tree *pointer = references->get (ref);
                      if (!pointer) {
                        printf ("GIMPLE_CALL - null pointer in hash\n");
                        break;
                      }
                      if (ref != NULL_TREE) {
                          tree mem_ref = build_mem_ref_on_pointer (ref, *pointer);
                          args.safe_push (mem_ref);
                      }
                    } else {
                          args.safe_push (node);
                    }
              }
              gcall *new_stmt = gimple_build_call_vec (gimple_op (call, 1), args);
              if (gimple_call_lhs (call)) {
                gimple_call_set_lhs (new_stmt, gimple_call_lhs (call));
              }
              gimple_call_copy_flags (new_stmt, call);
              gimple_call_set_chain (new_stmt, gimple_call_chain (call));
              gsi_replace (&gsi, new_stmt, true);
              break;
        }
        case GIMPLE_RETURN: {
              /* TODO */
              break;
        }
        default : {
              /* TODO */
              break;
      }
      }
    }
  }
  delete references;
}

/* Return true if we shoud apply this optimization */
static bool gate_safestack (void)
{
  return flag_pass_safestack; 
      /* Don't use true here if instrumentation function does a lot of stuff 
		 since the opt might get run on every function and compilation might fail since
		 some functions have limited resources 
		*/
}

namespace {

const pass_data pass_data_safestack =
{
  GIMPLE_PASS, /* type */
  "safestack", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_NONE, /* tv_id - some timevalue */
  ( PROP_ssa | PROP_cfg | PROP_gimple_leh ), /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  TODO_update_ssa, // TODO_update_ssa,  todo_flags_finish, not sure here */
};

class pass_safestack : public gimple_opt_pass
{
public:
  pass_safestack (gcc::context *ctxt)
    : gimple_opt_pass (pass_data_safestack, ctxt)
  {}

  //  opt_pass methods:
  opt_pass * clone () { return new pass_safestack (m_ctxt); }
  virtual bool gate (function *) { return gate_safestack (); }
  virtual unsigned int execute (function *) {
    safestack_instrument();
    //insert_ssa_var();
    return 0;
  }

};

} // anon namespace

gimple_opt_pass *
make_pass_safestack (gcc::context *ctxt)
{
  return new pass_safestack (ctxt);
}
