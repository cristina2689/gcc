/* AddressSanitizer, a fast memory error detector.
   Copyright (C) 2012-2015 Free Software Foundation, Inc.
   Contributed by Kostya Serebryany <kcc@google.com>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */


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

/* gcc calls optimizations per function */
static unsigned int safestack_instrument ()
{
  int no_instr = 0;

  basic_block bb;
  gimple_stmt_iterator bi;

  if (cfun) {
    FOR_EACH_BB_FN (bb, cfun) {
      for (bi = gsi_start_bb (bb); !gsi_end_p (bi); gsi_next (&bi)) {
        no_instr++;
      }
    }
  }
  return 0;
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
  0, // TODO_update_ssa,  todo_flags_finish, not sure here */
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
  virtual unsigned int execute (function *) { return safestack_instrument (); }

}; // class pass_asan

} // anon namespace

gimple_opt_pass *
make_pass_safestack (gcc::context *ctxt)
{
  return new pass_safestack (ctxt);
}

