/* This plugin recursively dumps the source-code location ranges of
   expressions, at the pre-gimplification tree stage.  */
/* { dg-options "-O" } */

#include "gcc-plugin.h"
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "stringpool.h"
#include "toplev.h"
#include "basic-block.h"
#include "hash-table.h"
#include "vec.h"
#include "ggc.h"
#include "basic-block.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "gimple-fold.h"
#include "tree-eh.h"
#include "gimple-expr.h"
#include "is-a.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "tree.h"
#include "tree-pass.h"
#include "intl.h"
#include "plugin-version.h"
#include "diagnostic.h"
#include "context.h"
#include "gcc-rich-location.h"
#include "print-tree.h"

/*
  Hack: fails with linker error:
./diagnostic_plugin_show_trees.so: undefined symbol: _ZN17gcc_rich_location8add_exprEP9tree_node
  since nothing in the tree is using gcc_rich_location::add_expr yet.

  I've tried various workarounds (adding DEBUG_FUNCTION to the
  method, taking its address), but can't seem to fix it that way.
  So as a nasty workaround, the following material is copied&pasted
  from gcc-rich-location.c: */

static bool
get_range_for_expr (tree expr, location_range *r)
{
  if (EXPR_HAS_RANGE (expr))
    {
      source_range sr = EXPR_LOCATION_RANGE (expr);

      /* Do we have meaningful data?  */
      if (sr.m_start && sr.m_finish)
	{
	  r->m_start = expand_location (sr.m_start);
	  r->m_finish = expand_location (sr.m_finish);
	  return true;
	}
    }

  return false;
}

/* Add a range to the rich_location, covering expression EXPR. */

void
gcc_rich_location::add_expr (tree expr)
{
  gcc_assert (expr);

  location_range r;
  r.m_show_caret_p = false;
  if (get_range_for_expr (expr, &r))
    add_range (&r);
}

/* FIXME: end of material taken from gcc-rich-location.c */

int plugin_is_GPL_compatible;

static void
show_tree (tree node)
{
  if (!CAN_HAVE_RANGE_P (node))
    return;

  gcc_rich_location richloc (EXPR_LOCATION (node));
  richloc.add_expr (node);

  if (richloc.get_num_locations () < 2)
    {
      error_at_rich_loc (&richloc, "range not found");
      return;
    }

  enum tree_code code = TREE_CODE (node);

  location_range *range = richloc.get_range (1);
  inform_at_rich_loc (&richloc,
		      "%s at range %i:%i-%i:%i",
		      get_tree_code_name (code),
		      range->m_start.line,
		      range->m_start.column,
		      range->m_finish.line,
		      range->m_finish.column);

  /* Recurse.  */
  int min_idx = 0;
  int max_idx = TREE_OPERAND_LENGTH (node);
  switch (code)
    {
    case CALL_EXPR:
      min_idx = 3;
      break;

    default:
      break;
    }

  for (int i = min_idx; i < max_idx; i++)
    show_tree (TREE_OPERAND (node, i));
}

tree
cb_walk_tree_fn (tree * tp, int * walk_subtrees,
		 void * data ATTRIBUTE_UNUSED)
{
  if (TREE_CODE (*tp) != CALL_EXPR)
    return NULL_TREE;

  tree call_expr = *tp;
  tree fn = CALL_EXPR_FN (call_expr);
  if (TREE_CODE (fn) != ADDR_EXPR)
    return NULL_TREE;
  fn = TREE_OPERAND (fn, 0);
  if (TREE_CODE (fn) != FUNCTION_DECL)
    return NULL_TREE;
  if (strcmp (IDENTIFIER_POINTER (DECL_NAME (fn)), "__show_tree"))
    return NULL_TREE;

  /* Get arg 1; print it! */
  tree arg = CALL_EXPR_ARG (call_expr, 1);

  show_tree (arg);

  return NULL_TREE;
}

static void
callback (void *gcc_data, void *user_data)
{
  tree fndecl = (tree)gcc_data;
  walk_tree (&DECL_SAVED_TREE (fndecl), cb_walk_tree_fn, NULL, NULL);
}

int
plugin_init (struct plugin_name_args *plugin_info,
	     struct plugin_gcc_version *version)
{
  struct register_pass_info pass_info;
  const char *plugin_name = plugin_info->base_name;
  int argc = plugin_info->argc;
  struct plugin_argument *argv = plugin_info->argv;

  if (!plugin_default_version_check (version, &gcc_version))
    return 1;

  register_callback (plugin_name,
		     PLUGIN_PRE_GENERICIZE,
		     callback,
		     NULL);

  return 0;
}
