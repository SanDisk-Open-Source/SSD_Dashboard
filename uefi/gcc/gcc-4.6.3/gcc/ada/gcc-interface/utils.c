/****************************************************************************
 *                                                                          *
 *                         GNAT COMPILER COMPONENTS                         *
 *                                                                          *
 *                                U T I L S                                 *
 *                                                                          *
 *                          C Implementation File                           *
 *                                                                          *
 *          Copyright (C) 1992-2011, Free Software Foundation, Inc.         *
 *                                                                          *
 * GNAT is free software;  you can  redistribute it  and/or modify it under *
 * terms of the  GNU General Public License as published  by the Free Soft- *
 * ware  Foundation;  either version 3,  or (at your option) any later ver- *
 * sion.  GNAT is distributed in the hope that it will be useful, but WITH- *
 * OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License *
 * for  more details.  You should have received a copy of the GNU General   *
 * Public License along with GCC; see the file COPYING3.  If not see        *
 * <http://www.gnu.org/licenses/>.                                          *
 *                                                                          *
 * GNAT was originally developed  by the GNAT team at  New York University. *
 * Extensive contributions were provided by Ada Core Technologies Inc.      *
 *                                                                          *
 ****************************************************************************/

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "flags.h"
#include "toplev.h"
#include "diagnostic-core.h"
#include "output.h"
#include "ggc.h"
#include "debug.h"
#include "convert.h"
#include "target.h"
#include "langhooks.h"
#include "cgraph.h"
#include "tree-dump.h"
#include "tree-inline.h"
#include "tree-iterator.h"

#include "ada.h"
#include "types.h"
#include "atree.h"
#include "elists.h"
#include "namet.h"
#include "nlists.h"
#include "stringt.h"
#include "uintp.h"
#include "fe.h"
#include "sinfo.h"
#include "einfo.h"
#include "ada-tree.h"
#include "gigi.h"

#ifndef MAX_BITS_PER_WORD
#define MAX_BITS_PER_WORD  BITS_PER_WORD
#endif

/* If nonzero, pretend we are allocating at global level.  */
int force_global;

/* The default alignment of "double" floating-point types, i.e. floating
   point types whose size is equal to 64 bits, or 0 if this alignment is
   not specifically capped.  */
int double_float_alignment;

/* The default alignment of "double" or larger scalar types, i.e. scalar
   types whose size is greater or equal to 64 bits, or 0 if this alignment
   is not specifically capped.  */
int double_scalar_alignment;

/* Tree nodes for the various types and decls we create.  */
tree gnat_std_decls[(int) ADT_LAST];

/* Functions to call for each of the possible raise reasons.  */
tree gnat_raise_decls[(int) LAST_REASON_CODE + 1];

/* Likewise, but with extra info for each of the possible raise reasons.  */
tree gnat_raise_decls_ext[(int) LAST_REASON_CODE + 1];

/* Forward declarations for handlers of attributes.  */
static tree handle_const_attribute (tree *, tree, tree, int, bool *);
static tree handle_nothrow_attribute (tree *, tree, tree, int, bool *);
static tree handle_pure_attribute (tree *, tree, tree, int, bool *);
static tree handle_novops_attribute (tree *, tree, tree, int, bool *);
static tree handle_nonnull_attribute (tree *, tree, tree, int, bool *);
static tree handle_sentinel_attribute (tree *, tree, tree, int, bool *);
static tree handle_noreturn_attribute (tree *, tree, tree, int, bool *);
static tree handle_leaf_attribute (tree *, tree, tree, int, bool *);
static tree handle_malloc_attribute (tree *, tree, tree, int, bool *);
static tree handle_type_generic_attribute (tree *, tree, tree, int, bool *);
static tree handle_vector_size_attribute (tree *, tree, tree, int, bool *);
static tree handle_vector_type_attribute (tree *, tree, tree, int, bool *);

/* Fake handler for attributes we don't properly support, typically because
   they'd require dragging a lot of the common-c front-end circuitry.  */
static tree fake_attribute_handler      (tree *, tree, tree, int, bool *);

/* Table of machine-independent internal attributes for Ada.  We support
   this minimal set of attributes to accommodate the needs of builtins.  */
const struct attribute_spec gnat_internal_attribute_table[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req, handler } */
  { "const",        0, 0,  true,  false, false, handle_const_attribute   },
  { "nothrow",      0, 0,  true,  false, false, handle_nothrow_attribute },
  { "pure",         0, 0,  true,  false, false, handle_pure_attribute },
  { "no vops",      0, 0,  true,  false, false, handle_novops_attribute },
  { "nonnull",      0, -1, false, true,  true,  handle_nonnull_attribute },
  { "sentinel",     0, 1,  false, true,  true,  handle_sentinel_attribute },
  { "noreturn",     0, 0,  true,  false, false, handle_noreturn_attribute },
  { "leaf",         0, 0,  true,  false, false, handle_leaf_attribute },
  { "malloc",       0, 0,  true,  false, false, handle_malloc_attribute },
  { "type generic", 0, 0,  false, true, true, handle_type_generic_attribute },

  { "vector_size",  1, 1,  false, true, false,  handle_vector_size_attribute },
  { "vector_type",  0, 0,  false, true, false,  handle_vector_type_attribute },
  { "may_alias",    0, 0, false, true, false, NULL },

  /* ??? format and format_arg are heavy and not supported, which actually
     prevents support for stdio builtins, which we however declare as part
     of the common builtins.def contents.  */
  { "format",     3, 3,  false, true,  true,  fake_attribute_handler },
  { "format_arg", 1, 1,  false, true,  true,  fake_attribute_handler },

  { NULL,         0, 0, false, false, false, NULL }
};

/* Associates a GNAT tree node to a GCC tree node. It is used in
   `save_gnu_tree', `get_gnu_tree' and `present_gnu_tree'. See documentation
   of `save_gnu_tree' for more info.  */
static GTY((length ("max_gnat_nodes"))) tree *associate_gnat_to_gnu;

#define GET_GNU_TREE(GNAT_ENTITY)	\
  associate_gnat_to_gnu[(GNAT_ENTITY) - First_Node_Id]

#define SET_GNU_TREE(GNAT_ENTITY,VAL)	\
  associate_gnat_to_gnu[(GNAT_ENTITY) - First_Node_Id] = (VAL)

#define PRESENT_GNU_TREE(GNAT_ENTITY)	\
  (associate_gnat_to_gnu[(GNAT_ENTITY) - First_Node_Id] != NULL_TREE)

/* Associates a GNAT entity to a GCC tree node used as a dummy, if any.  */
static GTY((length ("max_gnat_nodes"))) tree *dummy_node_table;

#define GET_DUMMY_NODE(GNAT_ENTITY)	\
  dummy_node_table[(GNAT_ENTITY) - First_Node_Id]

#define SET_DUMMY_NODE(GNAT_ENTITY,VAL)	\
  dummy_node_table[(GNAT_ENTITY) - First_Node_Id] = (VAL)

#define PRESENT_DUMMY_NODE(GNAT_ENTITY)	\
  (dummy_node_table[(GNAT_ENTITY) - First_Node_Id] != NULL_TREE)

/* This variable keeps a table for types for each precision so that we only
   allocate each of them once. Signed and unsigned types are kept separate.

   Note that these types are only used when fold-const requests something
   special.  Perhaps we should NOT share these types; we'll see how it
   goes later.  */
static GTY(()) tree signed_and_unsigned_types[2 * MAX_BITS_PER_WORD + 1][2];

/* Likewise for float types, but record these by mode.  */
static GTY(()) tree float_types[NUM_MACHINE_MODES];

/* For each binding contour we allocate a binding_level structure to indicate
   the binding depth.  */

struct GTY((chain_next ("%h.chain"))) gnat_binding_level {
  /* The binding level containing this one (the enclosing binding level). */
  struct gnat_binding_level *chain;
  /* The BLOCK node for this level.  */
  tree block;
  /* If nonzero, the setjmp buffer that needs to be updated for any
     variable-sized definition within this context.  */
  tree jmpbuf_decl;
};

/* The binding level currently in effect.  */
static GTY(()) struct gnat_binding_level *current_binding_level;

/* A chain of gnat_binding_level structures awaiting reuse.  */
static GTY((deletable)) struct gnat_binding_level *free_binding_level;

/* An array of global declarations.  */
static GTY(()) VEC(tree,gc) *global_decls;

/* An array of builtin function declarations.  */
static GTY(()) VEC(tree,gc) *builtin_decls;

/* An array of global renaming pointers.  */
static GTY(()) VEC(tree,gc) *global_renaming_pointers;

/* A chain of unused BLOCK nodes. */
static GTY((deletable)) tree free_block_chain;

static tree merge_sizes (tree, tree, tree, bool, bool);
static tree compute_related_constant (tree, tree);
static tree split_plus (tree, tree *);
static tree float_type_for_precision (int, enum machine_mode);
static tree convert_to_fat_pointer (tree, tree);
static tree convert_to_thin_pointer (tree, tree);
static bool potential_alignment_gap (tree, tree, tree);
static void process_attributes (tree, struct attrib *);

/* Initialize the association of GNAT nodes to GCC trees.  */

void
init_gnat_to_gnu (void)
{
  associate_gnat_to_gnu = ggc_alloc_cleared_vec_tree (max_gnat_nodes);
}

/* GNAT_ENTITY is a GNAT tree node for an entity.  Associate GNU_DECL, a GCC
   tree node, with GNAT_ENTITY.  If GNU_DECL is not a ..._DECL node, abort.
   If NO_CHECK is true, the latter check is suppressed.

   If GNU_DECL is zero, reset a previous association.  */

void
save_gnu_tree (Entity_Id gnat_entity, tree gnu_decl, bool no_check)
{
  /* Check that GNAT_ENTITY is not already defined and that it is being set
     to something which is a decl.  If that is not the case, this usually
     means GNAT_ENTITY is defined twice, but occasionally is due to some
     Gigi problem.  */
  gcc_assert (!(gnu_decl
		&& (PRESENT_GNU_TREE (gnat_entity)
		    || (!no_check && !DECL_P (gnu_decl)))));

  SET_GNU_TREE (gnat_entity, gnu_decl);
}

/* GNAT_ENTITY is a GNAT tree node for an entity.  Return the GCC tree node
   that was associated with it.  If there is no such tree node, abort.

   In some cases, such as delayed elaboration or expressions that need to
   be elaborated only once, GNAT_ENTITY is really not an entity.  */

tree
get_gnu_tree (Entity_Id gnat_entity)
{
  gcc_assert (PRESENT_GNU_TREE (gnat_entity));
  return GET_GNU_TREE (gnat_entity);
}

/* Return nonzero if a GCC tree has been associated with GNAT_ENTITY.  */

bool
present_gnu_tree (Entity_Id gnat_entity)
{
  return PRESENT_GNU_TREE (gnat_entity);
}

/* Initialize the association of GNAT nodes to GCC trees as dummies.  */

void
init_dummy_type (void)
{
  dummy_node_table = ggc_alloc_cleared_vec_tree (max_gnat_nodes);
}

/* Make a dummy type corresponding to GNAT_TYPE.  */

tree
make_dummy_type (Entity_Id gnat_type)
{
  Entity_Id gnat_underlying = Gigi_Equivalent_Type (gnat_type);
  tree gnu_type;

  /* If there is an equivalent type, get its underlying type.  */
  if (Present (gnat_underlying))
    gnat_underlying = Underlying_Type (gnat_underlying);

  /* If there was no equivalent type (can only happen when just annotating
     types) or underlying type, go back to the original type.  */
  if (No (gnat_underlying))
    gnat_underlying = gnat_type;

  /* If it there already a dummy type, use that one.  Else make one.  */
  if (PRESENT_DUMMY_NODE (gnat_underlying))
    return GET_DUMMY_NODE (gnat_underlying);

  /* If this is a record, make a RECORD_TYPE or UNION_TYPE; else make
     an ENUMERAL_TYPE.  */
  gnu_type = make_node (Is_Record_Type (gnat_underlying)
			? tree_code_for_record_type (gnat_underlying)
			: ENUMERAL_TYPE);
  TYPE_NAME (gnu_type) = get_entity_name (gnat_type);
  TYPE_DUMMY_P (gnu_type) = 1;
  TYPE_STUB_DECL (gnu_type)
    = create_type_stub_decl (TYPE_NAME (gnu_type), gnu_type);
  if (Is_By_Reference_Type (gnat_type))
    TREE_ADDRESSABLE (gnu_type) = 1;

  SET_DUMMY_NODE (gnat_underlying, gnu_type);

  return gnu_type;
}

/* Return nonzero if we are currently in the global binding level.  */

int
global_bindings_p (void)
{
  return ((force_global || !current_function_decl) ? -1 : 0);
}

/* Enter a new binding level.  */

void
gnat_pushlevel (void)
{
  struct gnat_binding_level *newlevel = NULL;

  /* Reuse a struct for this binding level, if there is one.  */
  if (free_binding_level)
    {
      newlevel = free_binding_level;
      free_binding_level = free_binding_level->chain;
    }
  else
    newlevel = ggc_alloc_gnat_binding_level ();

  /* Use a free BLOCK, if any; otherwise, allocate one.  */
  if (free_block_chain)
    {
      newlevel->block = free_block_chain;
      free_block_chain = BLOCK_CHAIN (free_block_chain);
      BLOCK_CHAIN (newlevel->block) = NULL_TREE;
    }
  else
    newlevel->block = make_node (BLOCK);

  /* Point the BLOCK we just made to its parent.  */
  if (current_binding_level)
    BLOCK_SUPERCONTEXT (newlevel->block) = current_binding_level->block;

  BLOCK_VARS (newlevel->block) = NULL_TREE;
  BLOCK_SUBBLOCKS (newlevel->block) = NULL_TREE;
  TREE_USED (newlevel->block) = 1;

  /* Add this level to the front of the chain (stack) of active levels.  */
  newlevel->chain = current_binding_level;
  newlevel->jmpbuf_decl = NULL_TREE;
  current_binding_level = newlevel;
}

/* Set SUPERCONTEXT of the BLOCK for the current binding level to FNDECL
   and point FNDECL to this BLOCK.  */

void
set_current_block_context (tree fndecl)
{
  BLOCK_SUPERCONTEXT (current_binding_level->block) = fndecl;
  DECL_INITIAL (fndecl) = current_binding_level->block;
  set_block_for_group (current_binding_level->block);
}

/* Set the jmpbuf_decl for the current binding level to DECL.  */

void
set_block_jmpbuf_decl (tree decl)
{
  current_binding_level->jmpbuf_decl = decl;
}

/* Get the jmpbuf_decl, if any, for the current binding level.  */

tree
get_block_jmpbuf_decl (void)
{
  return current_binding_level->jmpbuf_decl;
}

/* Exit a binding level.  Set any BLOCK into the current code group.  */

void
gnat_poplevel (void)
{
  struct gnat_binding_level *level = current_binding_level;
  tree block = level->block;

  BLOCK_VARS (block) = nreverse (BLOCK_VARS (block));
  BLOCK_SUBBLOCKS (block) = blocks_nreverse (BLOCK_SUBBLOCKS (block));

  /* If this is a function-level BLOCK don't do anything.  Otherwise, if there
     are no variables free the block and merge its subblocks into those of its
     parent block.  Otherwise, add it to the list of its parent.  */
  if (TREE_CODE (BLOCK_SUPERCONTEXT (block)) == FUNCTION_DECL)
    ;
  else if (BLOCK_VARS (block) == NULL_TREE)
    {
      BLOCK_SUBBLOCKS (level->chain->block)
	= chainon (BLOCK_SUBBLOCKS (block),
		   BLOCK_SUBBLOCKS (level->chain->block));
      BLOCK_CHAIN (block) = free_block_chain;
      free_block_chain = block;
    }
  else
    {
      BLOCK_CHAIN (block) = BLOCK_SUBBLOCKS (level->chain->block);
      BLOCK_SUBBLOCKS (level->chain->block) = block;
      TREE_USED (block) = 1;
      set_block_for_group (block);
    }

  /* Free this binding structure.  */
  current_binding_level = level->chain;
  level->chain = free_binding_level;
  free_binding_level = level;
}

/* Exit a binding level and discard the associated BLOCK.  */

void
gnat_zaplevel (void)
{
  struct gnat_binding_level *level = current_binding_level;
  tree block = level->block;

  BLOCK_CHAIN (block) = free_block_chain;
  free_block_chain = block;

  /* Free this binding structure.  */
  current_binding_level = level->chain;
  level->chain = free_binding_level;
  free_binding_level = level;
}

/* Records a ..._DECL node DECL as belonging to the current lexical scope
   and uses GNAT_NODE for location information and propagating flags.  */

void
gnat_pushdecl (tree decl, Node_Id gnat_node)
{
  /* If this decl is public external or at toplevel, there is no context.  */
  if ((TREE_PUBLIC (decl) && DECL_EXTERNAL (decl)) || global_bindings_p ())
    DECL_CONTEXT (decl) = 0;
  else
    {
      DECL_CONTEXT (decl) = current_function_decl;

      /* Functions imported in another function are not really nested.
	 For really nested functions mark them initially as needing
	 a static chain for uses of that flag before unnesting;
	 lower_nested_functions will then recompute it.  */
      if (TREE_CODE (decl) == FUNCTION_DECL && !TREE_PUBLIC (decl))
	DECL_STATIC_CHAIN (decl) = 1;
    }

  TREE_NO_WARNING (decl) = (gnat_node == Empty || Warnings_Off (gnat_node));

  /* Set the location of DECL and emit a declaration for it.  */
  if (Present (gnat_node))
    Sloc_to_locus (Sloc (gnat_node), &DECL_SOURCE_LOCATION (decl));
  add_decl_expr (decl, gnat_node);

  /* Put the declaration on the list.  The list of declarations is in reverse
     order.  The list will be reversed later.  Put global declarations in the
     globals list and local ones in the current block.  But skip TYPE_DECLs
     for UNCONSTRAINED_ARRAY_TYPE in both cases, as they will cause trouble
     with the debugger and aren't needed anyway.  */
  if (!(TREE_CODE (decl) == TYPE_DECL
        && TREE_CODE (TREE_TYPE (decl)) == UNCONSTRAINED_ARRAY_TYPE))
    {
      if (global_bindings_p ())
	{
	  VEC_safe_push (tree, gc, global_decls, decl);

	  if (TREE_CODE (decl) == FUNCTION_DECL && DECL_BUILT_IN (decl))
	    VEC_safe_push (tree, gc, builtin_decls, decl);
	}
      else if (!DECL_EXTERNAL (decl))
	{
	  DECL_CHAIN (decl) = BLOCK_VARS (current_binding_level->block);
	  BLOCK_VARS (current_binding_level->block) = decl;
	}
    }

  /* For the declaration of a type, set its name if it either is not already
     set or if the previous type name was not derived from a source name.
     We'd rather have the type named with a real name and all the pointer
     types to the same object have the same POINTER_TYPE node.  Code in the
     equivalent function of c-decl.c makes a copy of the type node here, but
     that may cause us trouble with incomplete types.  We make an exception
     for fat pointer types because the compiler automatically builds them
     for unconstrained array types and the debugger uses them to represent
     both these and pointers to these.  */
  if (TREE_CODE (decl) == TYPE_DECL && DECL_NAME (decl))
    {
      tree t = TREE_TYPE (decl);

      if (!(TYPE_NAME (t) && TREE_CODE (TYPE_NAME (t)) == TYPE_DECL))
	;
      else if (TYPE_IS_FAT_POINTER_P (t))
	{
	  tree tt = build_variant_type_copy (t);
	  TYPE_NAME (tt) = decl;
	  TREE_USED (tt) = TREE_USED (t);
	  TREE_TYPE (decl) = tt;
	  if (DECL_ORIGINAL_TYPE (TYPE_NAME (t)))
	    DECL_ORIGINAL_TYPE (decl) = DECL_ORIGINAL_TYPE (TYPE_NAME (t));
	  else
	    DECL_ORIGINAL_TYPE (decl) = t;
	  t = NULL_TREE;
	  DECL_ARTIFICIAL (decl) = 0;
	}
      else if (DECL_ARTIFICIAL (TYPE_NAME (t)) && !DECL_ARTIFICIAL (decl))
	;
      else
	t = NULL_TREE;

      /* Propagate the name to all the variants.  This is needed for
	 the type qualifiers machinery to work properly.  */
      if (t)
	for (t = TYPE_MAIN_VARIANT (t); t; t = TYPE_NEXT_VARIANT (t))
	  TYPE_NAME (t) = decl;
    }
}

/* Record TYPE as a builtin type for Ada.  NAME is the name of the type.  */

void
record_builtin_type (const char *name, tree type)
{
  tree type_decl = build_decl (input_location,
			       TYPE_DECL, get_identifier (name), type);

  gnat_pushdecl (type_decl, Empty);

  if (debug_hooks->type_decl)
    debug_hooks->type_decl (type_decl, false);
}

/* Given a record type RECORD_TYPE and a list of FIELD_DECL nodes FIELD_LIST,
   finish constructing the record or union type.  If REP_LEVEL is zero, this
   record has no representation clause and so will be entirely laid out here.
   If REP_LEVEL is one, this record has a representation clause and has been
   laid out already; only set the sizes and alignment.  If REP_LEVEL is two,
   this record is derived from a parent record and thus inherits its layout;
   only make a pass on the fields to finalize them.  DEBUG_INFO_P is true if
   we need to write debug information about this type.  */

void
finish_record_type (tree record_type, tree field_list, int rep_level,
		    bool debug_info_p)
{
  enum tree_code code = TREE_CODE (record_type);
  tree name = TYPE_NAME (record_type);
  tree ada_size = bitsize_zero_node;
  tree size = bitsize_zero_node;
  bool had_size = TYPE_SIZE (record_type) != 0;
  bool had_size_unit = TYPE_SIZE_UNIT (record_type) != 0;
  bool had_align = TYPE_ALIGN (record_type) != 0;
  tree field;

  TYPE_FIELDS (record_type) = field_list;

  /* Always attach the TYPE_STUB_DECL for a record type.  It is required to
     generate debug info and have a parallel type.  */
  if (name && TREE_CODE (name) == TYPE_DECL)
    name = DECL_NAME (name);
  TYPE_STUB_DECL (record_type) = create_type_stub_decl (name, record_type);

  /* Globally initialize the record first.  If this is a rep'ed record,
     that just means some initializations; otherwise, layout the record.  */
  if (rep_level > 0)
    {
      TYPE_ALIGN (record_type) = MAX (BITS_PER_UNIT, TYPE_ALIGN (record_type));

      if (!had_size_unit)
	TYPE_SIZE_UNIT (record_type) = size_zero_node;

      if (!had_size)
	TYPE_SIZE (record_type) = bitsize_zero_node;

      /* For all-repped records with a size specified, lay the QUAL_UNION_TYPE
	 out just like a UNION_TYPE, since the size will be fixed.  */
      else if (code == QUAL_UNION_TYPE)
	code = UNION_TYPE;
    }
  else
    {
      /* Ensure there isn't a size already set.  There can be in an error
	 case where there is a rep clause but all fields have errors and
	 no longer have a position.  */
      TYPE_SIZE (record_type) = 0;
      layout_type (record_type);
    }

  /* At this point, the position and size of each field is known.  It was
     either set before entry by a rep clause, or by laying out the type above.

     We now run a pass over the fields (in reverse order for QUAL_UNION_TYPEs)
     to compute the Ada size; the GCC size and alignment (for rep'ed records
     that are not padding types); and the mode (for rep'ed records).  We also
     clear the DECL_BIT_FIELD indication for the cases we know have not been
     handled yet, and adjust DECL_NONADDRESSABLE_P accordingly.  */

  if (code == QUAL_UNION_TYPE)
    field_list = nreverse (field_list);

  for (field = field_list; field; field = DECL_CHAIN (field))
    {
      tree type = TREE_TYPE (field);
      tree pos = bit_position (field);
      tree this_size = DECL_SIZE (field);
      tree this_ada_size;

      if ((TREE_CODE (type) == RECORD_TYPE
	   || TREE_CODE (type) == UNION_TYPE
	   || TREE_CODE (type) == QUAL_UNION_TYPE)
	  && !TYPE_FAT_POINTER_P (type)
	  && !TYPE_CONTAINS_TEMPLATE_P (type)
	  && TYPE_ADA_SIZE (type))
	this_ada_size = TYPE_ADA_SIZE (type);
      else
	this_ada_size = this_size;

      /* Clear DECL_BIT_FIELD for the cases layout_decl does not handle.  */
      if (DECL_BIT_FIELD (field)
	  && operand_equal_p (this_size, TYPE_SIZE (type), 0))
	{
	  unsigned int align = TYPE_ALIGN (type);

	  /* In the general case, type alignment is required.  */
	  if (value_factor_p (pos, align))
	    {
	      /* The enclosing record type must be sufficiently aligned.
		 Otherwise, if no alignment was specified for it and it
		 has been laid out already, bump its alignment to the
		 desired one if this is compatible with its size.  */
	      if (TYPE_ALIGN (record_type) >= align)
		{
		  DECL_ALIGN (field) = MAX (DECL_ALIGN (field), align);
		  DECL_BIT_FIELD (field) = 0;
		}
	      else if (!had_align
		       && rep_level == 0
		       && value_factor_p (TYPE_SIZE (record_type), align))
		{
		  TYPE_ALIGN (record_type) = align;
		  DECL_ALIGN (field) = MAX (DECL_ALIGN (field), align);
		  DECL_BIT_FIELD (field) = 0;
		}
	    }

	  /* In the non-strict alignment case, only byte alignment is.  */
	  if (!STRICT_ALIGNMENT
	      && DECL_BIT_FIELD (field)
	      && value_factor_p (pos, BITS_PER_UNIT))
	    DECL_BIT_FIELD (field) = 0;
	}

      /* If we still have DECL_BIT_FIELD set at this point, we know that the
	 field is technically not addressable.  Except that it can actually
	 be addressed if it is BLKmode and happens to be properly aligned.  */
      if (DECL_BIT_FIELD (field)
	  && !(DECL_MODE (field) == BLKmode
	       && value_factor_p (pos, BITS_PER_UNIT)))
	DECL_NONADDRESSABLE_P (field) = 1;

      /* A type must be as aligned as its most aligned field that is not
	 a bit-field.  But this is already enforced by layout_type.  */
      if (rep_level > 0 && !DECL_BIT_FIELD (field))
	TYPE_ALIGN (record_type)
	  = MAX (TYPE_ALIGN (record_type), DECL_ALIGN (field));

      switch (code)
	{
	case UNION_TYPE:
	  ada_size = size_binop (MAX_EXPR, ada_size, this_ada_size);
	  size = size_binop (MAX_EXPR, size, this_size);
	  break;

	case QUAL_UNION_TYPE:
	  ada_size
	    = fold_build3 (COND_EXPR, bitsizetype, DECL_QUALIFIER (field),
			   this_ada_size, ada_size);
	  size = fold_build3 (COND_EXPR, bitsizetype, DECL_QUALIFIER (field),
			      this_size, size);
	  break;

	case RECORD_TYPE:
	  /* Since we know here that all fields are sorted in order of
	     increasing bit position, the size of the record is one
	     higher than the ending bit of the last field processed
	     unless we have a rep clause, since in that case we might
	     have a field outside a QUAL_UNION_TYPE that has a higher ending
	     position.  So use a MAX in that case.  Also, if this field is a
	     QUAL_UNION_TYPE, we need to take into account the previous size in
	     the case of empty variants.  */
	  ada_size
	    = merge_sizes (ada_size, pos, this_ada_size,
			   TREE_CODE (type) == QUAL_UNION_TYPE, rep_level > 0);
	  size
	    = merge_sizes (size, pos, this_size,
			   TREE_CODE (type) == QUAL_UNION_TYPE, rep_level > 0);
	  break;

	default:
	  gcc_unreachable ();
	}
    }

  if (code == QUAL_UNION_TYPE)
    nreverse (field_list);

  if (rep_level < 2)
    {
      /* If this is a padding record, we never want to make the size smaller
	 than what was specified in it, if any.  */
      if (TYPE_IS_PADDING_P (record_type) && TYPE_SIZE (record_type))
	size = TYPE_SIZE (record_type);

      /* Now set any of the values we've just computed that apply.  */
      if (!TYPE_FAT_POINTER_P (record_type)
	  && !TYPE_CONTAINS_TEMPLATE_P (record_type))
	SET_TYPE_ADA_SIZE (record_type, ada_size);

      if (rep_level > 0)
	{
	  tree size_unit = had_size_unit
			   ? TYPE_SIZE_UNIT (record_type)
			   : convert (sizetype,
				      size_binop (CEIL_DIV_EXPR, size,
						  bitsize_unit_node));
	  unsigned int align = TYPE_ALIGN (record_type);

	  TYPE_SIZE (record_type) = variable_size (round_up (size, align));
	  TYPE_SIZE_UNIT (record_type)
	    = variable_size (round_up (size_unit, align / BITS_PER_UNIT));

	  compute_record_mode (record_type);
	}
    }

  if (debug_info_p)
    rest_of_record_type_compilation (record_type);
}

/* Wrap up compilation of RECORD_TYPE, i.e. output all the debug information
   associated with it.  It need not be invoked directly in most cases since
   finish_record_type takes care of doing so, but this can be necessary if
   a parallel type is to be attached to the record type.  */

void
rest_of_record_type_compilation (tree record_type)
{
  tree field_list = TYPE_FIELDS (record_type);
  tree field;
  enum tree_code code = TREE_CODE (record_type);
  bool var_size = false;

  for (field = field_list; field; field = DECL_CHAIN (field))
    {
      /* We need to make an XVE/XVU record if any field has variable size,
	 whether or not the record does.  For example, if we have a union,
	 it may be that all fields, rounded up to the alignment, have the
	 same size, in which case we'll use that size.  But the debug
	 output routines (except Dwarf2) won't be able to output the fields,
	 so we need to make the special record.  */
      if (TREE_CODE (DECL_SIZE (field)) != INTEGER_CST
	  /* If a field has a non-constant qualifier, the record will have
	     variable size too.  */
	  || (code == QUAL_UNION_TYPE
	      && TREE_CODE (DECL_QUALIFIER (field)) != INTEGER_CST))
	{
	  var_size = true;
	  break;
	}
    }

  /* If this record is of variable size, rename it so that the
     debugger knows it is and make a new, parallel, record
     that tells the debugger how the record is laid out.  See
     exp_dbug.ads.  But don't do this for records that are padding
     since they confuse GDB.  */
  if (var_size && !TYPE_IS_PADDING_P (record_type))
    {
      tree new_record_type
	= make_node (TREE_CODE (record_type) == QUAL_UNION_TYPE
		     ? UNION_TYPE : TREE_CODE (record_type));
      tree orig_name = TYPE_NAME (record_type), new_name;
      tree last_pos = bitsize_zero_node;
      tree old_field, prev_old_field = NULL_TREE;

      if (TREE_CODE (orig_name) == TYPE_DECL)
	orig_name = DECL_NAME (orig_name);

      new_name
	= concat_name (orig_name, TREE_CODE (record_type) == QUAL_UNION_TYPE
				  ? "XVU" : "XVE");
      TYPE_NAME (new_record_type) = new_name;
      TYPE_ALIGN (new_record_type) = BIGGEST_ALIGNMENT;
      TYPE_STUB_DECL (new_record_type)
	= create_type_stub_decl (new_name, new_record_type);
      DECL_IGNORED_P (TYPE_STUB_DECL (new_record_type))
	= DECL_IGNORED_P (TYPE_STUB_DECL (record_type));
      TYPE_SIZE (new_record_type) = size_int (TYPE_ALIGN (record_type));
      TYPE_SIZE_UNIT (new_record_type)
	= size_int (TYPE_ALIGN (record_type) / BITS_PER_UNIT);

      add_parallel_type (TYPE_STUB_DECL (record_type), new_record_type);

      /* Now scan all the fields, replacing each field with a new
	 field corresponding to the new encoding.  */
      for (old_field = TYPE_FIELDS (record_type); old_field;
	   old_field = DECL_CHAIN (old_field))
	{
	  tree field_type = TREE_TYPE (old_field);
	  tree field_name = DECL_NAME (old_field);
	  tree new_field;
	  tree curpos = bit_position (old_field);
	  bool var = false;
	  unsigned int align = 0;
	  tree pos;

	  /* See how the position was modified from the last position.

	  There are two basic cases we support: a value was added
	  to the last position or the last position was rounded to
	  a boundary and they something was added.  Check for the
	  first case first.  If not, see if there is any evidence
	  of rounding.  If so, round the last position and try
	  again.

	  If this is a union, the position can be taken as zero. */

	  /* Some computations depend on the shape of the position expression,
	     so strip conversions to make sure it's exposed.  */
	  curpos = remove_conversions (curpos, true);

	  if (TREE_CODE (new_record_type) == UNION_TYPE)
	    pos = bitsize_zero_node, align = 0;
	  else
	    pos = compute_related_constant (curpos, last_pos);

	  if (!pos && TREE_CODE (curpos) == MULT_EXPR
	      && host_integerp (TREE_OPERAND (curpos, 1), 1))
	    {
	      tree offset = TREE_OPERAND (curpos, 0);
	      align = tree_low_cst (TREE_OPERAND (curpos, 1), 1);

	      /* An offset which is a bitwise AND with a negative power of 2
		 means an alignment corresponding to this power of 2.  Note
		 that, as sizetype is sign-extended but nonetheless unsigned,
		 we don't directly use tree_int_cst_sgn.  */
	      offset = remove_conversions (offset, true);
	      if (TREE_CODE (offset) == BIT_AND_EXPR
		  && host_integerp (TREE_OPERAND (offset, 1), 0)
		  && TREE_INT_CST_HIGH (TREE_OPERAND (offset, 1)) < 0)
		{
		  unsigned int pow
		    = - tree_low_cst (TREE_OPERAND (offset, 1), 0);
		  if (exact_log2 (pow) > 0)
		    align *= pow;
		}

	      pos = compute_related_constant (curpos,
					      round_up (last_pos, align));
	    }
	  else if (!pos && TREE_CODE (curpos) == PLUS_EXPR
		   && TREE_CODE (TREE_OPERAND (curpos, 1)) == INTEGER_CST
		   && TREE_CODE (TREE_OPERAND (curpos, 0)) == MULT_EXPR
		   && host_integerp (TREE_OPERAND
				     (TREE_OPERAND (curpos, 0), 1),
				     1))
	    {
	      align
		= tree_low_cst
		(TREE_OPERAND (TREE_OPERAND (curpos, 0), 1), 1);
	      pos = compute_related_constant (curpos,
					      round_up (last_pos, align));
	    }
	  else if (potential_alignment_gap (prev_old_field, old_field,
					    pos))
	    {
	      align = TYPE_ALIGN (field_type);
	      pos = compute_related_constant (curpos,
					      round_up (last_pos, align));
	    }

	  /* If we can't compute a position, set it to zero.

	  ??? We really should abort here, but it's too much work
	  to get this correct for all cases.  */

	  if (!pos)
	    pos = bitsize_zero_node;

	  /* See if this type is variable-sized and make a pointer type
	     and indicate the indirection if so.  Beware that the debug
	     back-end may adjust the position computed above according
	     to the alignment of the field type, i.e. the pointer type
	     in this case, if we don't preventively counter that.  */
	  if (TREE_CODE (DECL_SIZE (old_field)) != INTEGER_CST)
	    {
	      field_type = build_pointer_type (field_type);
	      if (align != 0 && TYPE_ALIGN (field_type) > align)
		{
		  field_type = copy_node (field_type);
		  TYPE_ALIGN (field_type) = align;
		}
	      var = true;
	    }

	  /* Make a new field name, if necessary.  */
	  if (var || align != 0)
	    {
	      char suffix[16];

	      if (align != 0)
		sprintf (suffix, "XV%c%u", var ? 'L' : 'A',
			 align / BITS_PER_UNIT);
	      else
		strcpy (suffix, "XVL");

	      field_name = concat_name (field_name, suffix);
	    }

	  new_field
	    = create_field_decl (field_name, field_type, new_record_type,
				 DECL_SIZE (old_field), pos, 0, 0);
	  DECL_CHAIN (new_field) = TYPE_FIELDS (new_record_type);
	  TYPE_FIELDS (new_record_type) = new_field;

	  /* If old_field is a QUAL_UNION_TYPE, take its size as being
	     zero.  The only time it's not the last field of the record
	     is when there are other components at fixed positions after
	     it (meaning there was a rep clause for every field) and we
	     want to be able to encode them.  */
	  last_pos = size_binop (PLUS_EXPR, bit_position (old_field),
				 (TREE_CODE (TREE_TYPE (old_field))
				  == QUAL_UNION_TYPE)
				 ? bitsize_zero_node
				 : DECL_SIZE (old_field));
	  prev_old_field = old_field;
	}

      TYPE_FIELDS (new_record_type)
	= nreverse (TYPE_FIELDS (new_record_type));

      rest_of_type_decl_compilation (TYPE_STUB_DECL (new_record_type));
    }

  rest_of_type_decl_compilation (TYPE_STUB_DECL (record_type));
}

/* Append PARALLEL_TYPE on the chain of parallel types for decl.  */

void
add_parallel_type (tree decl, tree parallel_type)
{
  tree d = decl;

  while (DECL_PARALLEL_TYPE (d))
    d = TYPE_STUB_DECL (DECL_PARALLEL_TYPE (d));

  SET_DECL_PARALLEL_TYPE (d, parallel_type);
}

/* Utility function of above to merge LAST_SIZE, the previous size of a record
   with FIRST_BIT and SIZE that describe a field.  SPECIAL is true if this
   represents a QUAL_UNION_TYPE in which case we must look for COND_EXPRs and
   replace a value of zero with the old size.  If HAS_REP is true, we take the
   MAX of the end position of this field with LAST_SIZE.  In all other cases,
   we use FIRST_BIT plus SIZE.  Return an expression for the size.  */

static tree
merge_sizes (tree last_size, tree first_bit, tree size, bool special,
	     bool has_rep)
{
  tree type = TREE_TYPE (last_size);
  tree new_size;

  if (!special || TREE_CODE (size) != COND_EXPR)
    {
      new_size = size_binop (PLUS_EXPR, first_bit, size);
      if (has_rep)
	new_size = size_binop (MAX_EXPR, last_size, new_size);
    }

  else
    new_size = fold_build3 (COND_EXPR, type, TREE_OPERAND (size, 0),
			    integer_zerop (TREE_OPERAND (size, 1))
			    ? last_size : merge_sizes (last_size, first_bit,
						       TREE_OPERAND (size, 1),
						       1, has_rep),
			    integer_zerop (TREE_OPERAND (size, 2))
			    ? last_size : merge_sizes (last_size, first_bit,
						       TREE_OPERAND (size, 2),
						       1, has_rep));

  /* We don't need any NON_VALUE_EXPRs and they can confuse us (especially
     when fed through substitute_in_expr) into thinking that a constant
     size is not constant.  */
  while (TREE_CODE (new_size) == NON_LVALUE_EXPR)
    new_size = TREE_OPERAND (new_size, 0);

  return new_size;
}

/* Utility function of above to see if OP0 and OP1, both of SIZETYPE, are
   related by the addition of a constant.  Return that constant if so.  */

static tree
compute_related_constant (tree op0, tree op1)
{
  tree op0_var, op1_var;
  tree op0_con = split_plus (op0, &op0_var);
  tree op1_con = split_plus (op1, &op1_var);
  tree result = size_binop (MINUS_EXPR, op0_con, op1_con);

  if (operand_equal_p (op0_var, op1_var, 0))
    return result;
  else if (operand_equal_p (op0, size_binop (PLUS_EXPR, op1_var, result), 0))
    return result;
  else
    return 0;
}

/* Utility function of above to split a tree OP which may be a sum, into a
   constant part, which is returned, and a variable part, which is stored
   in *PVAR.  *PVAR may be bitsize_zero_node.  All operations must be of
   bitsizetype.  */

static tree
split_plus (tree in, tree *pvar)
{
  /* Strip NOPS in order to ease the tree traversal and maximize the
     potential for constant or plus/minus discovery. We need to be careful
     to always return and set *pvar to bitsizetype trees, but it's worth
     the effort.  */
  STRIP_NOPS (in);

  *pvar = convert (bitsizetype, in);

  if (TREE_CODE (in) == INTEGER_CST)
    {
      *pvar = bitsize_zero_node;
      return convert (bitsizetype, in);
    }
  else if (TREE_CODE (in) == PLUS_EXPR || TREE_CODE (in) == MINUS_EXPR)
    {
      tree lhs_var, rhs_var;
      tree lhs_con = split_plus (TREE_OPERAND (in, 0), &lhs_var);
      tree rhs_con = split_plus (TREE_OPERAND (in, 1), &rhs_var);

      if (lhs_var == TREE_OPERAND (in, 0)
	  && rhs_var == TREE_OPERAND (in, 1))
	return bitsize_zero_node;

      *pvar = size_binop (TREE_CODE (in), lhs_var, rhs_var);
      return size_binop (TREE_CODE (in), lhs_con, rhs_con);
    }
  else
    return bitsize_zero_node;
}

/* Return a FUNCTION_TYPE node.  RETURN_TYPE is the type returned by the
   subprogram.  If it is VOID_TYPE, then we are dealing with a procedure,
   otherwise we are dealing with a function.  PARAM_DECL_LIST is a list of
   PARM_DECL nodes that are the subprogram parameters.  CICO_LIST is the
   copy-in/copy-out list to be stored into the TYPE_CICO_LIST field.
   RETURN_UNCONSTRAINED_P is true if the function returns an unconstrained
   object.  RETURN_BY_DIRECT_REF_P is true if the function returns by direct
   reference.  RETURN_BY_INVISI_REF_P is true if the function returns by
   invisible reference.  */

tree
create_subprog_type (tree return_type, tree param_decl_list, tree cico_list,
		     bool return_unconstrained_p, bool return_by_direct_ref_p,
		     bool return_by_invisi_ref_p)
{
  /* A chain of TREE_LIST nodes whose TREE_VALUEs are the data type nodes of
     the subprogram formal parameters.  This list is generated by traversing
     the input list of PARM_DECL nodes.  */
  tree param_type_list = NULL_TREE;
  tree t, type;

  for (t = param_decl_list; t; t = DECL_CHAIN (t))
    param_type_list = tree_cons (NULL_TREE, TREE_TYPE (t), param_type_list);

  /* The list of the function parameter types has to be terminated by the void
     type to signal to the back-end that we are not dealing with a variable
     parameter subprogram, but that it has a fixed number of parameters.  */
  param_type_list = tree_cons (NULL_TREE, void_type_node, param_type_list);

  /* The list of argument types has been created in reverse so reverse it.  */
  param_type_list = nreverse (param_type_list);

  type = build_function_type (return_type, param_type_list);

  /* TYPE may have been shared since GCC hashes types.  If it has a different
     CICO_LIST, make a copy.  Likewise for the various flags.  */
  if (!fntype_same_flags_p (type, cico_list, return_unconstrained_p,
			    return_by_direct_ref_p, return_by_invisi_ref_p))
    {
      type = copy_type (type);
      TYPE_CI_CO_LIST (type) = cico_list;
      TYPE_RETURN_UNCONSTRAINED_P (type) = return_unconstrained_p;
      TYPE_RETURN_BY_DIRECT_REF_P (type) = return_by_direct_ref_p;
      TREE_ADDRESSABLE (type) = return_by_invisi_ref_p;
    }

  return type;
}

/* Return a copy of TYPE but safe to modify in any way.  */

tree
copy_type (tree type)
{
  tree new_type = copy_node (type);

  /* Unshare the language-specific data.  */
  if (TYPE_LANG_SPECIFIC (type))
    {
      TYPE_LANG_SPECIFIC (new_type) = NULL;
      SET_TYPE_LANG_SPECIFIC (new_type, GET_TYPE_LANG_SPECIFIC (type));
    }

  /* And the contents of the language-specific slot if needed.  */
  if ((INTEGRAL_TYPE_P (type) || TREE_CODE (type) == REAL_TYPE)
      && TYPE_RM_VALUES (type))
    {
      TYPE_RM_VALUES (new_type) = NULL_TREE;
      SET_TYPE_RM_SIZE (new_type, TYPE_RM_SIZE (type));
      SET_TYPE_RM_MIN_VALUE (new_type, TYPE_RM_MIN_VALUE (type));
      SET_TYPE_RM_MAX_VALUE (new_type, TYPE_RM_MAX_VALUE (type));
    }

  /* copy_node clears this field instead of copying it, because it is
     aliased with TREE_CHAIN.  */
  TYPE_STUB_DECL (new_type) = TYPE_STUB_DECL (type);

  TYPE_POINTER_TO (new_type) = 0;
  TYPE_REFERENCE_TO (new_type) = 0;
  TYPE_MAIN_VARIANT (new_type) = new_type;
  TYPE_NEXT_VARIANT (new_type) = 0;

  return new_type;
}

/* Return a subtype of sizetype with range MIN to MAX and whose
   TYPE_INDEX_TYPE is INDEX.  GNAT_NODE is used for the position
   of the associated TYPE_DECL.  */

tree
create_index_type (tree min, tree max, tree index, Node_Id gnat_node)
{
  /* First build a type for the desired range.  */
  tree type = build_nonshared_range_type (sizetype, min, max);

  /* Then set the index type.  */
  SET_TYPE_INDEX_TYPE (type, index);
  create_type_decl (NULL_TREE, type, NULL, true, false, gnat_node);

  return type;
}

/* Return a subtype of TYPE with range MIN to MAX.  If TYPE is NULL,
   sizetype is used.  */

tree
create_range_type (tree type, tree min, tree max)
{
  tree range_type;

  if (type == NULL_TREE)
    type = sizetype;

  /* First build a type with the base range.  */
  range_type = build_nonshared_range_type (type, TYPE_MIN_VALUE (type),
						 TYPE_MAX_VALUE (type));

  /* Then set the actual range.  */
  SET_TYPE_RM_MIN_VALUE (range_type, convert (type, min));
  SET_TYPE_RM_MAX_VALUE (range_type, convert (type, max));

  return range_type;
}

/* Return a TYPE_DECL node suitable for the TYPE_STUB_DECL field of a type.
   TYPE_NAME gives the name of the type and TYPE is a ..._TYPE node giving
   its data type.  */

tree
create_type_stub_decl (tree type_name, tree type)
{
  /* Using a named TYPE_DECL ensures that a type name marker is emitted in
     STABS while setting DECL_ARTIFICIAL ensures that no DW_TAG_typedef is
     emitted in DWARF.  */
  tree type_decl = build_decl (input_location,
			       TYPE_DECL, type_name, type);
  DECL_ARTIFICIAL (type_decl) = 1;
  return type_decl;
}

/* Return a TYPE_DECL node.  TYPE_NAME gives the name of the type and TYPE
   is a ..._TYPE node giving its data type.  ARTIFICIAL_P is true if this
   is a declaration that was generated by the compiler.  DEBUG_INFO_P is
   true if we need to write debug information about this type.  GNAT_NODE
   is used for the position of the decl.  */

tree
create_type_decl (tree type_name, tree type, struct attrib *attr_list,
		  bool artificial_p, bool debug_info_p, Node_Id gnat_node)
{
  enum tree_code code = TREE_CODE (type);
  bool named = TYPE_NAME (type) && TREE_CODE (TYPE_NAME (type)) == TYPE_DECL;
  tree type_decl;

  /* Only the builtin TYPE_STUB_DECL should be used for dummy types.  */
  gcc_assert (!TYPE_IS_DUMMY_P (type));

  /* If the type hasn't been named yet, we're naming it; preserve an existing
     TYPE_STUB_DECL that has been attached to it for some purpose.  */
  if (!named && TYPE_STUB_DECL (type))
    {
      type_decl = TYPE_STUB_DECL (type);
      DECL_NAME (type_decl) = type_name;
    }
  else
    type_decl = build_decl (input_location,
			    TYPE_DECL, type_name, type);

  DECL_ARTIFICIAL (type_decl) = artificial_p;

  /* Add this decl to the current binding level.  */
  gnat_pushdecl (type_decl, gnat_node);

  process_attributes (type_decl, attr_list);

  /* If we're naming the type, equate the TYPE_STUB_DECL to the name.
     This causes the name to be also viewed as a "tag" by the debug
     back-end, with the advantage that no DW_TAG_typedef is emitted
     for artificial "tagged" types in DWARF.  */
  if (!named)
    TYPE_STUB_DECL (type) = type_decl;

  /* Pass the type declaration to the debug back-end unless this is an
     UNCONSTRAINED_ARRAY_TYPE that the back-end does not support, or a
     type for which debugging information was not requested, or else an
     ENUMERAL_TYPE or RECORD_TYPE (except for fat pointers) which are
     handled separately.  And do not pass dummy types either.  */
  if (code == UNCONSTRAINED_ARRAY_TYPE || !debug_info_p)
    DECL_IGNORED_P (type_decl) = 1;
  else if (code != ENUMERAL_TYPE
	   && (code != RECORD_TYPE || TYPE_FAT_POINTER_P (type))
	   && !((code == POINTER_TYPE || code == REFERENCE_TYPE)
		&& TYPE_IS_DUMMY_P (TREE_TYPE (type)))
	   && !(code == RECORD_TYPE
		&& TYPE_IS_DUMMY_P
		   (TREE_TYPE (TREE_TYPE (TYPE_FIELDS (type))))))
    rest_of_type_decl_compilation (type_decl);

  return type_decl;
}

/* Return a VAR_DECL or CONST_DECL node.

   VAR_NAME gives the name of the variable.  ASM_NAME is its assembler name
   (if provided).  TYPE is its data type (a GCC ..._TYPE node).  VAR_INIT is
   the GCC tree for an optional initial expression; NULL_TREE if none.

   CONST_FLAG is true if this variable is constant, in which case we might
   return a CONST_DECL node unless CONST_DECL_ALLOWED_P is false.

   PUBLIC_FLAG is true if this is for a reference to a public entity or for a
   definition to be made visible outside of the current compilation unit, for
   instance variable definitions in a package specification.

   EXTERN_FLAG is true when processing an external variable declaration (as
   opposed to a definition: no storage is to be allocated for the variable).

   STATIC_FLAG is only relevant when not at top level.  In that case
   it indicates whether to always allocate storage to the variable.

   GNAT_NODE is used for the position of the decl.  */

tree
create_var_decl_1 (tree var_name, tree asm_name, tree type, tree var_init,
		   bool const_flag, bool public_flag, bool extern_flag,
		   bool static_flag, bool const_decl_allowed_p,
		   struct attrib *attr_list, Node_Id gnat_node)
{
  bool init_const
    = (var_init != 0
       && gnat_types_compatible_p (type, TREE_TYPE (var_init))
       && (global_bindings_p () || static_flag
	   ? initializer_constant_valid_p (var_init, TREE_TYPE (var_init)) != 0
	   : TREE_CONSTANT (var_init)));

  /* Whether we will make TREE_CONSTANT the DECL we produce here, in which
     case the initializer may be used in-lieu of the DECL node (as done in
     Identifier_to_gnu).  This is useful to prevent the need of elaboration
     code when an identifier for which such a decl is made is in turn used as
     an initializer.  We used to rely on CONST vs VAR_DECL for this purpose,
     but extra constraints apply to this choice (see below) and are not
     relevant to the distinction we wish to make. */
  bool constant_p = const_flag && init_const;

  /* The actual DECL node.  CONST_DECL was initially intended for enumerals
     and may be used for scalars in general but not for aggregates.  */
  tree var_decl
    = build_decl (input_location,
		  (constant_p && const_decl_allowed_p
		   && !AGGREGATE_TYPE_P (type)) ? CONST_DECL : VAR_DECL,
		  var_name, type);

  /* If this is external, throw away any initializations (they will be done
     elsewhere) unless this is a constant for which we would like to remain
     able to get the initializer.  If we are defining a global here, leave a
     constant initialization and save any variable elaborations for the
     elaboration routine.  If we are just annotating types, throw away the
     initialization if it isn't a constant.  */
  if ((extern_flag && !constant_p)
      || (type_annotate_only && var_init && !TREE_CONSTANT (var_init)))
    var_init = NULL_TREE;

  /* At the global level, an initializer requiring code to be generated
     produces elaboration statements.  Check that such statements are allowed,
     that is, not violating a No_Elaboration_Code restriction.  */
  if (global_bindings_p () && var_init != 0 && !init_const)
    Check_Elaboration_Code_Allowed (gnat_node);

  DECL_INITIAL  (var_decl) = var_init;
  TREE_READONLY (var_decl) = const_flag;
  DECL_EXTERNAL (var_decl) = extern_flag;
  TREE_PUBLIC   (var_decl) = public_flag || extern_flag;
  TREE_CONSTANT (var_decl) = constant_p;
  TREE_THIS_VOLATILE (var_decl) = TREE_SIDE_EFFECTS (var_decl)
    = TYPE_VOLATILE (type);

  /* Ada doesn't feature Fortran-like COMMON variables so we shouldn't
     try to fiddle with DECL_COMMON.  However, on platforms that don't
     support global BSS sections, uninitialized global variables would
     go in DATA instead, thus increasing the size of the executable.  */
  if (!flag_no_common
      && TREE_CODE (var_decl) == VAR_DECL
      && TREE_PUBLIC (var_decl)
      && !have_global_bss_p ())
    DECL_COMMON (var_decl) = 1;

  /* At the global binding level, we need to allocate static storage for the
     variable if it isn't external.  Otherwise, we allocate automatic storage
     unless requested not to.  */
  TREE_STATIC (var_decl)
    = !extern_flag && (static_flag || global_bindings_p ());

  /* For an external constant whose initializer is not absolute, do not emit
     debug info.  In DWARF this would mean a global relocation in a read-only
     section which runs afoul of the PE-COFF run-time relocation mechanism.  */
  if (extern_flag
      && constant_p
      && initializer_constant_valid_p (var_init, TREE_TYPE (var_init))
	   != null_pointer_node)
    DECL_IGNORED_P (var_decl) = 1;

  /* Add this decl to the current binding level.  */
  gnat_pushdecl (var_decl, gnat_node);

  if (TREE_SIDE_EFFECTS (var_decl))
    TREE_ADDRESSABLE (var_decl) = 1;

  if (TREE_CODE (var_decl) == VAR_DECL)
    {
      if (asm_name)
	SET_DECL_ASSEMBLER_NAME (var_decl, asm_name);
      process_attributes (var_decl, attr_list);
      if (global_bindings_p ())
	rest_of_decl_compilation (var_decl, true, 0);
    }
  else
    expand_decl (var_decl);

  return var_decl;
}

/* Return true if TYPE, an aggregate type, contains (or is) an array.  */

static bool
aggregate_type_contains_array_p (tree type)
{
  switch (TREE_CODE (type))
    {
    case RECORD_TYPE:
    case UNION_TYPE:
    case QUAL_UNION_TYPE:
      {
	tree field;
	for (field = TYPE_FIELDS (type); field; field = DECL_CHAIN (field))
	  if (AGGREGATE_TYPE_P (TREE_TYPE (field))
	      && aggregate_type_contains_array_p (TREE_TYPE (field)))
	    return true;
	return false;
      }

    case ARRAY_TYPE:
      return true;

    default:
      gcc_unreachable ();
    }
}

/* Return a FIELD_DECL node.  FIELD_NAME is the field's name, FIELD_TYPE is
   its type and RECORD_TYPE is the type of the enclosing record.  If SIZE is
   nonzero, it is the specified size of the field.  If POS is nonzero, it is
   the bit position.  PACKED is 1 if the enclosing record is packed, -1 if it
   has Component_Alignment of Storage_Unit.  If ADDRESSABLE is nonzero, it
   means we are allowed to take the address of the field; if it is negative,
   we should not make a bitfield, which is used by make_aligning_type.  */

tree
create_field_decl (tree field_name, tree field_type, tree record_type,
                   tree size, tree pos, int packed, int addressable)
{
  tree field_decl = build_decl (input_location,
				FIELD_DECL, field_name, field_type);

  DECL_CONTEXT (field_decl) = record_type;
  TREE_READONLY (field_decl) = TYPE_READONLY (field_type);

  /* If FIELD_TYPE is BLKmode, we must ensure this is aligned to at least a
     byte boundary since GCC cannot handle less-aligned BLKmode bitfields.
     Likewise for an aggregate without specified position that contains an
     array, because in this case slices of variable length of this array
     must be handled by GCC and variable-sized objects need to be aligned
     to at least a byte boundary.  */
  if (packed && (TYPE_MODE (field_type) == BLKmode
		 || (!pos
		     && AGGREGATE_TYPE_P (field_type)
		     && aggregate_type_contains_array_p (field_type))))
    DECL_ALIGN (field_decl) = BITS_PER_UNIT;

  /* If a size is specified, use it.  Otherwise, if the record type is packed
     compute a size to use, which may differ from the object's natural size.
     We always set a size in this case to trigger the checks for bitfield
     creation below, which is typically required when no position has been
     specified.  */
  if (size)
    size = convert (bitsizetype, size);
  else if (packed == 1)
    {
      size = rm_size (field_type);
      if (TYPE_MODE (field_type) == BLKmode)
	size = round_up (size, BITS_PER_UNIT);
    }

  /* If we may, according to ADDRESSABLE, make a bitfield if a size is
     specified for two reasons: first if the size differs from the natural
     size.  Second, if the alignment is insufficient.  There are a number of
     ways the latter can be true.

     We never make a bitfield if the type of the field has a nonconstant size,
     because no such entity requiring bitfield operations should reach here.

     We do *preventively* make a bitfield when there might be the need for it
     but we don't have all the necessary information to decide, as is the case
     of a field with no specified position in a packed record.

     We also don't look at STRICT_ALIGNMENT here, and rely on later processing
     in layout_decl or finish_record_type to clear the bit_field indication if
     it is in fact not needed.  */
  if (addressable >= 0
      && size
      && TREE_CODE (size) == INTEGER_CST
      && TREE_CODE (TYPE_SIZE (field_type)) == INTEGER_CST
      && (!tree_int_cst_equal (size, TYPE_SIZE (field_type))
	  || (pos && !value_factor_p (pos, TYPE_ALIGN (field_type)))
	  || packed
	  || (TYPE_ALIGN (record_type) != 0
	      && TYPE_ALIGN (record_type) < TYPE_ALIGN (field_type))))
    {
      DECL_BIT_FIELD (field_decl) = 1;
      DECL_SIZE (field_decl) = size;
      if (!packed && !pos)
	{
	  if (TYPE_ALIGN (record_type) != 0
	      && TYPE_ALIGN (record_type) < TYPE_ALIGN (field_type))
	    DECL_ALIGN (field_decl) = TYPE_ALIGN (record_type);
	  else
	    DECL_ALIGN (field_decl) = TYPE_ALIGN (field_type);
	}
    }

  DECL_PACKED (field_decl) = pos ? DECL_BIT_FIELD (field_decl) : packed;

  /* Bump the alignment if need be, either for bitfield/packing purposes or
     to satisfy the type requirements if no such consideration applies.  When
     we get the alignment from the type, indicate if this is from an explicit
     user request, which prevents stor-layout from lowering it later on.  */
  {
    unsigned int bit_align
      = (DECL_BIT_FIELD (field_decl) ? 1
	 : packed && TYPE_MODE (field_type) != BLKmode ? BITS_PER_UNIT : 0);

    if (bit_align > DECL_ALIGN (field_decl))
      DECL_ALIGN (field_decl) = bit_align;
    else if (!bit_align && TYPE_ALIGN (field_type) > DECL_ALIGN (field_decl))
      {
	DECL_ALIGN (field_decl) = TYPE_ALIGN (field_type);
	DECL_USER_ALIGN (field_decl) = TYPE_USER_ALIGN (field_type);
      }
  }

  if (pos)
    {
      /* We need to pass in the alignment the DECL is known to have.
	 This is the lowest-order bit set in POS, but no more than
	 the alignment of the record, if one is specified.  Note
	 that an alignment of 0 is taken as infinite.  */
      unsigned int known_align;

      if (host_integerp (pos, 1))
	known_align = tree_low_cst (pos, 1) & - tree_low_cst (pos, 1);
      else
	known_align = BITS_PER_UNIT;

      if (TYPE_ALIGN (record_type)
	  && (known_align == 0 || known_align > TYPE_ALIGN (record_type)))
	known_align = TYPE_ALIGN (record_type);

      layout_decl (field_decl, known_align);
      SET_DECL_OFFSET_ALIGN (field_decl,
			     host_integerp (pos, 1) ? BIGGEST_ALIGNMENT
			     : BITS_PER_UNIT);
      pos_from_bit (&DECL_FIELD_OFFSET (field_decl),
		    &DECL_FIELD_BIT_OFFSET (field_decl),
		    DECL_OFFSET_ALIGN (field_decl), pos);
    }

  /* In addition to what our caller says, claim the field is addressable if we
     know that its type is not suitable.

     The field may also be "technically" nonaddressable, meaning that even if
     we attempt to take the field's address we will actually get the address
     of a copy.  This is the case for true bitfields, but the DECL_BIT_FIELD
     value we have at this point is not accurate enough, so we don't account
     for this here and let finish_record_type decide.  */
  if (!addressable && !type_for_nonaliased_component_p (field_type))
    addressable = 1;

  DECL_NONADDRESSABLE_P (field_decl) = !addressable;

  return field_decl;
}

/* Return a PARM_DECL node.  PARAM_NAME is the name of the parameter and
   PARAM_TYPE is its type.  READONLY is true if the parameter is readonly
   (either an In parameter or an address of a pass-by-ref parameter).  */

tree
create_param_decl (tree param_name, tree param_type, bool readonly)
{
  tree param_decl = build_decl (input_location,
				PARM_DECL, param_name, param_type);

  /* Honor TARGET_PROMOTE_PROTOTYPES like the C compiler, as not doing so
     can lead to various ABI violations.  */
  if (targetm.calls.promote_prototypes (NULL_TREE)
      && INTEGRAL_TYPE_P (param_type)
      && TYPE_PRECISION (param_type) < TYPE_PRECISION (integer_type_node))
    {
      /* We have to be careful about biased types here.  Make a subtype
	 of integer_type_node with the proper biasing.  */
      if (TREE_CODE (param_type) == INTEGER_TYPE
	  && TYPE_BIASED_REPRESENTATION_P (param_type))
	{
	  tree subtype
	    = make_unsigned_type (TYPE_PRECISION (integer_type_node));
	  TREE_TYPE (subtype) = integer_type_node;
	  TYPE_BIASED_REPRESENTATION_P (subtype) = 1;
	  SET_TYPE_RM_MIN_VALUE (subtype, TYPE_MIN_VALUE (param_type));
	  SET_TYPE_RM_MAX_VALUE (subtype, TYPE_MAX_VALUE (param_type));
	  param_type = subtype;
	}
      else
	param_type = integer_type_node;
    }

  DECL_ARG_TYPE (param_decl) = param_type;
  TREE_READONLY (param_decl) = readonly;
  return param_decl;
}

/* Given a DECL and ATTR_LIST, process the listed attributes.  */

static void
process_attributes (tree decl, struct attrib *attr_list)
{
  for (; attr_list; attr_list = attr_list->next)
    switch (attr_list->type)
      {
      case ATTR_MACHINE_ATTRIBUTE:
	input_location = DECL_SOURCE_LOCATION (decl);
	decl_attributes (&decl, tree_cons (attr_list->name, attr_list->args,
					   NULL_TREE),
			 ATTR_FLAG_TYPE_IN_PLACE);
	break;

      case ATTR_LINK_ALIAS:
        if (! DECL_EXTERNAL (decl))
	  {
	    TREE_STATIC (decl) = 1;
	    assemble_alias (decl, attr_list->name);
	  }
	break;

      case ATTR_WEAK_EXTERNAL:
	if (SUPPORTS_WEAK)
	  declare_weak (decl);
	else
	  post_error ("?weak declarations not supported on this target",
		      attr_list->error_point);
	break;

      case ATTR_LINK_SECTION:
	if (targetm.have_named_sections)
	  {
	    DECL_SECTION_NAME (decl)
	      = build_string (IDENTIFIER_LENGTH (attr_list->name),
			      IDENTIFIER_POINTER (attr_list->name));
	    DECL_COMMON (decl) = 0;
	  }
	else
	  post_error ("?section attributes are not supported for this target",
		      attr_list->error_point);
	break;

      case ATTR_LINK_CONSTRUCTOR:
	DECL_STATIC_CONSTRUCTOR (decl) = 1;
	TREE_USED (decl) = 1;
	break;

      case ATTR_LINK_DESTRUCTOR:
	DECL_STATIC_DESTRUCTOR (decl) = 1;
	TREE_USED (decl) = 1;
	break;

      case ATTR_THREAD_LOCAL_STORAGE:
	DECL_TLS_MODEL (decl) = decl_default_tls_model (decl);
	DECL_COMMON (decl) = 0;
	break;
      }
}

/* Record DECL as a global renaming pointer.  */

void
record_global_renaming_pointer (tree decl)
{
  gcc_assert (DECL_RENAMED_OBJECT (decl));
  VEC_safe_push (tree, gc, global_renaming_pointers, decl);
}

/* Invalidate the global renaming pointers.   */

void
invalidate_global_renaming_pointers (void)
{
  unsigned int i;
  tree iter;

  FOR_EACH_VEC_ELT (tree, global_renaming_pointers, i, iter)
    SET_DECL_RENAMED_OBJECT (iter, NULL_TREE);

  VEC_free (tree, gc, global_renaming_pointers);
}

/* Return true if VALUE is a known to be a multiple of FACTOR, which must be
   a power of 2. */

bool
value_factor_p (tree value, HOST_WIDE_INT factor)
{
  if (host_integerp (value, 1))
    return tree_low_cst (value, 1) % factor == 0;

  if (TREE_CODE (value) == MULT_EXPR)
    return (value_factor_p (TREE_OPERAND (value, 0), factor)
            || value_factor_p (TREE_OPERAND (value, 1), factor));

  return false;
}

/* Given 2 consecutive field decls PREV_FIELD and CURR_FIELD, return true
   unless we can prove these 2 fields are laid out in such a way that no gap
   exist between the end of PREV_FIELD and the beginning of CURR_FIELD.  OFFSET
   is the distance in bits between the end of PREV_FIELD and the starting
   position of CURR_FIELD. It is ignored if null. */

static bool
potential_alignment_gap (tree prev_field, tree curr_field, tree offset)
{
  /* If this is the first field of the record, there cannot be any gap */
  if (!prev_field)
    return false;

  /* If the previous field is a union type, then return False: The only
     time when such a field is not the last field of the record is when
     there are other components at fixed positions after it (meaning there
     was a rep clause for every field), in which case we don't want the
     alignment constraint to override them. */
  if (TREE_CODE (TREE_TYPE (prev_field)) == QUAL_UNION_TYPE)
    return false;

  /* If the distance between the end of prev_field and the beginning of
     curr_field is constant, then there is a gap if the value of this
     constant is not null. */
  if (offset && host_integerp (offset, 1))
    return !integer_zerop (offset);

  /* If the size and position of the previous field are constant,
     then check the sum of this size and position. There will be a gap
     iff it is not multiple of the current field alignment. */
  if (host_integerp (DECL_SIZE (prev_field), 1)
      && host_integerp (bit_position (prev_field), 1))
    return ((tree_low_cst (bit_position (prev_field), 1)
	     + tree_low_cst (DECL_SIZE (prev_field), 1))
	    % DECL_ALIGN (curr_field) != 0);

  /* If both the position and size of the previous field are multiples
     of the current field alignment, there cannot be any gap. */
  if (value_factor_p (bit_position (prev_field), DECL_ALIGN (curr_field))
      && value_factor_p (DECL_SIZE (prev_field), DECL_ALIGN (curr_field)))
    return false;

  /* Fallback, return that there may be a potential gap */
  return true;
}

/* Returns a LABEL_DECL node for LABEL_NAME.  */

tree
create_label_decl (tree label_name)
{
  tree label_decl = build_decl (input_location,
				LABEL_DECL, label_name, void_type_node);

  DECL_CONTEXT (label_decl)     = current_function_decl;
  DECL_MODE (label_decl)        = VOIDmode;
  DECL_SOURCE_LOCATION (label_decl) = input_location;

  return label_decl;
}

/* Returns a FUNCTION_DECL node.  SUBPROG_NAME is the name of the subprogram,
   ASM_NAME is its assembler name, SUBPROG_TYPE is its type (a FUNCTION_TYPE
   node), PARAM_DECL_LIST is the list of the subprogram arguments (a list of
   PARM_DECL nodes chained through the TREE_CHAIN field).

   INLINE_FLAG, PUBLIC_FLAG, EXTERN_FLAG, and ATTR_LIST are used to set the
   appropriate fields in the FUNCTION_DECL.  GNAT_NODE gives the location.  */

tree
create_subprog_decl (tree subprog_name, tree asm_name,
                     tree subprog_type, tree param_decl_list, bool inline_flag,
		     bool public_flag, bool extern_flag,
                     struct attrib *attr_list, Node_Id gnat_node)
{
  tree subprog_decl = build_decl (input_location, FUNCTION_DECL, subprog_name,
				  subprog_type);
  tree result_decl = build_decl (input_location, RESULT_DECL, NULL_TREE,
				 TREE_TYPE (subprog_type));

  /* If this is a non-inline function nested inside an inlined external
     function, we cannot honor both requests without cloning the nested
     function in the current unit since it is private to the other unit.
     We could inline the nested function as well but it's probably better
     to err on the side of too little inlining.  */
  if (!inline_flag
      && current_function_decl
      && DECL_DECLARED_INLINE_P (current_function_decl)
      && DECL_EXTERNAL (current_function_decl))
    DECL_DECLARED_INLINE_P (current_function_decl) = 0;

  DECL_EXTERNAL (subprog_decl)  = extern_flag;
  TREE_PUBLIC (subprog_decl)    = public_flag;
  TREE_READONLY (subprog_decl)  = TYPE_READONLY (subprog_type);
  TREE_THIS_VOLATILE (subprog_decl) = TYPE_VOLATILE (subprog_type);
  TREE_SIDE_EFFECTS (subprog_decl) = TYPE_VOLATILE (subprog_type);
  DECL_DECLARED_INLINE_P (subprog_decl) = inline_flag;
  DECL_ARGUMENTS (subprog_decl) = param_decl_list;

  DECL_ARTIFICIAL (result_decl) = 1;
  DECL_IGNORED_P (result_decl) = 1;
  DECL_BY_REFERENCE (result_decl) = TREE_ADDRESSABLE (subprog_type);
  DECL_RESULT (subprog_decl) = result_decl;

  if (asm_name)
    {
      SET_DECL_ASSEMBLER_NAME (subprog_decl, asm_name);

      /* The expand_main_function circuitry expects "main_identifier_node" to
	 designate the DECL_NAME of the 'main' entry point, in turn expected
	 to be declared as the "main" function literally by default.  Ada
	 program entry points are typically declared with a different name
	 within the binder generated file, exported as 'main' to satisfy the
	 system expectations.  Force main_identifier_node in this case.  */
      if (asm_name == main_identifier_node)
	DECL_NAME (subprog_decl) = main_identifier_node;
    }

  /* Add this decl to the current binding level.  */
  gnat_pushdecl (subprog_decl, gnat_node);

  process_attributes (subprog_decl, attr_list);

  /* Output the assembler code and/or RTL for the declaration.  */
  rest_of_decl_compilation (subprog_decl, global_bindings_p (), 0);

  return subprog_decl;
}

/* Set up the framework for generating code for SUBPROG_DECL, a subprogram
   body.  This routine needs to be invoked before processing the declarations
   appearing in the subprogram.  */

void
begin_subprog_body (tree subprog_decl)
{
  tree param_decl;

  announce_function (subprog_decl);

  /* This function is being defined.  */
  TREE_STATIC (subprog_decl) = 1;

  current_function_decl = subprog_decl;

  /* Enter a new binding level and show that all the parameters belong to
     this function.  */
  gnat_pushlevel ();

  for (param_decl = DECL_ARGUMENTS (subprog_decl); param_decl;
       param_decl = DECL_CHAIN (param_decl))
    DECL_CONTEXT (param_decl) = subprog_decl;

  make_decl_rtl (subprog_decl);

  /* We handle pending sizes via the elaboration of types, so we don't need to
     save them.  This causes them to be marked as part of the outer function
     and then discarded.  */
  get_pending_sizes ();
}

/* Finish the definition of the current subprogram BODY and finalize it.  */

void
end_subprog_body (tree body)
{
  tree fndecl = current_function_decl;

  /* Attach the BLOCK for this level to the function and pop the level.  */
  BLOCK_SUPERCONTEXT (current_binding_level->block) = fndecl;
  DECL_INITIAL (fndecl) = current_binding_level->block;
  gnat_poplevel ();

  /* We handle pending sizes via the elaboration of types, so we don't
     need to save them.  */
  get_pending_sizes ();

  /* Mark the RESULT_DECL as being in this subprogram. */
  DECL_CONTEXT (DECL_RESULT (fndecl)) = fndecl;

  /* The body should be a BIND_EXPR whose BLOCK is the top-level one.  */
  if (TREE_CODE (body) == BIND_EXPR)
    {
      BLOCK_SUPERCONTEXT (BIND_EXPR_BLOCK (body)) = fndecl;
      DECL_INITIAL (fndecl) = BIND_EXPR_BLOCK (body);
    }

  DECL_SAVED_TREE (fndecl) = body;

  current_function_decl = DECL_CONTEXT (fndecl);

  /* We cannot track the location of errors past this point.  */
  error_gnat_node = Empty;

  /* If we're only annotating types, don't actually compile this function.  */
  if (type_annotate_only)
    return;

  /* Dump functions before gimplification.  */
  dump_function (TDI_original, fndecl);

  /* ??? This special handling of nested functions is probably obsolete.  */
  if (!DECL_CONTEXT (fndecl))
    cgraph_finalize_function (fndecl, false);
  else
    /* Register this function with cgraph just far enough to get it
       added to our parent's nested function list.  */
    (void) cgraph_node (fndecl);
}

tree
gnat_builtin_function (tree decl)
{
  gnat_pushdecl (decl, Empty);
  return decl;
}

/* Return an integer type with the number of bits of precision given by
   PRECISION.  UNSIGNEDP is nonzero if the type is unsigned; otherwise
   it is a signed type.  */

tree
gnat_type_for_size (unsigned precision, int unsignedp)
{
  tree t;
  char type_name[20];

  if (precision <= 2 * MAX_BITS_PER_WORD
      && signed_and_unsigned_types[precision][unsignedp])
    return signed_and_unsigned_types[precision][unsignedp];

 if (unsignedp)
    t = make_unsigned_type (precision);
  else
    t = make_signed_type (precision);

  if (precision <= 2 * MAX_BITS_PER_WORD)
    signed_and_unsigned_types[precision][unsignedp] = t;

  if (!TYPE_NAME (t))
    {
      sprintf (type_name, "%sSIGNED_%d", unsignedp ? "UN" : "", precision);
      TYPE_NAME (t) = get_identifier (type_name);
    }

  return t;
}

/* Likewise for floating-point types.  */

static tree
float_type_for_precision (int precision, enum machine_mode mode)
{
  tree t;
  char type_name[20];

  if (float_types[(int) mode])
    return float_types[(int) mode];

  float_types[(int) mode] = t = make_node (REAL_TYPE);
  TYPE_PRECISION (t) = precision;
  layout_type (t);

  gcc_assert (TYPE_MODE (t) == mode);
  if (!TYPE_NAME (t))
    {
      sprintf (type_name, "FLOAT_%d", precision);
      TYPE_NAME (t) = get_identifier (type_name);
    }

  return t;
}

/* Return a data type that has machine mode MODE.  UNSIGNEDP selects
   an unsigned type; otherwise a signed type is returned.  */

tree
gnat_type_for_mode (enum machine_mode mode, int unsignedp)
{
  if (mode == BLKmode)
    return NULL_TREE;

  if (mode == VOIDmode)
    return void_type_node;

  if (COMPLEX_MODE_P (mode))
    return NULL_TREE;

  if (SCALAR_FLOAT_MODE_P (mode))
    return float_type_for_precision (GET_MODE_PRECISION (mode), mode);

  if (SCALAR_INT_MODE_P (mode))
    return gnat_type_for_size (GET_MODE_BITSIZE (mode), unsignedp);

  if (VECTOR_MODE_P (mode))
    {
      enum machine_mode inner_mode = GET_MODE_INNER (mode);
      tree inner_type = gnat_type_for_mode (inner_mode, unsignedp);
      if (inner_type)
	return build_vector_type_for_mode (inner_type, mode);
    }

  return NULL_TREE;
}

/* Return the unsigned version of a TYPE_NODE, a scalar type.  */

tree
gnat_unsigned_type (tree type_node)
{
  tree type = gnat_type_for_size (TYPE_PRECISION (type_node), 1);

  if (TREE_CODE (type_node) == INTEGER_TYPE && TYPE_MODULAR_P (type_node))
    {
      type = copy_node (type);
      TREE_TYPE (type) = type_node;
    }
  else if (TREE_TYPE (type_node)
	   && TREE_CODE (TREE_TYPE (type_node)) == INTEGER_TYPE
	   && TYPE_MODULAR_P (TREE_TYPE (type_node)))
    {
      type = copy_node (type);
      TREE_TYPE (type) = TREE_TYPE (type_node);
    }

  return type;
}

/* Return the signed version of a TYPE_NODE, a scalar type.  */

tree
gnat_signed_type (tree type_node)
{
  tree type = gnat_type_for_size (TYPE_PRECISION (type_node), 0);

  if (TREE_CODE (type_node) == INTEGER_TYPE && TYPE_MODULAR_P (type_node))
    {
      type = copy_node (type);
      TREE_TYPE (type) = type_node;
    }
  else if (TREE_TYPE (type_node)
	   && TREE_CODE (TREE_TYPE (type_node)) == INTEGER_TYPE
	   && TYPE_MODULAR_P (TREE_TYPE (type_node)))
    {
      type = copy_node (type);
      TREE_TYPE (type) = TREE_TYPE (type_node);
    }

  return type;
}

/* Return 1 if the types T1 and T2 are compatible, i.e. if they can be
   transparently converted to each other.  */

int
gnat_types_compatible_p (tree t1, tree t2)
{
  enum tree_code code;

  /* This is the default criterion.  */
  if (TYPE_MAIN_VARIANT (t1) == TYPE_MAIN_VARIANT (t2))
    return 1;

  /* We only check structural equivalence here.  */
  if ((code = TREE_CODE (t1)) != TREE_CODE (t2))
    return 0;

  /* Vector types are also compatible if they have the same number of subparts
     and the same form of (scalar) element type.  */
  if (code == VECTOR_TYPE
      && TYPE_VECTOR_SUBPARTS (t1) == TYPE_VECTOR_SUBPARTS (t2)
      && TREE_CODE (TREE_TYPE (t1)) == TREE_CODE (TREE_TYPE (t2))
      && TYPE_PRECISION (TREE_TYPE (t1)) == TYPE_PRECISION (TREE_TYPE (t2)))
    return 1;

  /* Array types are also compatible if they are constrained and have the same
     domain(s) and the same component type.  */
  if (code == ARRAY_TYPE
      && (TYPE_DOMAIN (t1) == TYPE_DOMAIN (t2)
	  || (TYPE_DOMAIN (t1)
	      && TYPE_DOMAIN (t2)
	      && tree_int_cst_equal (TYPE_MIN_VALUE (TYPE_DOMAIN (t1)),
				     TYPE_MIN_VALUE (TYPE_DOMAIN (t2)))
	      && tree_int_cst_equal (TYPE_MAX_VALUE (TYPE_DOMAIN (t1)),
				     TYPE_MAX_VALUE (TYPE_DOMAIN (t2)))))
      && (TREE_TYPE (t1) == TREE_TYPE (t2)
	  || (TREE_CODE (TREE_TYPE (t1)) == ARRAY_TYPE
	      && gnat_types_compatible_p (TREE_TYPE (t1), TREE_TYPE (t2)))))
    return 1;

  /* Padding record types are also compatible if they pad the same
     type and have the same constant size.  */
  if (code == RECORD_TYPE
      && TYPE_PADDING_P (t1) && TYPE_PADDING_P (t2)
      && TREE_TYPE (TYPE_FIELDS (t1)) == TREE_TYPE (TYPE_FIELDS (t2))
      && tree_int_cst_equal (TYPE_SIZE (t1), TYPE_SIZE (t2)))
    return 1;

  return 0;
}

/* Return true if T, a FUNCTION_TYPE, has the specified list of flags.  */

bool
fntype_same_flags_p (const_tree t, tree cico_list, bool return_unconstrained_p,
		     bool return_by_direct_ref_p, bool return_by_invisi_ref_p)
{
  return TYPE_CI_CO_LIST (t) == cico_list
	 && TYPE_RETURN_UNCONSTRAINED_P (t) == return_unconstrained_p
	 && TYPE_RETURN_BY_DIRECT_REF_P (t) == return_by_direct_ref_p
	 && TREE_ADDRESSABLE (t) == return_by_invisi_ref_p;
}

/* EXP is an expression for the size of an object.  If this size contains
   discriminant references, replace them with the maximum (if MAX_P) or
   minimum (if !MAX_P) possible value of the discriminant.  */

tree
max_size (tree exp, bool max_p)
{
  enum tree_code code = TREE_CODE (exp);
  tree type = TREE_TYPE (exp);

  switch (TREE_CODE_CLASS (code))
    {
    case tcc_declaration:
    case tcc_constant:
      return exp;

    case tcc_vl_exp:
      if (code == CALL_EXPR)
	{
	  tree t, *argarray;
	  int n, i;

	  t = maybe_inline_call_in_expr (exp);
	  if (t)
	    return max_size (t, max_p);

	  n = call_expr_nargs (exp);
	  gcc_assert (n > 0);
	  argarray = XALLOCAVEC (tree, n);
	  for (i = 0; i < n; i++)
	    argarray[i] = max_size (CALL_EXPR_ARG (exp, i), max_p);
	  return build_call_array (type, CALL_EXPR_FN (exp), n, argarray);
	}
      break;

    case tcc_reference:
      /* If this contains a PLACEHOLDER_EXPR, it is the thing we want to
	 modify.  Otherwise, we treat it like a variable.  */
      if (!CONTAINS_PLACEHOLDER_P (exp))
	return exp;

      type = TREE_TYPE (TREE_OPERAND (exp, 1));
      return
	max_size (max_p ? TYPE_MAX_VALUE (type) : TYPE_MIN_VALUE (type), true);

    case tcc_comparison:
      return max_p ? size_one_node : size_zero_node;

    case tcc_unary:
    case tcc_binary:
    case tcc_expression:
      switch (TREE_CODE_LENGTH (code))
	{
	case 1:
	  if (code == NON_LVALUE_EXPR)
	    return max_size (TREE_OPERAND (exp, 0), max_p);
	  else
	    return
	      fold_build1 (code, type,
			   max_size (TREE_OPERAND (exp, 0),
				     code == NEGATE_EXPR ? !max_p : max_p));

	case 2:
	  if (code == COMPOUND_EXPR)
	    return max_size (TREE_OPERAND (exp, 1), max_p);

	  {
	    tree lhs = max_size (TREE_OPERAND (exp, 0), max_p);
	    tree rhs = max_size (TREE_OPERAND (exp, 1),
				 code == MINUS_EXPR ? !max_p : max_p);

	    /* Special-case wanting the maximum value of a MIN_EXPR.
	       In that case, if one side overflows, return the other.
	       sizetype is signed, but we know sizes are non-negative.
	       Likewise, handle a MINUS_EXPR or PLUS_EXPR with the LHS
	       overflowing and the RHS a variable.  */
	    if (max_p
		&& code == MIN_EXPR
		&& TREE_CODE (rhs) == INTEGER_CST
		&& TREE_OVERFLOW (rhs))
	      return lhs;
	    else if (max_p
		     && code == MIN_EXPR
		     && TREE_CODE (lhs) == INTEGER_CST
		     && TREE_OVERFLOW (lhs))
	      return rhs;
	    else if ((code == MINUS_EXPR || code == PLUS_EXPR)
		     && TREE_CODE (lhs) == INTEGER_CST
		     && TREE_OVERFLOW (lhs)
		     && !TREE_CONSTANT (rhs))
	      return lhs;
	    else
	      return fold_build2 (code, type, lhs, rhs);
	  }

	case 3:
	  if (code == SAVE_EXPR)
	    return exp;
	  else if (code == COND_EXPR)
	    return fold_build2 (max_p ? MAX_EXPR : MIN_EXPR, type,
				max_size (TREE_OPERAND (exp, 1), max_p),
				max_size (TREE_OPERAND (exp, 2), max_p));
	}

      /* Other tree classes cannot happen.  */
    default:
      break;
    }

  gcc_unreachable ();
}

/* Build a template of type TEMPLATE_TYPE from the array bounds of ARRAY_TYPE.
   EXPR is an expression that we can use to locate any PLACEHOLDER_EXPRs.
   Return a constructor for the template.  */

tree
build_template (tree template_type, tree array_type, tree expr)
{
  VEC(constructor_elt,gc) *template_elts = NULL;
  tree bound_list = NULL_TREE;
  tree field;

  while (TREE_CODE (array_type) == RECORD_TYPE
	 && (TYPE_PADDING_P (array_type)
	     || TYPE_JUSTIFIED_MODULAR_P (array_type)))
    array_type = TREE_TYPE (TYPE_FIELDS (array_type));

  if (TREE_CODE (array_type) == ARRAY_TYPE
      || (TREE_CODE (array_type) == INTEGER_TYPE
	  && TYPE_HAS_ACTUAL_BOUNDS_P (array_type)))
    bound_list = TYPE_ACTUAL_BOUNDS (array_type);

  /* First make the list for a CONSTRUCTOR for the template.  Go down the
     field list of the template instead of the type chain because this
     array might be an Ada array of arrays and we can't tell where the
     nested arrays stop being the underlying object.  */

  for (field = TYPE_FIELDS (template_type); field;
       (bound_list
	? (bound_list = TREE_CHAIN (bound_list))
	: (array_type = TREE_TYPE (array_type))),
       field = DECL_CHAIN (DECL_CHAIN (field)))
    {
      tree bounds, min, max;

      /* If we have a bound list, get the bounds from there.  Likewise
	 for an ARRAY_TYPE.  Otherwise, if expr is a PARM_DECL with
	 DECL_BY_COMPONENT_PTR_P, use the bounds of the field in the template.
	 This will give us a maximum range.  */
      if (bound_list)
	bounds = TREE_VALUE (bound_list);
      else if (TREE_CODE (array_type) == ARRAY_TYPE)
	bounds = TYPE_INDEX_TYPE (TYPE_DOMAIN (array_type));
      else if (expr && TREE_CODE (expr) == PARM_DECL
	       && DECL_BY_COMPONENT_PTR_P (expr))
	bounds = TREE_TYPE (field);
      else
	gcc_unreachable ();

      min = convert (TREE_TYPE (field), TYPE_MIN_VALUE (bounds));
      max = convert (TREE_TYPE (DECL_CHAIN (field)), TYPE_MAX_VALUE (bounds));

      /* If either MIN or MAX involve a PLACEHOLDER_EXPR, we must
	 substitute it from OBJECT.  */
      min = SUBSTITUTE_PLACEHOLDER_IN_EXPR (min, expr);
      max = SUBSTITUTE_PLACEHOLDER_IN_EXPR (max, expr);

      CONSTRUCTOR_APPEND_ELT (template_elts, field, min);
      CONSTRUCTOR_APPEND_ELT (template_elts, DECL_CHAIN (field), max);
    }

  return gnat_build_constructor (template_type, template_elts);
}

/* Helper routine to make a descriptor field.  FIELD_LIST is the list of decls
   being built; the new decl is chained on to the front of the list.  */

static tree
make_descriptor_field (const char *name, tree type, tree rec_type,
		       tree initial, tree field_list)
{
  tree field
    = create_field_decl (get_identifier (name), type, rec_type, NULL_TREE,
			 NULL_TREE, 0, 0);

  DECL_INITIAL (field) = initial;
  DECL_CHAIN (field) = field_list;
  return field;
}

/* Build a 32-bit VMS descriptor from a Mechanism_Type, which must specify a
   descriptor type, and the GCC type of an object.  Each FIELD_DECL in the
   type contains in its DECL_INITIAL the expression to use when a constructor
   is made for the type.  GNAT_ENTITY is an entity used to print out an error
   message if the mechanism cannot be applied to an object of that type and
   also for the name.  */

tree
build_vms_descriptor32 (tree type, Mechanism_Type mech, Entity_Id gnat_entity)
{
  tree record_type = make_node (RECORD_TYPE);
  tree pointer32_type, pointer64_type;
  tree field_list = NULL_TREE;
  int klass, ndim, i, dtype = 0;
  tree inner_type, tem;
  tree *idx_arr;

  /* If TYPE is an unconstrained array, use the underlying array type.  */
  if (TREE_CODE (type) == UNCONSTRAINED_ARRAY_TYPE)
    type = TREE_TYPE (TREE_TYPE (TYPE_FIELDS (TREE_TYPE (type))));

  /* If this is an array, compute the number of dimensions in the array,
     get the index types, and point to the inner type.  */
  if (TREE_CODE (type) != ARRAY_TYPE)
    ndim = 0;
  else
    for (ndim = 1, inner_type = type;
	 TREE_CODE (TREE_TYPE (inner_type)) == ARRAY_TYPE
	 && TYPE_MULTI_ARRAY_P (TREE_TYPE (inner_type));
	 ndim++, inner_type = TREE_TYPE (inner_type))
      ;

  idx_arr = XALLOCAVEC (tree, ndim);

  if (mech != By_Descriptor_NCA && mech != By_Short_Descriptor_NCA
      && TREE_CODE (type) == ARRAY_TYPE && TYPE_CONVENTION_FORTRAN_P (type))
    for (i = ndim - 1, inner_type = type;
	 i >= 0;
	 i--, inner_type = TREE_TYPE (inner_type))
      idx_arr[i] = TYPE_DOMAIN (inner_type);
  else
    for (i = 0, inner_type = type;
	 i < ndim;
	 i++, inner_type = TREE_TYPE (inner_type))
      idx_arr[i] = TYPE_DOMAIN (inner_type);

  /* Now get the DTYPE value.  */
  switch (TREE_CODE (type))
    {
    case INTEGER_TYPE:
    case ENUMERAL_TYPE:
    case BOOLEAN_TYPE:
      if (TYPE_VAX_FLOATING_POINT_P (type))
	switch (tree_low_cst (TYPE_DIGITS_VALUE (type), 1))
	  {
	  case 6:
	    dtype = 10;
	    break;
	  case 9:
	    dtype = 11;
	    break;
	  case 15:
	    dtype = 27;
	    break;
	  }
      else
	switch (GET_MODE_BITSIZE (TYPE_MODE (type)))
	  {
	  case 8:
	    dtype = TYPE_UNSIGNED (type) ? 2 : 6;
	    break;
	  case 16:
	    dtype = TYPE_UNSIGNED (type) ? 3 : 7;
	    break;
	  case 32:
	    dtype = TYPE_UNSIGNED (type) ? 4 : 8;
	    break;
	  case 64:
	    dtype = TYPE_UNSIGNED (type) ? 5 : 9;
	    break;
	  case 128:
	    dtype = TYPE_UNSIGNED (type) ? 25 : 26;
	    break;
	  }
      break;

    case REAL_TYPE:
      dtype = GET_MODE_BITSIZE (TYPE_MODE (type)) == 32 ? 52 : 53;
      break;

    case COMPLEX_TYPE:
      if (TREE_CODE (TREE_TYPE (type)) == INTEGER_TYPE
	  && TYPE_VAX_FLOATING_POINT_P (type))
	switch (tree_low_cst (TYPE_DIGITS_VALUE (type), 1))
	  {
	  case 6:
	    dtype = 12;
	    break;
	  case 9:
	    dtype = 13;
	    break;
	  case 15:
	    dtype = 29;
	  }
      else
	dtype = GET_MODE_BITSIZE (TYPE_MODE (TREE_TYPE (type))) == 32 ? 54: 55;
      break;

    case ARRAY_TYPE:
      dtype = 14;
      break;

    default:
      break;
    }

  /* Get the CLASS value.  */
  switch (mech)
    {
    case By_Descriptor_A:
    case By_Short_Descriptor_A:
      klass = 4;
      break;
    case By_Descriptor_NCA:
    case By_Short_Descriptor_NCA:
      klass = 10;
      break;
    case By_Descriptor_SB:
    case By_Short_Descriptor_SB:
      klass = 15;
      break;
    case By_Descriptor:
    case By_Short_Descriptor:
    case By_Descriptor_S:
    case By_Short_Descriptor_S:
    default:
      klass = 1;
      break;
    }

  /* Make the type for a descriptor for VMS.  The first four fields are the
     same for all types.  */
  field_list
    = make_descriptor_field ("LENGTH", gnat_type_for_size (16, 1), record_type,
			     size_in_bytes ((mech == By_Descriptor_A
					     || mech == By_Short_Descriptor_A)
					    ? inner_type : type),
			     field_list);
  field_list
    = make_descriptor_field ("DTYPE", gnat_type_for_size (8, 1), record_type,
			     size_int (dtype), field_list);
  field_list
    = make_descriptor_field ("CLASS", gnat_type_for_size (8, 1), record_type,
			     size_int (klass), field_list);

  pointer32_type = build_pointer_type_for_mode (type, SImode, false);
  pointer64_type = build_pointer_type_for_mode (type, DImode, false);

  /* Ensure that only 32-bit pointers are passed in 32-bit descriptors.  Note
     that we cannot build a template call to the CE routine as it would get a
     wrong source location; instead we use a second placeholder for it.  */
  tem = build_unary_op (ADDR_EXPR, pointer64_type,
			build0 (PLACEHOLDER_EXPR, type));
  tem = build3 (COND_EXPR, pointer32_type,
		build_binary_op (GE_EXPR, boolean_type_node, tem,
				 build_int_cstu (pointer64_type, 0x80000000)),
		build0 (PLACEHOLDER_EXPR, void_type_node),
		convert (pointer32_type, tem));

  field_list
    = make_descriptor_field ("POINTER", pointer32_type, record_type, tem,
			     field_list);

  switch (mech)
    {
    case By_Descriptor:
    case By_Short_Descriptor:
    case By_Descriptor_S:
    case By_Short_Descriptor_S:
      break;

    case By_Descriptor_SB:
    case By_Short_Descriptor_SB:
      field_list
	= make_descriptor_field ("SB_L1", gnat_type_for_size (32, 1),
			         record_type,
			         (TREE_CODE (type) == ARRAY_TYPE
				  ? TYPE_MIN_VALUE (TYPE_DOMAIN (type))
				  : size_zero_node),
				 field_list);
      field_list
	= make_descriptor_field ("SB_U1", gnat_type_for_size (32, 1),
				 record_type,
				 (TREE_CODE (type) == ARRAY_TYPE
				  ? TYPE_MAX_VALUE (TYPE_DOMAIN (type))
				  : size_zero_node),
				 field_list);
      break;

    case By_Descriptor_A:
    case By_Short_Descriptor_A:
    case By_Descriptor_NCA:
    case By_Short_Descriptor_NCA:
      field_list
	= make_descriptor_field ("SCALE", gnat_type_for_size (8, 1),
				 record_type, size_zero_node, field_list);

      field_list
	= make_descriptor_field ("DIGITS", gnat_type_for_size (8, 1),
				 record_type, size_zero_node, field_list);

      field_list
	= make_descriptor_field ("AFLAGS", gnat_type_for_size (8, 1),
				 record_type,
				 size_int ((mech == By_Descriptor_NCA
					    || mech == By_Short_Descriptor_NCA)
					   ? 0
					   /* Set FL_COLUMN, FL_COEFF, and
					      FL_BOUNDS.  */
					   : (TREE_CODE (type) == ARRAY_TYPE
					      && TYPE_CONVENTION_FORTRAN_P
						 (type)
					     ? 224 : 192)),
				 field_list);

      field_list
	= make_descriptor_field ("DIMCT", gnat_type_for_size (8, 1),
				 record_type, size_int (ndim), field_list);

      field_list
	= make_descriptor_field ("ARSIZE", gnat_type_for_size (32, 1),
				 record_type, size_in_bytes (type),
				 field_list);

      /* Now build a pointer to the 0,0,0... element.  */
      tem = build0 (PLACEHOLDER_EXPR, type);
      for (i = 0, inner_type = type; i < ndim;
	   i++, inner_type = TREE_TYPE (inner_type))
	tem = build4 (ARRAY_REF, TREE_TYPE (inner_type), tem,
		      convert (TYPE_DOMAIN (inner_type), size_zero_node),
		      NULL_TREE, NULL_TREE);

      field_list
	= make_descriptor_field ("A0", pointer32_type, record_type,
				 build1 (ADDR_EXPR, pointer32_type, tem),
				 field_list);

      /* Next come the addressing coefficients.  */
      tem = size_one_node;
      for (i = 0; i < ndim; i++)
	{
	  char fname[3];
	  tree idx_length
	    = size_binop (MULT_EXPR, tem,
			  size_binop (PLUS_EXPR,
				      size_binop (MINUS_EXPR,
						  TYPE_MAX_VALUE (idx_arr[i]),
						  TYPE_MIN_VALUE (idx_arr[i])),
				      size_int (1)));

	  fname[0] = ((mech == By_Descriptor_NCA ||
                       mech == By_Short_Descriptor_NCA) ? 'S' : 'M');
	  fname[1] = '0' + i, fname[2] = 0;
	  field_list
	    = make_descriptor_field (fname, gnat_type_for_size (32, 1),
				     record_type, idx_length, field_list);

	  if (mech == By_Descriptor_NCA || mech == By_Short_Descriptor_NCA)
	    tem = idx_length;
	}

      /* Finally here are the bounds.  */
      for (i = 0; i < ndim; i++)
	{
	  char fname[3];

	  fname[0] = 'L', fname[1] = '0' + i, fname[2] = 0;
	  field_list
	    = make_descriptor_field (fname, gnat_type_for_size (32, 1),
				     record_type, TYPE_MIN_VALUE (idx_arr[i]),
				     field_list);

	  fname[0] = 'U';
	  field_list
	    = make_descriptor_field (fname, gnat_type_for_size (32, 1),
				     record_type, TYPE_MAX_VALUE (idx_arr[i]),
				     field_list);
	}
      break;

    default:
      post_error ("unsupported descriptor type for &", gnat_entity);
    }

  TYPE_NAME (record_type) = create_concat_name (gnat_entity, "DESC");
  finish_record_type (record_type, nreverse (field_list), 0, false);
  return record_type;
}

/* Build a 64-bit VMS descriptor from a Mechanism_Type, which must specify a
   descriptor type, and the GCC type of an object.  Each FIELD_DECL in the
   type contains in its DECL_INITIAL the expression to use when a constructor
   is made for the type.  GNAT_ENTITY is an entity used to print out an error
   message if the mechanism cannot be applied to an object of that type and
   also for the name.  */

tree
build_vms_descriptor (tree type, Mechanism_Type mech, Entity_Id gnat_entity)
{
  tree record_type = make_node (RECORD_TYPE);
  tree pointer64_type;
  tree field_list = NULL_TREE;
  int klass, ndim, i, dtype = 0;
  tree inner_type, tem;
  tree *idx_arr;

  /* If TYPE is an unconstrained array, use the underlying array type.  */
  if (TREE_CODE (type) == UNCONSTRAINED_ARRAY_TYPE)
    type = TREE_TYPE (TREE_TYPE (TYPE_FIELDS (TREE_TYPE (type))));

  /* If this is an array, compute the number of dimensions in the array,
     get the index types, and point to the inner type.  */
  if (TREE_CODE (type) != ARRAY_TYPE)
    ndim = 0;
  else
    for (ndim = 1, inner_type = type;
	 TREE_CODE (TREE_TYPE (inner_type)) == ARRAY_TYPE
	 && TYPE_MULTI_ARRAY_P (TREE_TYPE (inner_type));
	 ndim++, inner_type = TREE_TYPE (inner_type))
      ;

  idx_arr = XALLOCAVEC (tree, ndim);

  if (mech != By_Descriptor_NCA
      && TREE_CODE (type) == ARRAY_TYPE && TYPE_CONVENTION_FORTRAN_P (type))
    for (i = ndim - 1, inner_type = type;
	 i >= 0;
	 i--, inner_type = TREE_TYPE (inner_type))
      idx_arr[i] = TYPE_DOMAIN (inner_type);
  else
    for (i = 0, inner_type = type;
	 i < ndim;
	 i++, inner_type = TREE_TYPE (inner_type))
      idx_arr[i] = TYPE_DOMAIN (inner_type);

  /* Now get the DTYPE value.  */
  switch (TREE_CODE (type))
    {
    case INTEGER_TYPE:
    case ENUMERAL_TYPE:
    case BOOLEAN_TYPE:
      if (TYPE_VAX_FLOATING_POINT_P (type))
	switch (tree_low_cst (TYPE_DIGITS_VALUE (type), 1))
	  {
	  case 6:
	    dtype = 10;
	    break;
	  case 9:
	    dtype = 11;
	    break;
	  case 15:
	    dtype = 27;
	    break;
	  }
      else
	switch (GET_MODE_BITSIZE (TYPE_MODE (type)))
	  {
	  case 8:
	    dtype = TYPE_UNSIGNED (type) ? 2 : 6;
	    break;
	  case 16:
	    dtype = TYPE_UNSIGNED (type) ? 3 : 7;
	    break;
	  case 32:
	    dtype = TYPE_UNSIGNED (type) ? 4 : 8;
	    break;
	  case 64:
	    dtype = TYPE_UNSIGNED (type) ? 5 : 9;
	    break;
	  case 128:
	    dtype = TYPE_UNSIGNED (type) ? 25 : 26;
	    break;
	  }
      break;

    case REAL_TYPE:
      dtype = GET_MODE_BITSIZE (TYPE_MODE (type)) == 32 ? 52 : 53;
      break;

    case COMPLEX_TYPE:
      if (TREE_CODE (TREE_TYPE (type)) == INTEGER_TYPE
	  && TYPE_VAX_FLOATING_POINT_P (type))
	switch (tree_low_cst (TYPE_DIGITS_VALUE (type), 1))
	  {
	  case 6:
	    dtype = 12;
	    break;
	  case 9:
	    dtype = 13;
	    break;
	  case 15:
	    dtype = 29;
	  }
      else
	dtype = GET_MODE_BITSIZE (TYPE_MODE (TREE_TYPE (type))) == 32 ? 54: 55;
      break;

    case ARRAY_TYPE:
      dtype = 14;
      break;

    default:
      break;
    }

  /* Get the CLASS value.  */
  switch (mech)
    {
    case By_Descriptor_A:
      klass = 4;
      break;
    case By_Descriptor_NCA:
      klass = 10;
      break;
    case By_Descriptor_SB:
      klass = 15;
      break;
    case By_Descriptor:
    case By_Descriptor_S:
    default:
      klass = 1;
      break;
    }

  /* Make the type for a 64-bit descriptor for VMS.  The first six fields
     are the same for all types.  */
  field_list
    = make_descriptor_field ("MBO", gnat_type_for_size (16, 1),
			     record_type, size_int (1), field_list);
  field_list
    = make_descriptor_field ("DTYPE", gnat_type_for_size (8, 1),
			     record_type, size_int (dtype), field_list);
  field_list
    = make_descriptor_field ("CLASS", gnat_type_for_size (8, 1),
			     record_type, size_int (klass), field_list);
  field_list
    = make_descriptor_field ("MBMO", gnat_type_for_size (32, 1),
			     record_type, ssize_int (-1), field_list);
  field_list
    = make_descriptor_field ("LENGTH", gnat_type_for_size (64, 1),
			     record_type,
			     size_in_bytes (mech == By_Descriptor_A
					    ? inner_type : type),
			     field_list);

  pointer64_type = build_pointer_type_for_mode (type, DImode, false);

  field_list
    = make_descriptor_field ("POINTER", pointer64_type, record_type,
			     build_unary_op (ADDR_EXPR, pointer64_type,
					     build0 (PLACEHOLDER_EXPR, type)),
			     field_list);

  switch (mech)
    {
    case By_Descriptor:
    case By_Descriptor_S:
      break;

    case By_Descriptor_SB:
      field_list
	= make_descriptor_field ("SB_L1", gnat_type_for_size (64, 1),
				 record_type,
				 (TREE_CODE (type) == ARRAY_TYPE
				  ? TYPE_MIN_VALUE (TYPE_DOMAIN (type))
				  : size_zero_node),
				 field_list);
      field_list
	= make_descriptor_field ("SB_U1", gnat_type_for_size (64, 1),
				 record_type,
				 (TREE_CODE (type) == ARRAY_TYPE
				  ? TYPE_MAX_VALUE (TYPE_DOMAIN (type))
				  : size_zero_node),
				 field_list);
      break;

    case By_Descriptor_A:
    case By_Descriptor_NCA:
      field_list
	= make_descriptor_field ("SCALE", gnat_type_for_size (8, 1),
				 record_type, size_zero_node, field_list);

      field_list
	= make_descriptor_field ("DIGITS", gnat_type_for_size (8, 1),
				 record_type, size_zero_node, field_list);

      dtype = (mech == By_Descriptor_NCA
	       ? 0
	       /* Set FL_COLUMN, FL_COEFF, and
		  FL_BOUNDS.  */
	       : (TREE_CODE (type) == ARRAY_TYPE
		  && TYPE_CONVENTION_FORTRAN_P (type)
		  ? 224 : 192));
      field_list
	= make_descriptor_field ("AFLAGS", gnat_type_for_size (8, 1),
				 record_type, size_int (dtype),
				 field_list);

      field_list
	= make_descriptor_field ("DIMCT", gnat_type_for_size (8, 1),
				 record_type, size_int (ndim), field_list);

      field_list
	= make_descriptor_field ("MBZ", gnat_type_for_size (32, 1),
				 record_type, size_int (0), field_list);
      field_list
	= make_descriptor_field ("ARSIZE", gnat_type_for_size (64, 1),
				 record_type, size_in_bytes (type),
				 field_list);

      /* Now build a pointer to the 0,0,0... element.  */
      tem = build0 (PLACEHOLDER_EXPR, type);
      for (i = 0, inner_type = type; i < ndim;
	   i++, inner_type = TREE_TYPE (inner_type))
	tem = build4 (ARRAY_REF, TREE_TYPE (inner_type), tem,
		      convert (TYPE_DOMAIN (inner_type), size_zero_node),
		      NULL_TREE, NULL_TREE);

      field_list
	= make_descriptor_field ("A0", pointer64_type, record_type,
				 build1 (ADDR_EXPR, pointer64_type, tem),
				 field_list);

      /* Next come the addressing coefficients.  */
      tem = size_one_node;
      for (i = 0; i < ndim; i++)
	{
	  char fname[3];
	  tree idx_length
	    = size_binop (MULT_EXPR, tem,
			  size_binop (PLUS_EXPR,
				      size_binop (MINUS_EXPR,
						  TYPE_MAX_VALUE (idx_arr[i]),
						  TYPE_MIN_VALUE (idx_arr[i])),
				      size_int (1)));

	  fname[0] = (mech == By_Descriptor_NCA ? 'S' : 'M');
	  fname[1] = '0' + i, fname[2] = 0;
	  field_list
	    = make_descriptor_field (fname, gnat_type_for_size (64, 1),
				     record_type, idx_length, field_list);

	  if (mech == By_Descriptor_NCA)
	    tem = idx_length;
	}

      /* Finally here are the bounds.  */
      for (i = 0; i < ndim; i++)
	{
	  char fname[3];

	  fname[0] = 'L', fname[1] = '0' + i, fname[2] = 0;
	  field_list
	    = make_descriptor_field (fname, gnat_type_for_size (64, 1),
				     record_type,
				     TYPE_MIN_VALUE (idx_arr[i]), field_list);

	  fname[0] = 'U';
	  field_list
	    = make_descriptor_field (fname, gnat_type_for_size (64, 1),
				     record_type,
				     TYPE_MAX_VALUE (idx_arr[i]), field_list);
	}
      break;

    default:
      post_error ("unsupported descriptor type for &", gnat_entity);
    }

  TYPE_NAME (record_type) = create_concat_name (gnat_entity, "DESC64");
  finish_record_type (record_type, nreverse (field_list), 0, false);
  return record_type;
}

/* Fill in a VMS descriptor of GNU_TYPE for GNU_EXPR and return the result.
   GNAT_ACTUAL is the actual parameter for which the descriptor is built.  */

tree
fill_vms_descriptor (tree gnu_type, tree gnu_expr, Node_Id gnat_actual)
{
  VEC(constructor_elt,gc) *v = NULL;
  tree field;

  gnu_expr = maybe_unconstrained_array (gnu_expr);
  gnu_expr = gnat_protect_expr (gnu_expr);
  gnat_mark_addressable (gnu_expr);

  /* We may need to substitute both GNU_EXPR and a CALL_EXPR to the raise CE
     routine in case we have a 32-bit descriptor.  */
  gnu_expr = build2 (COMPOUND_EXPR, void_type_node,
		     build_call_raise (CE_Range_Check_Failed, gnat_actual,
				       N_Raise_Constraint_Error),
		     gnu_expr);

  for (field = TYPE_FIELDS (gnu_type); field; field = DECL_CHAIN (field))
    {
      tree value
	= convert (TREE_TYPE (field),
		   SUBSTITUTE_PLACEHOLDER_IN_EXPR (DECL_INITIAL (field),
						   gnu_expr));
      CONSTRUCTOR_APPEND_ELT (v, field, value);
    }

  return gnat_build_constructor (gnu_type, v);
}

/* Convert GNU_EXPR, a pointer to a 64bit VMS descriptor, to GNU_TYPE, a
   regular pointer or fat pointer type.  GNAT_SUBPROG is the subprogram to
   which the VMS descriptor is passed.  */

static tree
convert_vms_descriptor64 (tree gnu_type, tree gnu_expr, Entity_Id gnat_subprog)
{
  tree desc_type = TREE_TYPE (TREE_TYPE (gnu_expr));
  tree desc = build1 (INDIRECT_REF, desc_type, gnu_expr);
  /* The CLASS field is the 3rd field in the descriptor.  */
  tree klass = DECL_CHAIN (DECL_CHAIN (TYPE_FIELDS (desc_type)));
  /* The POINTER field is the 6th field in the descriptor.  */
  tree pointer = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (klass)));

  /* Retrieve the value of the POINTER field.  */
  tree gnu_expr64
    = build3 (COMPONENT_REF, TREE_TYPE (pointer), desc, pointer, NULL_TREE);

  if (POINTER_TYPE_P (gnu_type))
    return convert (gnu_type, gnu_expr64);

  else if (TYPE_IS_FAT_POINTER_P (gnu_type))
    {
      tree p_array_type = TREE_TYPE (TYPE_FIELDS (gnu_type));
      tree p_bounds_type = TREE_TYPE (TREE_CHAIN (TYPE_FIELDS (gnu_type)));
      tree template_type = TREE_TYPE (p_bounds_type);
      tree min_field = TYPE_FIELDS (template_type);
      tree max_field = TREE_CHAIN (TYPE_FIELDS (template_type));
      tree template_tree, template_addr, aflags, dimct, t, u;
      /* See the head comment of build_vms_descriptor.  */
      int iklass = TREE_INT_CST_LOW (DECL_INITIAL (klass));
      tree lfield, ufield;
      VEC(constructor_elt,gc) *v;

      /* Convert POINTER to the pointer-to-array type.  */
      gnu_expr64 = convert (p_array_type, gnu_expr64);

      switch (iklass)
	{
	case 1:  /* Class S  */
	case 15: /* Class SB */
	  /* Build {1, LENGTH} template; LENGTH64 is the 5th field.  */
	  v = VEC_alloc (constructor_elt, gc, 2);
	  t = DECL_CHAIN (DECL_CHAIN (klass));
	  t = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
	  CONSTRUCTOR_APPEND_ELT (v, min_field,
				  convert (TREE_TYPE (min_field),
					   integer_one_node));
	  CONSTRUCTOR_APPEND_ELT (v, max_field,
				  convert (TREE_TYPE (max_field), t));
	  template_tree = gnat_build_constructor (template_type, v);
	  template_addr = build_unary_op (ADDR_EXPR, NULL_TREE, template_tree);

	  /* For class S, we are done.  */
	  if (iklass == 1)
	    break;

	  /* Test that we really have a SB descriptor, like DEC Ada.  */
	  t = build3 (COMPONENT_REF, TREE_TYPE (klass), desc, klass, NULL);
	  u = convert (TREE_TYPE (klass), DECL_INITIAL (klass));
	  u = build_binary_op (EQ_EXPR, boolean_type_node, t, u);
	  /* If so, there is already a template in the descriptor and
	     it is located right after the POINTER field.  The fields are
             64bits so they must be repacked. */
	  t = TREE_CHAIN (pointer);
          lfield = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
          lfield = convert (TREE_TYPE (TYPE_FIELDS (template_type)), lfield);

	  t = TREE_CHAIN (t);
          ufield = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
          ufield = convert
           (TREE_TYPE (DECL_CHAIN (TYPE_FIELDS (template_type))), ufield);

	  /* Build the template in the form of a constructor. */
	  v = VEC_alloc (constructor_elt, gc, 2);
	  CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (template_type), lfield);
	  CONSTRUCTOR_APPEND_ELT (v, TREE_CHAIN (TYPE_FIELDS (template_type)),
				  ufield);
	  template_tree = gnat_build_constructor (template_type, v);

	  /* Otherwise use the {1, LENGTH} template we build above.  */
	  template_addr = build3 (COND_EXPR, p_bounds_type, u,
				  build_unary_op (ADDR_EXPR, p_bounds_type,
				 		 template_tree),
				  template_addr);
	  break;

	case 4:  /* Class A */
	  /* The AFLAGS field is the 3rd field after the pointer in the
             descriptor.  */
	  t = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (pointer)));
	  aflags = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
	  /* The DIMCT field is the next field in the descriptor after
             aflags.  */
	  t = TREE_CHAIN (t);
	  dimct = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
	  /* Raise CONSTRAINT_ERROR if either more than 1 dimension
	     or FL_COEFF or FL_BOUNDS not set.  */
	  u = build_int_cst (TREE_TYPE (aflags), 192);
	  u = build_binary_op (TRUTH_OR_EXPR, boolean_type_node,
			       build_binary_op (NE_EXPR, boolean_type_node,
						dimct,
						convert (TREE_TYPE (dimct),
							 size_one_node)),
			       build_binary_op (NE_EXPR, boolean_type_node,
						build2 (BIT_AND_EXPR,
							TREE_TYPE (aflags),
							aflags, u),
						u));
	  /* There is already a template in the descriptor and it is located
             in block 3.  The fields are 64bits so they must be repacked. */
	  t = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (DECL_CHAIN
              (t)))));
          lfield = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
          lfield = convert (TREE_TYPE (TYPE_FIELDS (template_type)), lfield);

	  t = TREE_CHAIN (t);
          ufield = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
          ufield = convert
           (TREE_TYPE (DECL_CHAIN (TYPE_FIELDS (template_type))), ufield);

	  /* Build the template in the form of a constructor. */
	  v = VEC_alloc (constructor_elt, gc, 2);
	  CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (template_type), lfield);
	  CONSTRUCTOR_APPEND_ELT (v, DECL_CHAIN (TYPE_FIELDS (template_type)),
				  ufield);
	  template_tree = gnat_build_constructor (template_type, v);
	  template_tree = build3 (COND_EXPR, template_type, u,
			    build_call_raise (CE_Length_Check_Failed, Empty,
					      N_Raise_Constraint_Error),
			    template_tree);
	  template_addr
	    = build_unary_op (ADDR_EXPR, p_bounds_type, template_tree);
	  break;

	case 10: /* Class NCA */
	default:
	  post_error ("unsupported descriptor type for &", gnat_subprog);
	  template_addr = integer_zero_node;
	  break;
	}

      /* Build the fat pointer in the form of a constructor.  */
      v = VEC_alloc (constructor_elt, gc, 2);
      CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (gnu_type), gnu_expr64);
      CONSTRUCTOR_APPEND_ELT (v, DECL_CHAIN (TYPE_FIELDS (gnu_type)),
			      template_addr);
      return gnat_build_constructor (gnu_type, v);
    }

  else
    gcc_unreachable ();
}

/* Convert GNU_EXPR, a pointer to a 32bit VMS descriptor, to GNU_TYPE, a
   regular pointer or fat pointer type.  GNAT_SUBPROG is the subprogram to
   which the VMS descriptor is passed.  */

static tree
convert_vms_descriptor32 (tree gnu_type, tree gnu_expr, Entity_Id gnat_subprog)
{
  tree desc_type = TREE_TYPE (TREE_TYPE (gnu_expr));
  tree desc = build1 (INDIRECT_REF, desc_type, gnu_expr);
  /* The CLASS field is the 3rd field in the descriptor.  */
  tree klass = DECL_CHAIN (DECL_CHAIN (TYPE_FIELDS (desc_type)));
  /* The POINTER field is the 4th field in the descriptor.  */
  tree pointer = DECL_CHAIN (klass);

  /* Retrieve the value of the POINTER field.  */
  tree gnu_expr32
    = build3 (COMPONENT_REF, TREE_TYPE (pointer), desc, pointer, NULL_TREE);

  if (POINTER_TYPE_P (gnu_type))
    return convert (gnu_type, gnu_expr32);

  else if (TYPE_IS_FAT_POINTER_P (gnu_type))
    {
      tree p_array_type = TREE_TYPE (TYPE_FIELDS (gnu_type));
      tree p_bounds_type = TREE_TYPE (TREE_CHAIN (TYPE_FIELDS (gnu_type)));
      tree template_type = TREE_TYPE (p_bounds_type);
      tree min_field = TYPE_FIELDS (template_type);
      tree max_field = TREE_CHAIN (TYPE_FIELDS (template_type));
      tree template_tree, template_addr, aflags, dimct, t, u;
      /* See the head comment of build_vms_descriptor.  */
      int iklass = TREE_INT_CST_LOW (DECL_INITIAL (klass));
      VEC(constructor_elt,gc) *v;

      /* Convert POINTER to the pointer-to-array type.  */
      gnu_expr32 = convert (p_array_type, gnu_expr32);

      switch (iklass)
	{
	case 1:  /* Class S  */
	case 15: /* Class SB */
	  /* Build {1, LENGTH} template; LENGTH is the 1st field.  */
	  v = VEC_alloc (constructor_elt, gc, 2);
	  t = TYPE_FIELDS (desc_type);
	  t = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
	  CONSTRUCTOR_APPEND_ELT (v, min_field,
				  convert (TREE_TYPE (min_field),
					   integer_one_node));
	  CONSTRUCTOR_APPEND_ELT (v, max_field,
				  convert (TREE_TYPE (max_field), t));
	  template_tree = gnat_build_constructor (template_type, v);
	  template_addr = build_unary_op (ADDR_EXPR, NULL_TREE, template_tree);

	  /* For class S, we are done.  */
	  if (iklass == 1)
	    break;

	  /* Test that we really have a SB descriptor, like DEC Ada.  */
	  t = build3 (COMPONENT_REF, TREE_TYPE (klass), desc, klass, NULL);
	  u = convert (TREE_TYPE (klass), DECL_INITIAL (klass));
	  u = build_binary_op (EQ_EXPR, boolean_type_node, t, u);
	  /* If so, there is already a template in the descriptor and
	     it is located right after the POINTER field.  */
	  t = TREE_CHAIN (pointer);
	  template_tree
	    = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
	  /* Otherwise use the {1, LENGTH} template we build above.  */
	  template_addr = build3 (COND_EXPR, p_bounds_type, u,
				  build_unary_op (ADDR_EXPR, p_bounds_type,
				 		 template_tree),
				  template_addr);
	  break;

	case 4:  /* Class A */
	  /* The AFLAGS field is the 7th field in the descriptor.  */
	  t = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (pointer)));
	  aflags = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
	  /* The DIMCT field is the 8th field in the descriptor.  */
	  t = TREE_CHAIN (t);
	  dimct = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
	  /* Raise CONSTRAINT_ERROR if either more than 1 dimension
	     or FL_COEFF or FL_BOUNDS not set.  */
	  u = build_int_cst (TREE_TYPE (aflags), 192);
	  u = build_binary_op (TRUTH_OR_EXPR, boolean_type_node,
			       build_binary_op (NE_EXPR, boolean_type_node,
						dimct,
						convert (TREE_TYPE (dimct),
							 size_one_node)),
			       build_binary_op (NE_EXPR, boolean_type_node,
						build2 (BIT_AND_EXPR,
							TREE_TYPE (aflags),
							aflags, u),
						u));
	  /* There is already a template in the descriptor and it is
	     located at the start of block 3 (12th field).  */
	  t = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (t))));
	  template_tree
	    = build3 (COMPONENT_REF, TREE_TYPE (t), desc, t, NULL_TREE);
	  template_tree = build3 (COND_EXPR, TREE_TYPE (t), u,
			    build_call_raise (CE_Length_Check_Failed, Empty,
					      N_Raise_Constraint_Error),
			    template_tree);
	  template_addr
	    = build_unary_op (ADDR_EXPR, p_bounds_type, template_tree);
	  break;

	case 10: /* Class NCA */
	default:
	  post_error ("unsupported descriptor type for &", gnat_subprog);
	  template_addr = integer_zero_node;
	  break;
	}

      /* Build the fat pointer in the form of a constructor.  */
      v = VEC_alloc (constructor_elt, gc, 2);
      CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (gnu_type), gnu_expr32);
      CONSTRUCTOR_APPEND_ELT (v, DECL_CHAIN (TYPE_FIELDS (gnu_type)),
			      template_addr);

      return gnat_build_constructor (gnu_type, v);
    }

  else
    gcc_unreachable ();
}

/* Convert GNU_EXPR, a pointer to a VMS descriptor, to GNU_TYPE, a regular
   pointer or fat pointer type.  GNU_EXPR_ALT_TYPE is the alternate (32-bit)
   pointer type of GNU_EXPR.  BY_REF is true if the result is to be used by
   reference.  GNAT_SUBPROG is the subprogram to which the VMS descriptor is
   passed.  */

static tree
convert_vms_descriptor (tree gnu_type, tree gnu_expr, tree gnu_expr_alt_type,
			bool by_ref, Entity_Id gnat_subprog)
{
  tree desc_type = TREE_TYPE (TREE_TYPE (gnu_expr));
  tree desc = build1 (INDIRECT_REF, desc_type, gnu_expr);
  tree mbo = TYPE_FIELDS (desc_type);
  const char *mbostr = IDENTIFIER_POINTER (DECL_NAME (mbo));
  tree mbmo = DECL_CHAIN (DECL_CHAIN (DECL_CHAIN (mbo)));
  tree real_type, is64bit, gnu_expr32, gnu_expr64;

  if (by_ref)
    real_type = TREE_TYPE (gnu_type);
  else
    real_type = gnu_type;

  /* If the field name is not MBO, it must be 32-bit and no alternate.
     Otherwise primary must be 64-bit and alternate 32-bit.  */
  if (strcmp (mbostr, "MBO") != 0)
    {
      tree ret = convert_vms_descriptor32 (real_type, gnu_expr, gnat_subprog);
      if (by_ref)
	ret = build_unary_op (ADDR_EXPR, gnu_type, ret);
      return ret;
    }

  /* Build the test for 64-bit descriptor.  */
  mbo = build3 (COMPONENT_REF, TREE_TYPE (mbo), desc, mbo, NULL_TREE);
  mbmo = build3 (COMPONENT_REF, TREE_TYPE (mbmo), desc, mbmo, NULL_TREE);
  is64bit
    = build_binary_op (TRUTH_ANDIF_EXPR, boolean_type_node,
		       build_binary_op (EQ_EXPR, boolean_type_node,
					convert (integer_type_node, mbo),
					integer_one_node),
		       build_binary_op (EQ_EXPR, boolean_type_node,
					convert (integer_type_node, mbmo),
					integer_minus_one_node));

  /* Build the 2 possible end results.  */
  gnu_expr64 = convert_vms_descriptor64 (real_type, gnu_expr, gnat_subprog);
  if (by_ref)
    gnu_expr64 =  build_unary_op (ADDR_EXPR, gnu_type, gnu_expr64);
  gnu_expr = fold_convert (gnu_expr_alt_type, gnu_expr);
  gnu_expr32 = convert_vms_descriptor32 (real_type, gnu_expr, gnat_subprog);
  if (by_ref)
    gnu_expr32 =  build_unary_op (ADDR_EXPR, gnu_type, gnu_expr32);

  return build3 (COND_EXPR, gnu_type, is64bit, gnu_expr64, gnu_expr32);
}

/* Build a stub for the subprogram specified by the GCC tree GNU_SUBPROG
   and the GNAT node GNAT_SUBPROG.  */

void
build_function_stub (tree gnu_subprog, Entity_Id gnat_subprog)
{
  tree gnu_subprog_type, gnu_subprog_addr, gnu_subprog_call;
  tree gnu_subprog_param, gnu_stub_param, gnu_param;
  tree gnu_stub_decl = DECL_FUNCTION_STUB (gnu_subprog);
  VEC(tree,gc) *gnu_param_vec = NULL;

  gnu_subprog_type = TREE_TYPE (gnu_subprog);

  /* Initialize the information structure for the function.  */
  allocate_struct_function (gnu_stub_decl, false);
  set_cfun (NULL);

  begin_subprog_body (gnu_stub_decl);

  start_stmt_group ();
  gnat_pushlevel ();

  /* Loop over the parameters of the stub and translate any of them
     passed by descriptor into a by reference one.  */
  for (gnu_stub_param = DECL_ARGUMENTS (gnu_stub_decl),
       gnu_subprog_param = DECL_ARGUMENTS (gnu_subprog);
       gnu_stub_param;
       gnu_stub_param = TREE_CHAIN (gnu_stub_param),
       gnu_subprog_param = TREE_CHAIN (gnu_subprog_param))
    {
      if (DECL_BY_DESCRIPTOR_P (gnu_stub_param))
	{
	  gcc_assert (DECL_BY_REF_P (gnu_subprog_param));
	  gnu_param
	    = convert_vms_descriptor (TREE_TYPE (gnu_subprog_param),
				      gnu_stub_param,
				      DECL_PARM_ALT_TYPE (gnu_stub_param),
				      DECL_BY_DOUBLE_REF_P (gnu_subprog_param),
				      gnat_subprog);
	}
      else
	gnu_param = gnu_stub_param;

      VEC_safe_push (tree, gc, gnu_param_vec, gnu_param);
    }

  /* Invoke the internal subprogram.  */
  gnu_subprog_addr = build1 (ADDR_EXPR, build_pointer_type (gnu_subprog_type),
			     gnu_subprog);
  gnu_subprog_call = build_call_vec (TREE_TYPE (gnu_subprog_type),
                                     gnu_subprog_addr, gnu_param_vec);

  /* Propagate the return value, if any.  */
  if (VOID_TYPE_P (TREE_TYPE (gnu_subprog_type)))
    add_stmt (gnu_subprog_call);
  else
    add_stmt (build_return_expr (DECL_RESULT (gnu_stub_decl),
				 gnu_subprog_call));

  gnat_poplevel ();
  end_subprog_body (end_stmt_group ());
}

/* Build a type to be used to represent an aliased object whose nominal type
   is an unconstrained array.  This consists of a RECORD_TYPE containing a
   field of TEMPLATE_TYPE and a field of OBJECT_TYPE, which is an ARRAY_TYPE.
   If ARRAY_TYPE is that of an unconstrained array, this is used to represent
   an arbitrary unconstrained object.  Use NAME as the name of the record.
   DEBUG_INFO_P is true if we need to write debug information for the type.  */

tree
build_unc_object_type (tree template_type, tree object_type, tree name,
		       bool debug_info_p)
{
  tree type = make_node (RECORD_TYPE);
  tree template_field
    = create_field_decl (get_identifier ("BOUNDS"), template_type, type,
			 NULL_TREE, NULL_TREE, 0, 1);
  tree array_field
    = create_field_decl (get_identifier ("ARRAY"), object_type, type,
			 NULL_TREE, NULL_TREE, 0, 1);

  TYPE_NAME (type) = name;
  TYPE_CONTAINS_TEMPLATE_P (type) = 1;
  DECL_CHAIN (template_field) = array_field;
  finish_record_type (type, template_field, 0, true);

  /* Declare it now since it will never be declared otherwise.  This is
     necessary to ensure that its subtrees are properly marked.  */
  create_type_decl (name, type, NULL, true, debug_info_p, Empty);

  return type;
}

/* Same, taking a thin or fat pointer type instead of a template type. */

tree
build_unc_object_type_from_ptr (tree thin_fat_ptr_type, tree object_type,
				tree name, bool debug_info_p)
{
  tree template_type;

  gcc_assert (TYPE_IS_FAT_OR_THIN_POINTER_P (thin_fat_ptr_type));

  template_type
    = (TYPE_IS_FAT_POINTER_P (thin_fat_ptr_type)
       ? TREE_TYPE (TREE_TYPE (DECL_CHAIN (TYPE_FIELDS (thin_fat_ptr_type))))
       : TREE_TYPE (TYPE_FIELDS (TREE_TYPE (thin_fat_ptr_type))));

  return
    build_unc_object_type (template_type, object_type, name, debug_info_p);
}

/* Shift the component offsets within an unconstrained object TYPE to make it
   suitable for use as a designated type for thin pointers.  */

void
shift_unc_components_for_thin_pointers (tree type)
{
  /* Thin pointer values designate the ARRAY data of an unconstrained object,
     allocated past the BOUNDS template.  The designated type is adjusted to
     have ARRAY at position zero and the template at a negative offset, so
     that COMPONENT_REFs on (*thin_ptr) designate the proper location.  */

  tree bounds_field = TYPE_FIELDS (type);
  tree array_field  = DECL_CHAIN (TYPE_FIELDS (type));

  DECL_FIELD_OFFSET (bounds_field)
    = size_binop (MINUS_EXPR, size_zero_node, byte_position (array_field));

  DECL_FIELD_OFFSET (array_field) = size_zero_node;
  DECL_FIELD_BIT_OFFSET (array_field) = bitsize_zero_node;
}

/* Update anything previously pointing to OLD_TYPE to point to NEW_TYPE.
   In the normal case this is just two adjustments, but we have more to
   do if NEW_TYPE is an UNCONSTRAINED_ARRAY_TYPE.  */

void
update_pointer_to (tree old_type, tree new_type)
{
  tree ptr = TYPE_POINTER_TO (old_type);
  tree ref = TYPE_REFERENCE_TO (old_type);
  tree t;

  /* If this is the main variant, process all the other variants first.  */
  if (TYPE_MAIN_VARIANT (old_type) == old_type)
    for (t = TYPE_NEXT_VARIANT (old_type); t; t = TYPE_NEXT_VARIANT (t))
      update_pointer_to (t, new_type);

  /* If no pointers and no references, we are done.  */
  if (!ptr && !ref)
    return;

  /* Merge the old type qualifiers in the new type.

     Each old variant has qualifiers for specific reasons, and the new
     designated type as well.  Each set of qualifiers represents useful
     information grabbed at some point, and merging the two simply unifies
     these inputs into the final type description.

     Consider for instance a volatile type frozen after an access to constant
     type designating it; after the designated type's freeze, we get here with
     a volatile NEW_TYPE and a dummy OLD_TYPE with a readonly variant, created
     when the access type was processed.  We will make a volatile and readonly
     designated type, because that's what it really is.

     We might also get here for a non-dummy OLD_TYPE variant with different
     qualifiers than those of NEW_TYPE, for instance in some cases of pointers
     to private record type elaboration (see the comments around the call to
     this routine in gnat_to_gnu_entity <E_Access_Type>).  We have to merge
     the qualifiers in those cases too, to avoid accidentally discarding the
     initial set, and will often end up with OLD_TYPE == NEW_TYPE then.  */
  new_type
    = build_qualified_type (new_type,
			    TYPE_QUALS (old_type) | TYPE_QUALS (new_type));

  /* If old type and new type are identical, there is nothing to do.  */
  if (old_type == new_type)
    return;

  /* Otherwise, first handle the simple case.  */
  if (TREE_CODE (new_type) != UNCONSTRAINED_ARRAY_TYPE)
    {
      tree new_ptr, new_ref;

      /* If pointer or reference already points to new type, nothing to do.
	 This can happen as update_pointer_to can be invoked multiple times
	 on the same couple of types because of the type variants.  */
      if ((ptr && TREE_TYPE (ptr) == new_type)
	  || (ref && TREE_TYPE (ref) == new_type))
	return;

      /* Chain PTR and its variants at the end.  */
      new_ptr = TYPE_POINTER_TO (new_type);
      if (new_ptr)
	{
	  while (TYPE_NEXT_PTR_TO (new_ptr))
	    new_ptr = TYPE_NEXT_PTR_TO (new_ptr);
	  TYPE_NEXT_PTR_TO (new_ptr) = ptr;
	}
      else
	TYPE_POINTER_TO (new_type) = ptr;

      /* Now adjust them.  */
      for (; ptr; ptr = TYPE_NEXT_PTR_TO (ptr))
	for (t = TYPE_MAIN_VARIANT (ptr); t; t = TYPE_NEXT_VARIANT (t))
	  TREE_TYPE (t) = new_type;
      TYPE_POINTER_TO (old_type) = NULL_TREE;

      /* Chain REF and its variants at the end.  */
      new_ref = TYPE_REFERENCE_TO (new_type);
      if (new_ref)
	{
	  while (TYPE_NEXT_REF_TO (new_ref))
	    new_ref = TYPE_NEXT_REF_TO (new_ref);
	  TYPE_NEXT_REF_TO (new_ref) = ref;
	}
      else
	TYPE_REFERENCE_TO (new_type) = ref;

      /* Now adjust them.  */
      for (; ref; ref = TYPE_NEXT_REF_TO (ref))
	for (t = TYPE_MAIN_VARIANT (ref); t; t = TYPE_NEXT_VARIANT (t))
	  TREE_TYPE (t) = new_type;
      TYPE_REFERENCE_TO (old_type) = NULL_TREE;
    }

  /* Now deal with the unconstrained array case.  In this case the pointer
     is actually a record where both fields are pointers to dummy nodes.
     Turn them into pointers to the correct types using update_pointer_to.  */
  else
    {
      tree new_ptr = TYPE_MAIN_VARIANT (TYPE_POINTER_TO (new_type));
      tree new_obj_rec = TYPE_OBJECT_RECORD_TYPE (new_type);
      tree array_field, bounds_field, new_ref, last = NULL_TREE;

      gcc_assert (TYPE_IS_FAT_POINTER_P (ptr));

      /* If PTR already points to new type, nothing to do.  This can happen
	 since update_pointer_to can be invoked multiple times on the same
	 couple of types because of the type variants.  */
      if (TYPE_UNCONSTRAINED_ARRAY (ptr) == new_type)
	return;

      array_field = TYPE_FIELDS (ptr);
      bounds_field = DECL_CHAIN (array_field);

      /* Make pointers to the dummy template point to the real template.  */
      update_pointer_to
	(TREE_TYPE (TREE_TYPE (bounds_field)),
	 TREE_TYPE (TREE_TYPE (DECL_CHAIN (TYPE_FIELDS (new_ptr)))));

      /* The references to the template bounds present in the array type use
	 the bounds field of NEW_PTR through a PLACEHOLDER_EXPR.  Since we
	 are going to merge PTR in NEW_PTR, we must rework these references
	 to use the bounds field of PTR instead.  */
      new_ref = build3 (COMPONENT_REF, TREE_TYPE (bounds_field),
			build0 (PLACEHOLDER_EXPR, new_ptr),
			bounds_field, NULL_TREE);

      /* Create the new array for the new PLACEHOLDER_EXPR and make pointers
	 to the dummy array point to it.  */
      update_pointer_to
	(TREE_TYPE (TREE_TYPE (array_field)),
	 substitute_in_type (TREE_TYPE (TREE_TYPE (TYPE_FIELDS (new_ptr))),
			     DECL_CHAIN (TYPE_FIELDS (new_ptr)), new_ref));

      /* Merge PTR in NEW_PTR.  */
      DECL_FIELD_CONTEXT (array_field) = new_ptr;
      DECL_FIELD_CONTEXT (bounds_field) = new_ptr;
      for (t = new_ptr; t; last = t, t = TYPE_NEXT_VARIANT (t))
	TYPE_FIELDS (t) = TYPE_FIELDS (ptr);
      TYPE_ALIAS_SET (new_ptr) = TYPE_ALIAS_SET (ptr);

      /* Chain PTR and its variants at the end.  */
      TYPE_NEXT_VARIANT (last) = TYPE_MAIN_VARIANT (ptr);

      /* Now adjust them.  */
      for (t = TYPE_MAIN_VARIANT (ptr); t; t = TYPE_NEXT_VARIANT (t))
	{
	  TYPE_MAIN_VARIANT (t) = new_ptr;
	  SET_TYPE_UNCONSTRAINED_ARRAY (t, new_type);

	  /* And show the original pointer NEW_PTR to the debugger.  This is
	     the counterpart of the special processing for fat pointer types
	     in gnat_pushdecl, but when the unconstrained array type is only
	     frozen after access types to it.  */
	  if (TYPE_NAME (t) && TREE_CODE (TYPE_NAME (t)) == TYPE_DECL)
	    {
	      DECL_ORIGINAL_TYPE (TYPE_NAME (t)) = new_ptr;
	      DECL_ARTIFICIAL (TYPE_NAME (t)) = 0;
	    }
	}

      /* Now handle updating the allocation record, what the thin pointer
	 points to.  Update all pointers from the old record into the new
	 one, update the type of the array field, and recompute the size.  */
      update_pointer_to (TYPE_OBJECT_RECORD_TYPE (old_type), new_obj_rec);
      TREE_TYPE (DECL_CHAIN (TYPE_FIELDS (new_obj_rec)))
	= TREE_TYPE (TREE_TYPE (array_field));

      /* The size recomputation needs to account for alignment constraints, so
	 we let layout_type work it out.  This will reset the field offsets to
	 what they would be in a regular record, so we shift them back to what
	 we want them to be for a thin pointer designated type afterwards.  */
      DECL_SIZE (TYPE_FIELDS (new_obj_rec)) = NULL_TREE;
      DECL_SIZE (DECL_CHAIN (TYPE_FIELDS (new_obj_rec))) = NULL_TREE;
      TYPE_SIZE (new_obj_rec) = NULL_TREE;
      layout_type (new_obj_rec);
      shift_unc_components_for_thin_pointers (new_obj_rec);

      /* We are done, at last.  */
      rest_of_record_type_compilation (ptr);
    }
}

/* Convert EXPR, a pointer to a constrained array, into a pointer to an
   unconstrained one.  This involves making or finding a template.  */

static tree
convert_to_fat_pointer (tree type, tree expr)
{
  tree template_type = TREE_TYPE (TREE_TYPE (DECL_CHAIN (TYPE_FIELDS (type))));
  tree p_array_type = TREE_TYPE (TYPE_FIELDS (type));
  tree etype = TREE_TYPE (expr);
  tree template_tree;
  VEC(constructor_elt,gc) *v = VEC_alloc (constructor_elt, gc, 2);

  /* If EXPR is null, make a fat pointer that contains null pointers to the
     template and array.  */
  if (integer_zerop (expr))
    {
      CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (type),
			      convert (p_array_type, expr));
      CONSTRUCTOR_APPEND_ELT (v, DECL_CHAIN (TYPE_FIELDS (type)),
			      convert (build_pointer_type (template_type),
				       expr));
      return gnat_build_constructor (type, v);
    }

  /* If EXPR is a thin pointer, make template and data from the record..  */
  else if (TYPE_IS_THIN_POINTER_P (etype))
    {
      tree fields = TYPE_FIELDS (TREE_TYPE (etype));

      expr = gnat_protect_expr (expr);
      if (TREE_CODE (expr) == ADDR_EXPR)
	expr = TREE_OPERAND (expr, 0);
      else
	expr = build1 (INDIRECT_REF, TREE_TYPE (etype), expr);

      template_tree = build_component_ref (expr, NULL_TREE, fields, false);
      expr = build_unary_op (ADDR_EXPR, NULL_TREE,
			     build_component_ref (expr, NULL_TREE,
						  DECL_CHAIN (fields), false));
    }

  /* Otherwise, build the constructor for the template.  */
  else
    template_tree = build_template (template_type, TREE_TYPE (etype), expr);

  /* The final result is a constructor for the fat pointer.

     If EXPR is an argument of a foreign convention subprogram, the type it
     points to is directly the component type.  In this case, the expression
     type may not match the corresponding FIELD_DECL type at this point, so we
     call "convert" here to fix that up if necessary.  This type consistency is
     required, for instance because it ensures that possible later folding of
     COMPONENT_REFs against this constructor always yields something of the
     same type as the initial reference.

     Note that the call to "build_template" above is still fine because it
     will only refer to the provided TEMPLATE_TYPE in this case.  */
  CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (type),
			  convert (p_array_type, expr));
  CONSTRUCTOR_APPEND_ELT (v, DECL_CHAIN (TYPE_FIELDS (type)),
			  build_unary_op (ADDR_EXPR, NULL_TREE,
					  template_tree));
  return gnat_build_constructor (type, v);
}

/* Convert to a thin pointer type, TYPE.  The only thing we know how to convert
   is something that is a fat pointer, so convert to it first if it EXPR
   is not already a fat pointer.  */

static tree
convert_to_thin_pointer (tree type, tree expr)
{
  if (!TYPE_IS_FAT_POINTER_P (TREE_TYPE (expr)))
    expr
      = convert_to_fat_pointer
	(TREE_TYPE (TYPE_UNCONSTRAINED_ARRAY (TREE_TYPE (type))), expr);

  /* We get the pointer to the data and use a NOP_EXPR to make it the
     proper GCC type.  */
  expr = build_component_ref (expr, NULL_TREE, TYPE_FIELDS (TREE_TYPE (expr)),
			      false);
  expr = build1 (NOP_EXPR, type, expr);

  return expr;
}

/* Create an expression whose value is that of EXPR,
   converted to type TYPE.  The TREE_TYPE of the value
   is always TYPE.  This function implements all reasonable
   conversions; callers should filter out those that are
   not permitted by the language being compiled.  */

tree
convert (tree type, tree expr)
{
  tree etype = TREE_TYPE (expr);
  enum tree_code ecode = TREE_CODE (etype);
  enum tree_code code = TREE_CODE (type);

  /* If the expression is already of the right type, we are done.  */
  if (etype == type)
    return expr;

  /* If both input and output have padding and are of variable size, do this
     as an unchecked conversion.  Likewise if one is a mere variant of the
     other, so we avoid a pointless unpad/repad sequence.  */
  else if (code == RECORD_TYPE && ecode == RECORD_TYPE
	   && TYPE_PADDING_P (type) && TYPE_PADDING_P (etype)
	   && (!TREE_CONSTANT (TYPE_SIZE (type))
	       || !TREE_CONSTANT (TYPE_SIZE (etype))
	       || gnat_types_compatible_p (type, etype)
	       || TYPE_NAME (TREE_TYPE (TYPE_FIELDS (type)))
		  == TYPE_NAME (TREE_TYPE (TYPE_FIELDS (etype)))))
    ;

  /* If the output type has padding, convert to the inner type and make a
     constructor to build the record, unless a variable size is involved.  */
  else if (code == RECORD_TYPE && TYPE_PADDING_P (type))
    {
      VEC(constructor_elt,gc) *v;

      /* If we previously converted from another type and our type is
	 of variable size, remove the conversion to avoid the need for
	 variable-sized temporaries.  Likewise for a conversion between
	 original and packable version.  */
      if (TREE_CODE (expr) == VIEW_CONVERT_EXPR
	  && (!TREE_CONSTANT (TYPE_SIZE (type))
	      || (ecode == RECORD_TYPE
		  && TYPE_NAME (etype)
		     == TYPE_NAME (TREE_TYPE (TREE_OPERAND (expr, 0))))))
	expr = TREE_OPERAND (expr, 0);

      /* If we are just removing the padding from expr, convert the original
	 object if we have variable size in order to avoid the need for some
	 variable-sized temporaries.  Likewise if the padding is a variant
	 of the other, so we avoid a pointless unpad/repad sequence.  */
      if (TREE_CODE (expr) == COMPONENT_REF
	  && TYPE_IS_PADDING_P (TREE_TYPE (TREE_OPERAND (expr, 0)))
	  && (!TREE_CONSTANT (TYPE_SIZE (type))
	      || gnat_types_compatible_p (type,
					  TREE_TYPE (TREE_OPERAND (expr, 0)))
	      || (ecode == RECORD_TYPE
		  && TYPE_NAME (etype)
		     == TYPE_NAME (TREE_TYPE (TYPE_FIELDS (type))))))
	return convert (type, TREE_OPERAND (expr, 0));

      /* If the inner type is of self-referential size and the expression type
	 is a record, do this as an unchecked conversion.  But first pad the
	 expression if possible to have the same size on both sides.  */
      if (ecode == RECORD_TYPE
	  && CONTAINS_PLACEHOLDER_P (DECL_SIZE (TYPE_FIELDS (type))))
	{
	  if (TREE_CODE (TYPE_SIZE (etype)) == INTEGER_CST)
	    expr = convert (maybe_pad_type (etype, TYPE_SIZE (type), 0, Empty,
					    false, false, false, true),
			    expr);
	  return unchecked_convert (type, expr, false);
	}

      /* If we are converting between array types with variable size, do the
	 final conversion as an unchecked conversion, again to avoid the need
	 for some variable-sized temporaries.  If valid, this conversion is
	 very likely purely technical and without real effects.  */
      if (ecode == ARRAY_TYPE
	  && TREE_CODE (TREE_TYPE (TYPE_FIELDS (type))) == ARRAY_TYPE
	  && !TREE_CONSTANT (TYPE_SIZE (etype))
	  && !TREE_CONSTANT (TYPE_SIZE (type)))
	return unchecked_convert (type,
				  convert (TREE_TYPE (TYPE_FIELDS (type)),
					   expr),
				  false);

      v = VEC_alloc (constructor_elt, gc, 1);
      CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (type),
			      convert (TREE_TYPE (TYPE_FIELDS (type)), expr));
      return gnat_build_constructor (type, v);
    }

  /* If the input type has padding, remove it and convert to the output type.
     The conditions ordering is arranged to ensure that the output type is not
     a padding type here, as it is not clear whether the conversion would
     always be correct if this was to happen.  */
  else if (ecode == RECORD_TYPE && TYPE_PADDING_P (etype))
    {
      tree unpadded;

      /* If we have just converted to this padded type, just get the
	 inner expression.  */
      if (TREE_CODE (expr) == CONSTRUCTOR
	  && !VEC_empty (constructor_elt, CONSTRUCTOR_ELTS (expr))
	  && VEC_index (constructor_elt, CONSTRUCTOR_ELTS (expr), 0)->index
	     == TYPE_FIELDS (etype))
	unpadded
	  = VEC_index (constructor_elt, CONSTRUCTOR_ELTS (expr), 0)->value;

      /* Otherwise, build an explicit component reference.  */
      else
	unpadded
	  = build_component_ref (expr, NULL_TREE, TYPE_FIELDS (etype), false);

      return convert (type, unpadded);
    }

  /* If the input is a biased type, adjust first.  */
  if (ecode == INTEGER_TYPE && TYPE_BIASED_REPRESENTATION_P (etype))
    return convert (type, fold_build2 (PLUS_EXPR, TREE_TYPE (etype),
				       fold_convert (TREE_TYPE (etype),
						     expr),
				       TYPE_MIN_VALUE (etype)));

  /* If the input is a justified modular type, we need to extract the actual
     object before converting it to any other type with the exceptions of an
     unconstrained array or of a mere type variant.  It is useful to avoid the
     extraction and conversion in the type variant case because it could end
     up replacing a VAR_DECL expr by a constructor and we might be about the
     take the address of the result.  */
  if (ecode == RECORD_TYPE && TYPE_JUSTIFIED_MODULAR_P (etype)
      && code != UNCONSTRAINED_ARRAY_TYPE
      && TYPE_MAIN_VARIANT (type) != TYPE_MAIN_VARIANT (etype))
    return convert (type, build_component_ref (expr, NULL_TREE,
					       TYPE_FIELDS (etype), false));

  /* If converting to a type that contains a template, convert to the data
     type and then build the template. */
  if (code == RECORD_TYPE && TYPE_CONTAINS_TEMPLATE_P (type))
    {
      tree obj_type = TREE_TYPE (DECL_CHAIN (TYPE_FIELDS (type)));
      VEC(constructor_elt,gc) *v = VEC_alloc (constructor_elt, gc, 2);

      /* If the source already has a template, get a reference to the
	 associated array only, as we are going to rebuild a template
	 for the target type anyway.  */
      expr = maybe_unconstrained_array (expr);

      CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (type),
			      build_template (TREE_TYPE (TYPE_FIELDS (type)),
					      obj_type, NULL_TREE));
      CONSTRUCTOR_APPEND_ELT (v, DECL_CHAIN (TYPE_FIELDS (type)),
			      convert (obj_type, expr));
      return gnat_build_constructor (type, v);
    }

  /* There are some special cases of expressions that we process
     specially.  */
  switch (TREE_CODE (expr))
    {
    case ERROR_MARK:
      return expr;

    case NULL_EXPR:
      /* Just set its type here.  For TRANSFORM_EXPR, we will do the actual
	 conversion in gnat_expand_expr.  NULL_EXPR does not represent
	 and actual value, so no conversion is needed.  */
      expr = copy_node (expr);
      TREE_TYPE (expr) = type;
      return expr;

    case STRING_CST:
      /* If we are converting a STRING_CST to another constrained array type,
	 just make a new one in the proper type.  */
      if (code == ecode && AGGREGATE_TYPE_P (etype)
	  && !(TREE_CODE (TYPE_SIZE (etype)) == INTEGER_CST
	       && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST))
	{
	  expr = copy_node (expr);
	  TREE_TYPE (expr) = type;
	  return expr;
	}
      break;

    case VECTOR_CST:
      /* If we are converting a VECTOR_CST to a mere variant type, just make
	 a new one in the proper type.  */
      if (code == ecode && gnat_types_compatible_p (type, etype))
	{
	  expr = copy_node (expr);
	  TREE_TYPE (expr) = type;
	  return expr;
	}

    case CONSTRUCTOR:
      /* If we are converting a CONSTRUCTOR to a mere variant type, just make
	 a new one in the proper type.  */
      if (code == ecode && gnat_types_compatible_p (type, etype))
	{
	  expr = copy_node (expr);
	  TREE_TYPE (expr) = type;
	  return expr;
	}

      /* Likewise for a conversion between original and packable version, or
	 conversion between types of the same size and with the same list of
	 fields, but we have to work harder to preserve type consistency.  */
      if (code == ecode
	  && code == RECORD_TYPE
	  && (TYPE_NAME (type) == TYPE_NAME (etype)
	      || tree_int_cst_equal (TYPE_SIZE (type), TYPE_SIZE (etype))))

	{
	  VEC(constructor_elt,gc) *e = CONSTRUCTOR_ELTS (expr);
	  unsigned HOST_WIDE_INT len = VEC_length (constructor_elt, e);
	  VEC(constructor_elt,gc) *v = VEC_alloc (constructor_elt, gc, len);
	  tree efield = TYPE_FIELDS (etype), field = TYPE_FIELDS (type);
	  unsigned HOST_WIDE_INT idx;
	  tree index, value;

	  /* Whether we need to clear TREE_CONSTANT et al. on the output
	     constructor when we convert in place.  */
	  bool clear_constant = false;

	  FOR_EACH_CONSTRUCTOR_ELT(e, idx, index, value)
	    {
	      constructor_elt *elt;
	      /* We expect only simple constructors.  */
	      if (!SAME_FIELD_P (index, efield))
		break;
	      /* The field must be the same.  */
	      if (!SAME_FIELD_P (efield, field))
		break;
	      elt = VEC_quick_push (constructor_elt, v, NULL);
	      elt->index = field;
	      elt->value = convert (TREE_TYPE (field), value);

	      /* If packing has made this field a bitfield and the input
		 value couldn't be emitted statically any more, we need to
		 clear TREE_CONSTANT on our output.  */
	      if (!clear_constant
		  && TREE_CONSTANT (expr)
		  && !CONSTRUCTOR_BITFIELD_P (efield)
		  && CONSTRUCTOR_BITFIELD_P (field)
		  && !initializer_constant_valid_for_bitfield_p (value))
		clear_constant = true;

	      efield = DECL_CHAIN (efield);
	      field = DECL_CHAIN (field);
	    }

	  /* If we have been able to match and convert all the input fields
	     to their output type, convert in place now.  We'll fallback to a
	     view conversion downstream otherwise.  */
	  if (idx == len)
	    {
	      expr = copy_node (expr);
	      TREE_TYPE (expr) = type;
	      CONSTRUCTOR_ELTS (expr) = v;
	      if (clear_constant)
		TREE_CONSTANT (expr) = TREE_STATIC (expr) = 0;
	      return expr;
	    }
	}

      /* Likewise for a conversion between array type and vector type with a
         compatible representative array.  */
      else if (code == VECTOR_TYPE
	       && ecode == ARRAY_TYPE
	       && gnat_types_compatible_p (TYPE_REPRESENTATIVE_ARRAY (type),
					   etype))
	{
	  VEC(constructor_elt,gc) *e = CONSTRUCTOR_ELTS (expr);
	  unsigned HOST_WIDE_INT len = VEC_length (constructor_elt, e);
	  VEC(constructor_elt,gc) *v;
	  unsigned HOST_WIDE_INT ix;
	  tree value;

	  /* Build a VECTOR_CST from a *constant* array constructor.  */
	  if (TREE_CONSTANT (expr))
	    {
	      bool constant_p = true;

	      /* Iterate through elements and check if all constructor
		 elements are *_CSTs.  */
	      FOR_EACH_CONSTRUCTOR_VALUE (e, ix, value)
		if (!CONSTANT_CLASS_P (value))
		  {
		    constant_p = false;
		    break;
		  }

	      if (constant_p)
		return build_vector_from_ctor (type,
					       CONSTRUCTOR_ELTS (expr));
	    }

	  /* Otherwise, build a regular vector constructor.  */
	  v = VEC_alloc (constructor_elt, gc, len);
	  FOR_EACH_CONSTRUCTOR_VALUE (e, ix, value)
	    {
	      constructor_elt *elt = VEC_quick_push (constructor_elt, v, NULL);
	      elt->index = NULL_TREE;
	      elt->value = value;
	    }
	  expr = copy_node (expr);
	  TREE_TYPE (expr) = type;
	  CONSTRUCTOR_ELTS (expr) = v;
	  return expr;
	}
      break;

    case UNCONSTRAINED_ARRAY_REF:
      /* Convert this to the type of the inner array by getting the address of
	 the array from the template.  */
      expr = TREE_OPERAND (expr, 0);
      expr = build_unary_op (INDIRECT_REF, NULL_TREE,
			     build_component_ref (expr, NULL_TREE,
						  TYPE_FIELDS
						  (TREE_TYPE (expr)),
						  false));
      etype = TREE_TYPE (expr);
      ecode = TREE_CODE (etype);
      break;

    case VIEW_CONVERT_EXPR:
      {
	/* GCC 4.x is very sensitive to type consistency overall, and view
	   conversions thus are very frequent.  Even though just "convert"ing
	   the inner operand to the output type is fine in most cases, it
	   might expose unexpected input/output type mismatches in special
	   circumstances so we avoid such recursive calls when we can.  */
	tree op0 = TREE_OPERAND (expr, 0);

	/* If we are converting back to the original type, we can just
	   lift the input conversion.  This is a common occurrence with
	   switches back-and-forth amongst type variants.  */
	if (type == TREE_TYPE (op0))
	  return op0;

	/* Otherwise, if we're converting between two aggregate or vector
	   types, we might be allowed to substitute the VIEW_CONVERT_EXPR
	   target type in place or to just convert the inner expression.  */
	if ((AGGREGATE_TYPE_P (type) && AGGREGATE_TYPE_P (etype))
	    || (VECTOR_TYPE_P (type) && VECTOR_TYPE_P (etype)))
	  {
	    /* If we are converting between mere variants, we can just
	       substitute the VIEW_CONVERT_EXPR in place.  */
	    if (gnat_types_compatible_p (type, etype))
	      return build1 (VIEW_CONVERT_EXPR, type, op0);

	    /* Otherwise, we may just bypass the input view conversion unless
	       one of the types is a fat pointer,  which is handled by
	       specialized code below which relies on exact type matching.  */
	    else if (!TYPE_IS_FAT_POINTER_P (type)
		     && !TYPE_IS_FAT_POINTER_P (etype))
	      return convert (type, op0);
	  }
      }
      break;

    default:
      break;
    }

  /* Check for converting to a pointer to an unconstrained array.  */
  if (TYPE_IS_FAT_POINTER_P (type) && !TYPE_IS_FAT_POINTER_P (etype))
    return convert_to_fat_pointer (type, expr);

  /* If we are converting between two aggregate or vector types that are mere
     variants, just make a VIEW_CONVERT_EXPR.  Likewise when we are converting
     to a vector type from its representative array type.  */
  else if ((code == ecode
	    && (AGGREGATE_TYPE_P (type) || VECTOR_TYPE_P (type))
	    && gnat_types_compatible_p (type, etype))
	   || (code == VECTOR_TYPE
	       && ecode == ARRAY_TYPE
	       && gnat_types_compatible_p (TYPE_REPRESENTATIVE_ARRAY (type),
					   etype)))
    return build1 (VIEW_CONVERT_EXPR, type, expr);

  /* If we are converting between tagged types, try to upcast properly.  */
  else if (ecode == RECORD_TYPE && code == RECORD_TYPE
	   && TYPE_ALIGN_OK (etype) && TYPE_ALIGN_OK (type))
    {
      tree child_etype = etype;
      do {
	tree field = TYPE_FIELDS (child_etype);
	if (DECL_NAME (field) == parent_name_id && TREE_TYPE (field) == type)
	  return build_component_ref (expr, NULL_TREE, field, false);
	child_etype = TREE_TYPE (field);
      } while (TREE_CODE (child_etype) == RECORD_TYPE);
    }

  /* In all other cases of related types, make a NOP_EXPR.  */
  else if (TYPE_MAIN_VARIANT (type) == TYPE_MAIN_VARIANT (etype))
    return fold_convert (type, expr);

  switch (code)
    {
    case VOID_TYPE:
      return fold_build1 (CONVERT_EXPR, type, expr);

    case INTEGER_TYPE:
      if (TYPE_HAS_ACTUAL_BOUNDS_P (type)
	  && (ecode == ARRAY_TYPE || ecode == UNCONSTRAINED_ARRAY_TYPE
	      || (ecode == RECORD_TYPE && TYPE_CONTAINS_TEMPLATE_P (etype))))
	return unchecked_convert (type, expr, false);
      else if (TYPE_BIASED_REPRESENTATION_P (type))
	return fold_convert (type,
			     fold_build2 (MINUS_EXPR, TREE_TYPE (type),
					  convert (TREE_TYPE (type), expr),
					  TYPE_MIN_VALUE (type)));

      /* ... fall through ... */

    case ENUMERAL_TYPE:
    case BOOLEAN_TYPE:
      /* If we are converting an additive expression to an integer type
	 with lower precision, be wary of the optimization that can be
	 applied by convert_to_integer.  There are 2 problematic cases:
	   - if the first operand was originally of a biased type,
	     because we could be recursively called to convert it
	     to an intermediate type and thus rematerialize the
	     additive operator endlessly,
	   - if the expression contains a placeholder, because an
	     intermediate conversion that changes the sign could
	     be inserted and thus introduce an artificial overflow
	     at compile time when the placeholder is substituted.  */
      if (code == INTEGER_TYPE
	  && ecode == INTEGER_TYPE
	  && TYPE_PRECISION (type) < TYPE_PRECISION (etype)
	  && (TREE_CODE (expr) == PLUS_EXPR || TREE_CODE (expr) == MINUS_EXPR))
	{
	  tree op0 = get_unwidened (TREE_OPERAND (expr, 0), type);

	  if ((TREE_CODE (TREE_TYPE (op0)) == INTEGER_TYPE
	       && TYPE_BIASED_REPRESENTATION_P (TREE_TYPE (op0)))
	      || CONTAINS_PLACEHOLDER_P (expr))
	    return build1 (NOP_EXPR, type, expr);
	}

      return fold (convert_to_integer (type, expr));

    case POINTER_TYPE:
    case REFERENCE_TYPE:
      /* If converting between two pointers to records denoting
	 both a template and type, adjust if needed to account
	 for any differing offsets, since one might be negative.  */
      if (TYPE_IS_THIN_POINTER_P (etype) && TYPE_IS_THIN_POINTER_P (type))
	{
	  tree bit_diff
	    = size_diffop (bit_position (TYPE_FIELDS (TREE_TYPE (etype))),
			   bit_position (TYPE_FIELDS (TREE_TYPE (type))));
	  tree byte_diff
	    = size_binop (CEIL_DIV_EXPR, bit_diff, sbitsize_unit_node);
	  expr = build1 (NOP_EXPR, type, expr);
	  TREE_CONSTANT (expr) = TREE_CONSTANT (TREE_OPERAND (expr, 0));
	  if (integer_zerop (byte_diff))
	    return expr;

	  return build_binary_op (POINTER_PLUS_EXPR, type, expr,
				  fold (convert (sizetype, byte_diff)));
	}

      /* If converting to a thin pointer, handle specially.  */
      if (TYPE_IS_THIN_POINTER_P (type)
	  && TYPE_UNCONSTRAINED_ARRAY (TREE_TYPE (type)))
	return convert_to_thin_pointer (type, expr);

      /* If converting fat pointer to normal pointer, get the pointer to the
	 array and then convert it.  */
      else if (TYPE_IS_FAT_POINTER_P (etype))
	expr
	  = build_component_ref (expr, NULL_TREE, TYPE_FIELDS (etype), false);

      return fold (convert_to_pointer (type, expr));

    case REAL_TYPE:
      return fold (convert_to_real (type, expr));

    case RECORD_TYPE:
      if (TYPE_JUSTIFIED_MODULAR_P (type) && !AGGREGATE_TYPE_P (etype))
	{
	  VEC(constructor_elt,gc) *v = VEC_alloc (constructor_elt, gc, 1);

	  CONSTRUCTOR_APPEND_ELT (v, TYPE_FIELDS (type),
				  convert (TREE_TYPE (TYPE_FIELDS (type)),
					   expr));
	  return gnat_build_constructor (type, v);
	}

      /* ... fall through ... */

    case ARRAY_TYPE:
      /* In these cases, assume the front-end has validated the conversion.
	 If the conversion is valid, it will be a bit-wise conversion, so
	 it can be viewed as an unchecked conversion.  */
      return unchecked_convert (type, expr, false);

    case UNION_TYPE:
      /* This is a either a conversion between a tagged type and some
	 subtype, which we have to mark as a UNION_TYPE because of
	 overlapping fields or a conversion of an Unchecked_Union.  */
      return unchecked_convert (type, expr, false);

    case UNCONSTRAINED_ARRAY_TYPE:
      /* If the input is a VECTOR_TYPE, convert to the representative
	 array type first.  */
      if (ecode == VECTOR_TYPE)
	{
	  expr = convert (TYPE_REPRESENTATIVE_ARRAY (etype), expr);
	  etype = TREE_TYPE (expr);
	  ecode = TREE_CODE (etype);
	}

      /* If EXPR is a constrained array, take its address, convert it to a
	 fat pointer, and then dereference it.  Likewise if EXPR is a
	 record containing both a template and a constrained array.
	 Note that a record representing a justified modular type
	 always represents a packed constrained array.  */
      if (ecode == ARRAY_TYPE
	  || (ecode == INTEGER_TYPE && TYPE_HAS_ACTUAL_BOUNDS_P (etype))
	  || (ecode == RECORD_TYPE && TYPE_CONTAINS_TEMPLATE_P (etype))
	  || (ecode == RECORD_TYPE && TYPE_JUSTIFIED_MODULAR_P (etype)))
	return
	  build_unary_op
	    (INDIRECT_REF, NULL_TREE,
	     convert_to_fat_pointer (TREE_TYPE (type),
				     build_unary_op (ADDR_EXPR,
						     NULL_TREE, expr)));

      /* Do something very similar for converting one unconstrained
	 array to another.  */
      else if (ecode == UNCONSTRAINED_ARRAY_TYPE)
	return
	  build_unary_op (INDIRECT_REF, NULL_TREE,
			  convert (TREE_TYPE (type),
				   build_unary_op (ADDR_EXPR,
						   NULL_TREE, expr)));
      else
	gcc_unreachable ();

    case COMPLEX_TYPE:
      return fold (convert_to_complex (type, expr));

    default:
      gcc_unreachable ();
    }
}

/* Remove all conversions that are done in EXP.  This includes converting
   from a padded type or to a justified modular type.  If TRUE_ADDRESS
   is true, always return the address of the containing object even if
   the address is not bit-aligned.  */

tree
remove_conversions (tree exp, bool true_address)
{
  switch (TREE_CODE (exp))
    {
    case CONSTRUCTOR:
      if (true_address
	  && TREE_CODE (TREE_TYPE (exp)) == RECORD_TYPE
	  && TYPE_JUSTIFIED_MODULAR_P (TREE_TYPE (exp)))
	return
	  remove_conversions (VEC_index (constructor_elt,
					 CONSTRUCTOR_ELTS (exp), 0)->value,
			      true);
      break;

    case COMPONENT_REF:
      if (TYPE_IS_PADDING_P (TREE_TYPE (TREE_OPERAND (exp, 0))))
	return remove_conversions (TREE_OPERAND (exp, 0), true_address);
      break;

    case VIEW_CONVERT_EXPR:  case NON_LVALUE_EXPR:
    CASE_CONVERT:
      return remove_conversions (TREE_OPERAND (exp, 0), true_address);

    default:
      break;
    }

  return exp;
}

/* If EXP's type is an UNCONSTRAINED_ARRAY_TYPE, return an expression that
   refers to the underlying array.  If it has TYPE_CONTAINS_TEMPLATE_P,
   likewise return an expression pointing to the underlying array.  */

tree
maybe_unconstrained_array (tree exp)
{
  enum tree_code code = TREE_CODE (exp);
  tree new_exp;

  switch (TREE_CODE (TREE_TYPE (exp)))
    {
    case UNCONSTRAINED_ARRAY_TYPE:
      if (code == UNCONSTRAINED_ARRAY_REF)
	{
	  new_exp = TREE_OPERAND (exp, 0);
	  new_exp
	    = build_unary_op (INDIRECT_REF, NULL_TREE,
			      build_component_ref (new_exp, NULL_TREE,
						   TYPE_FIELDS
						   (TREE_TYPE (new_exp)),
						   false));
	  TREE_READONLY (new_exp) = TREE_READONLY (exp);
	  return new_exp;
	}

      else if (code == NULL_EXPR)
	return build1 (NULL_EXPR,
		       TREE_TYPE (TREE_TYPE (TYPE_FIELDS
					     (TREE_TYPE (TREE_TYPE (exp))))),
		       TREE_OPERAND (exp, 0));

    case RECORD_TYPE:
      /* If this is a padded type, convert to the unpadded type and see if
	 it contains a template.  */
      if (TYPE_PADDING_P (TREE_TYPE (exp)))
	{
	  new_exp = convert (TREE_TYPE (TYPE_FIELDS (TREE_TYPE (exp))), exp);
	  if (TREE_CODE (TREE_TYPE (new_exp)) == RECORD_TYPE
	      && TYPE_CONTAINS_TEMPLATE_P (TREE_TYPE (new_exp)))
	    return
	      build_component_ref (new_exp, NULL_TREE,
				   DECL_CHAIN
				   (TYPE_FIELDS (TREE_TYPE (new_exp))),
				   false);
	}
      else if (TYPE_CONTAINS_TEMPLATE_P (TREE_TYPE (exp)))
	return
	  build_component_ref (exp, NULL_TREE,
			       DECL_CHAIN (TYPE_FIELDS (TREE_TYPE (exp))),
			       false);
      break;

    default:
      break;
    }

  return exp;
}

/* If EXP's type is a VECTOR_TYPE, return EXP converted to the associated
   TYPE_REPRESENTATIVE_ARRAY.  */

tree
maybe_vector_array (tree exp)
{
  tree etype = TREE_TYPE (exp);

  if (VECTOR_TYPE_P (etype))
    exp = convert (TYPE_REPRESENTATIVE_ARRAY (etype), exp);

  return exp;
}

/* Return true if EXPR is an expression that can be folded as an operand
   of a VIEW_CONVERT_EXPR.  See ada-tree.h for a complete rationale.  */

static bool
can_fold_for_view_convert_p (tree expr)
{
  tree t1, t2;

  /* The folder will fold NOP_EXPRs between integral types with the same
     precision (in the middle-end's sense).  We cannot allow it if the
     types don't have the same precision in the Ada sense as well.  */
  if (TREE_CODE (expr) != NOP_EXPR)
    return true;

  t1 = TREE_TYPE (expr);
  t2 = TREE_TYPE (TREE_OPERAND (expr, 0));

  /* Defer to the folder for non-integral conversions.  */
  if (!(INTEGRAL_TYPE_P (t1) && INTEGRAL_TYPE_P (t2)))
    return true;

  /* Only fold conversions that preserve both precisions.  */
  if (TYPE_PRECISION (t1) == TYPE_PRECISION (t2)
      && operand_equal_p (rm_size (t1), rm_size (t2), 0))
    return true;

  return false;
}

/* Return an expression that does an unchecked conversion of EXPR to TYPE.
   If NOTRUNC_P is true, truncation operations should be suppressed.

   Special care is required with (source or target) integral types whose
   precision is not equal to their size, to make sure we fetch or assign
   the value bits whose location might depend on the endianness, e.g.

     Rmsize : constant := 8;
     subtype Int is Integer range 0 .. 2 ** Rmsize - 1;

     type Bit_Array is array (1 .. Rmsize) of Boolean;
     pragma Pack (Bit_Array);

     function To_Bit_Array is new Unchecked_Conversion (Int, Bit_Array);

     Value : Int := 2#1000_0001#;
     Vbits : Bit_Array := To_Bit_Array (Value);

   we expect the 8 bits at Vbits'Address to always contain Value, while
   their original location depends on the endianness, at Value'Address
   on a little-endian architecture but not on a big-endian one.  */

tree
unchecked_convert (tree type, tree expr, bool notrunc_p)
{
  tree etype = TREE_TYPE (expr);
  enum tree_code ecode = TREE_CODE (etype);
  enum tree_code code = TREE_CODE (type);
  int c;

  /* If the expression is already of the right type, we are done.  */
  if (etype == type)
    return expr;

  /* If both types types are integral just do a normal conversion.
     Likewise for a conversion to an unconstrained array.  */
  if ((((INTEGRAL_TYPE_P (type)
	 && !(code == INTEGER_TYPE && TYPE_VAX_FLOATING_POINT_P (type)))
	|| (POINTER_TYPE_P (type) && ! TYPE_IS_THIN_POINTER_P (type))
	|| (code == RECORD_TYPE && TYPE_JUSTIFIED_MODULAR_P (type)))
       && ((INTEGRAL_TYPE_P (etype)
	    && !(ecode == INTEGER_TYPE && TYPE_VAX_FLOATING_POINT_P (etype)))
	   || (POINTER_TYPE_P (etype) && !TYPE_IS_THIN_POINTER_P (etype))
	   || (ecode == RECORD_TYPE && TYPE_JUSTIFIED_MODULAR_P (etype))))
      || code == UNCONSTRAINED_ARRAY_TYPE)
    {
      if (ecode == INTEGER_TYPE && TYPE_BIASED_REPRESENTATION_P (etype))
	{
	  tree ntype = copy_type (etype);
	  TYPE_BIASED_REPRESENTATION_P (ntype) = 0;
	  TYPE_MAIN_VARIANT (ntype) = ntype;
	  expr = build1 (NOP_EXPR, ntype, expr);
	}

      if (code == INTEGER_TYPE && TYPE_BIASED_REPRESENTATION_P (type))
	{
	  tree rtype = copy_type (type);
	  TYPE_BIASED_REPRESENTATION_P (rtype) = 0;
	  TYPE_MAIN_VARIANT (rtype) = rtype;
	  expr = convert (rtype, expr);
	  expr = build1 (NOP_EXPR, type, expr);
	}
      else
	expr = convert (type, expr);
    }

  /* If we are converting to an integral type whose precision is not equal
     to its size, first unchecked convert to a record that contains an
     object of the output type.  Then extract the field. */
  else if (INTEGRAL_TYPE_P (type)
	   && TYPE_RM_SIZE (type)
	   && 0 != compare_tree_int (TYPE_RM_SIZE (type),
				     GET_MODE_BITSIZE (TYPE_MODE (type))))
    {
      tree rec_type = make_node (RECORD_TYPE);
      tree field = create_field_decl (get_identifier ("OBJ"), type, rec_type,
				      NULL_TREE, NULL_TREE, 1, 0);

      TYPE_FIELDS (rec_type) = field;
      layout_type (rec_type);

      expr = unchecked_convert (rec_type, expr, notrunc_p);
      expr = build_component_ref (expr, NULL_TREE, field, false);
    }

  /* Similarly if we are converting from an integral type whose precision
     is not equal to its size.  */
  else if (INTEGRAL_TYPE_P (etype)
	   && TYPE_RM_SIZE (etype)
	   && 0 != compare_tree_int (TYPE_RM_SIZE (etype),
				     GET_MODE_BITSIZE (TYPE_MODE (etype))))
    {
      tree rec_type = make_node (RECORD_TYPE);
      tree field = create_field_decl (get_identifier ("OBJ"), etype, rec_type,
				      NULL_TREE, NULL_TREE, 1, 0);
      VEC(constructor_elt,gc) *v = VEC_alloc (constructor_elt, gc, 1);

      TYPE_FIELDS (rec_type) = field;
      layout_type (rec_type);

      CONSTRUCTOR_APPEND_ELT (v, field, expr);
      expr = gnat_build_constructor (rec_type, v);
      expr = unchecked_convert (type, expr, notrunc_p);
    }

  /* If we are converting from a scalar type to a type with a different size,
     we need to pad to have the same size on both sides.

     ??? We cannot do it unconditionally because unchecked conversions are
     used liberally by the front-end to implement polymorphism, e.g. in:

       S191s : constant ada__tags__addr_ptr := ada__tags__addr_ptr!(S190s);
       return p___size__4 (p__object!(S191s.all));

     so we skip all expressions that are references.  */
  else if (!REFERENCE_CLASS_P (expr)
	   && !AGGREGATE_TYPE_P (etype)
	   && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST
	   && (c = tree_int_cst_compare (TYPE_SIZE (etype), TYPE_SIZE (type))))
    {
      if (c < 0)
	{
	  expr = convert (maybe_pad_type (etype, TYPE_SIZE (type), 0, Empty,
					  false, false, false, true),
			  expr);
	  expr = unchecked_convert (type, expr, notrunc_p);
	}
      else
	{
	  tree rec_type = maybe_pad_type (type, TYPE_SIZE (etype), 0, Empty,
					  false, false, false, true);
	  expr = unchecked_convert (rec_type, expr, notrunc_p);
	  expr = build_component_ref (expr, NULL_TREE, TYPE_FIELDS (rec_type),
				      false);
	}
    }

  /* We have a special case when we are converting between two unconstrained
     array types.  In that case, take the address, convert the fat pointer
     types, and dereference.  */
  else if (ecode == code && code == UNCONSTRAINED_ARRAY_TYPE)
    expr = build_unary_op (INDIRECT_REF, NULL_TREE,
			   build1 (VIEW_CONVERT_EXPR, TREE_TYPE (type),
				   build_unary_op (ADDR_EXPR, NULL_TREE,
						   expr)));

  /* Another special case is when we are converting to a vector type from its
     representative array type; this a regular conversion.  */
  else if (code == VECTOR_TYPE
	   && ecode == ARRAY_TYPE
	   && gnat_types_compatible_p (TYPE_REPRESENTATIVE_ARRAY (type),
				       etype))
    expr = convert (type, expr);

  else
    {
      expr = maybe_unconstrained_array (expr);
      etype = TREE_TYPE (expr);
      ecode = TREE_CODE (etype);
      if (can_fold_for_view_convert_p (expr))
	expr = fold_build1 (VIEW_CONVERT_EXPR, type, expr);
      else
	expr = build1 (VIEW_CONVERT_EXPR, type, expr);
    }

  /* If the result is an integral type whose precision is not equal to its
     size, sign- or zero-extend the result.  We need not do this if the input
     is an integral type of the same precision and signedness or if the output
     is a biased type or if both the input and output are unsigned.  */
  if (!notrunc_p
      && INTEGRAL_TYPE_P (type) && TYPE_RM_SIZE (type)
      && !(code == INTEGER_TYPE && TYPE_BIASED_REPRESENTATION_P (type))
      && 0 != compare_tree_int (TYPE_RM_SIZE (type),
				GET_MODE_BITSIZE (TYPE_MODE (type)))
      && !(INTEGRAL_TYPE_P (etype)
	   && TYPE_UNSIGNED (type) == TYPE_UNSIGNED (etype)
	   && operand_equal_p (TYPE_RM_SIZE (type),
			       (TYPE_RM_SIZE (etype) != 0
				? TYPE_RM_SIZE (etype) : TYPE_SIZE (etype)),
			       0))
      && !(TYPE_UNSIGNED (type) && TYPE_UNSIGNED (etype)))
    {
      tree base_type
	= gnat_type_for_mode (TYPE_MODE (type), TYPE_UNSIGNED (type));
      tree shift_expr
	= convert (base_type,
		   size_binop (MINUS_EXPR,
			       bitsize_int
			       (GET_MODE_BITSIZE (TYPE_MODE (type))),
			       TYPE_RM_SIZE (type)));
      expr
	= convert (type,
		   build_binary_op (RSHIFT_EXPR, base_type,
				    build_binary_op (LSHIFT_EXPR, base_type,
						     convert (base_type, expr),
						     shift_expr),
				    shift_expr));
    }

  /* An unchecked conversion should never raise Constraint_Error.  The code
     below assumes that GCC's conversion routines overflow the same way that
     the underlying hardware does.  This is probably true.  In the rare case
     when it is false, we can rely on the fact that such conversions are
     erroneous anyway.  */
  if (TREE_CODE (expr) == INTEGER_CST)
    TREE_OVERFLOW (expr) = 0;

  /* If the sizes of the types differ and this is an VIEW_CONVERT_EXPR,
     show no longer constant.  */
  if (TREE_CODE (expr) == VIEW_CONVERT_EXPR
      && !operand_equal_p (TYPE_SIZE_UNIT (type), TYPE_SIZE_UNIT (etype),
			   OEP_ONLY_CONST))
    TREE_CONSTANT (expr) = 0;

  return expr;
}

/* Return the appropriate GCC tree code for the specified GNAT_TYPE,
   the latter being a record type as predicated by Is_Record_Type.  */

enum tree_code
tree_code_for_record_type (Entity_Id gnat_type)
{
  Node_Id component_list
    = Component_List (Type_Definition
		      (Declaration_Node
		       (Implementation_Base_Type (gnat_type))));
  Node_Id component;

 /* Make this a UNION_TYPE unless it's either not an Unchecked_Union or
    we have a non-discriminant field outside a variant.  In either case,
    it's a RECORD_TYPE.  */

  if (!Is_Unchecked_Union (gnat_type))
    return RECORD_TYPE;

  for (component = First_Non_Pragma (Component_Items (component_list));
       Present (component);
       component = Next_Non_Pragma (component))
    if (Ekind (Defining_Entity (component)) == E_Component)
      return RECORD_TYPE;

  return UNION_TYPE;
}

/* Return true if GNAT_TYPE is a "double" floating-point type, i.e. whose
   size is equal to 64 bits, or an array of such a type.  Set ALIGN_CLAUSE
   according to the presence of an alignment clause on the type or, if it
   is an array, on the component type.  */

bool
is_double_float_or_array (Entity_Id gnat_type, bool *align_clause)
{
  gnat_type = Underlying_Type (gnat_type);

  *align_clause = Present (Alignment_Clause (gnat_type));

  if (Is_Array_Type (gnat_type))
    {
      gnat_type = Underlying_Type (Component_Type (gnat_type));
      if (Present (Alignment_Clause (gnat_type)))
	*align_clause = true;
    }

  if (!Is_Floating_Point_Type (gnat_type))
    return false;

  if (UI_To_Int (Esize (gnat_type)) != 64)
    return false;

  return true;
}

/* Return true if GNAT_TYPE is a "double" or larger scalar type, i.e. whose
   size is greater or equal to 64 bits, or an array of such a type.  Set
   ALIGN_CLAUSE according to the presence of an alignment clause on the
   type or, if it is an array, on the component type.  */

bool
is_double_scalar_or_array (Entity_Id gnat_type, bool *align_clause)
{
  gnat_type = Underlying_Type (gnat_type);

  *align_clause = Present (Alignment_Clause (gnat_type));

  if (Is_Array_Type (gnat_type))
    {
      gnat_type = Underlying_Type (Component_Type (gnat_type));
      if (Present (Alignment_Clause (gnat_type)))
	*align_clause = true;
    }

  if (!Is_Scalar_Type (gnat_type))
    return false;

  if (UI_To_Int (Esize (gnat_type)) < 64)
    return false;

  return true;
}

/* Return true if GNU_TYPE is suitable as the type of a non-aliased
   component of an aggregate type.  */

bool
type_for_nonaliased_component_p (tree gnu_type)
{
  /* If the type is passed by reference, we may have pointers to the
     component so it cannot be made non-aliased. */
  if (must_pass_by_ref (gnu_type) || default_pass_by_ref (gnu_type))
    return false;

  /* We used to say that any component of aggregate type is aliased
     because the front-end may take 'Reference of it.  The front-end
     has been enhanced in the meantime so as to use a renaming instead
     in most cases, but the back-end can probably take the address of
     such a component too so we go for the conservative stance.

     For instance, we might need the address of any array type, even
     if normally passed by copy, to construct a fat pointer if the
     component is used as an actual for an unconstrained formal.

     Likewise for record types: even if a specific record subtype is
     passed by copy, the parent type might be passed by ref (e.g. if
     it's of variable size) and we might take the address of a child
     component to pass to a parent formal.  We have no way to check
     for such conditions here.  */
  if (AGGREGATE_TYPE_P (gnu_type))
    return false;

  return true;
}

/* Perform final processing on global variables.  */

void
gnat_write_global_declarations (void)
{
  /* Proceed to optimize and emit assembly.
     FIXME: shouldn't be the front end's responsibility to call this.  */
  cgraph_finalize_compilation_unit ();

  /* Emit debug info for all global declarations.  */
  emit_debug_global_declarations (VEC_address (tree, global_decls),
				  VEC_length (tree, global_decls));
}

/* ************************************************************************
 * *                           GCC builtins support                       *
 * ************************************************************************ */

/* The general scheme is fairly simple:

   For each builtin function/type to be declared, gnat_install_builtins calls
   internal facilities which eventually get to gnat_push_decl, which in turn
   tracks the so declared builtin function decls in the 'builtin_decls' global
   datastructure. When an Intrinsic subprogram declaration is processed, we
   search this global datastructure to retrieve the associated BUILT_IN DECL
   node.  */

/* Search the chain of currently available builtin declarations for a node
   corresponding to function NAME (an IDENTIFIER_NODE).  Return the first node
   found, if any, or NULL_TREE otherwise.  */
tree
builtin_decl_for (tree name)
{
  unsigned i;
  tree decl;

  FOR_EACH_VEC_ELT (tree, builtin_decls, i, decl)
    if (DECL_NAME (decl) == name)
      return decl;

  return NULL_TREE;
}

/* The code below eventually exposes gnat_install_builtins, which declares
   the builtin types and functions we might need, either internally or as
   user accessible facilities.

   ??? This is a first implementation shot, still in rough shape.  It is
   heavily inspired from the "C" family implementation, with chunks copied
   verbatim from there.

   Two obvious TODO candidates are
   o Use a more efficient name/decl mapping scheme
   o Devise a middle-end infrastructure to avoid having to copy
     pieces between front-ends.  */

/* ----------------------------------------------------------------------- *
 *                         BUILTIN ELEMENTARY TYPES                        *
 * ----------------------------------------------------------------------- */

/* Standard data types to be used in builtin argument declarations.  */

enum c_tree_index
{
    CTI_SIGNED_SIZE_TYPE, /* For format checking only.  */
    CTI_STRING_TYPE,
    CTI_CONST_STRING_TYPE,

    CTI_MAX
};

static tree c_global_trees[CTI_MAX];

#define signed_size_type_node	c_global_trees[CTI_SIGNED_SIZE_TYPE]
#define string_type_node	c_global_trees[CTI_STRING_TYPE]
#define const_string_type_node	c_global_trees[CTI_CONST_STRING_TYPE]

/* ??? In addition some attribute handlers, we currently don't support a
   (small) number of builtin-types, which in turns inhibits support for a
   number of builtin functions.  */
#define wint_type_node    void_type_node
#define intmax_type_node  void_type_node
#define uintmax_type_node void_type_node

/* Build the void_list_node (void_type_node having been created).  */

static tree
build_void_list_node (void)
{
  tree t = build_tree_list (NULL_TREE, void_type_node);
  return t;
}

/* Used to help initialize the builtin-types.def table.  When a type of
   the correct size doesn't exist, use error_mark_node instead of NULL.
   The later results in segfaults even when a decl using the type doesn't
   get invoked.  */

static tree
builtin_type_for_size (int size, bool unsignedp)
{
  tree type = gnat_type_for_size (size, unsignedp);
  return type ? type : error_mark_node;
}

/* Build/push the elementary type decls that builtin functions/types
   will need.  */

static void
install_builtin_elementary_types (void)
{
  signed_size_type_node = gnat_signed_type (size_type_node);
  pid_type_node = integer_type_node;
  void_list_node = build_void_list_node ();

  string_type_node = build_pointer_type (char_type_node);
  const_string_type_node
    = build_pointer_type (build_qualified_type
			  (char_type_node, TYPE_QUAL_CONST));
}

/* ----------------------------------------------------------------------- *
 *                          BUILTIN FUNCTION TYPES                         *
 * ----------------------------------------------------------------------- */

/* Now, builtin function types per se.  */

enum c_builtin_type
{
#define DEF_PRIMITIVE_TYPE(NAME, VALUE) NAME,
#define DEF_FUNCTION_TYPE_0(NAME, RETURN) NAME,
#define DEF_FUNCTION_TYPE_1(NAME, RETURN, ARG1) NAME,
#define DEF_FUNCTION_TYPE_2(NAME, RETURN, ARG1, ARG2) NAME,
#define DEF_FUNCTION_TYPE_3(NAME, RETURN, ARG1, ARG2, ARG3) NAME,
#define DEF_FUNCTION_TYPE_4(NAME, RETURN, ARG1, ARG2, ARG3, ARG4) NAME,
#define DEF_FUNCTION_TYPE_5(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5) NAME,
#define DEF_FUNCTION_TYPE_6(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6) NAME,
#define DEF_FUNCTION_TYPE_7(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) NAME,
#define DEF_FUNCTION_TYPE_VAR_0(NAME, RETURN) NAME,
#define DEF_FUNCTION_TYPE_VAR_1(NAME, RETURN, ARG1) NAME,
#define DEF_FUNCTION_TYPE_VAR_2(NAME, RETURN, ARG1, ARG2) NAME,
#define DEF_FUNCTION_TYPE_VAR_3(NAME, RETURN, ARG1, ARG2, ARG3) NAME,
#define DEF_FUNCTION_TYPE_VAR_4(NAME, RETURN, ARG1, ARG2, ARG3, ARG4) NAME,
#define DEF_FUNCTION_TYPE_VAR_5(NAME, RETURN, ARG1, ARG2, ARG3, ARG4, ARG6) \
  NAME,
#define DEF_POINTER_TYPE(NAME, TYPE) NAME,
#include "builtin-types.def"
#undef DEF_PRIMITIVE_TYPE
#undef DEF_FUNCTION_TYPE_0
#undef DEF_FUNCTION_TYPE_1
#undef DEF_FUNCTION_TYPE_2
#undef DEF_FUNCTION_TYPE_3
#undef DEF_FUNCTION_TYPE_4
#undef DEF_FUNCTION_TYPE_5
#undef DEF_FUNCTION_TYPE_6
#undef DEF_FUNCTION_TYPE_7
#undef DEF_FUNCTION_TYPE_VAR_0
#undef DEF_FUNCTION_TYPE_VAR_1
#undef DEF_FUNCTION_TYPE_VAR_2
#undef DEF_FUNCTION_TYPE_VAR_3
#undef DEF_FUNCTION_TYPE_VAR_4
#undef DEF_FUNCTION_TYPE_VAR_5
#undef DEF_POINTER_TYPE
  BT_LAST
};

typedef enum c_builtin_type builtin_type;

/* A temporary array used in communication with def_fn_type.  */
static GTY(()) tree builtin_types[(int) BT_LAST + 1];

/* A helper function for install_builtin_types.  Build function type
   for DEF with return type RET and N arguments.  If VAR is true, then the
   function should be variadic after those N arguments.

   Takes special care not to ICE if any of the types involved are
   error_mark_node, which indicates that said type is not in fact available
   (see builtin_type_for_size).  In which case the function type as a whole
   should be error_mark_node.  */

static void
def_fn_type (builtin_type def, builtin_type ret, bool var, int n, ...)
{
  tree args = NULL, t;
  va_list list;
  int i;

  va_start (list, n);
  for (i = 0; i < n; ++i)
    {
      builtin_type a = (builtin_type) va_arg (list, int);
      t = builtin_types[a];
      if (t == error_mark_node)
	goto egress;
      args = tree_cons (NULL_TREE, t, args);
    }
  va_end (list);

  args = nreverse (args);
  if (!var)
    args = chainon (args, void_list_node);

  t = builtin_types[ret];
  if (t == error_mark_node)
    goto egress;
  t = build_function_type (t, args);

 egress:
  builtin_types[def] = t;
}

/* Build the builtin function types and install them in the builtin_types
   array for later use in builtin function decls.  */

static void
install_builtin_function_types (void)
{
  tree va_list_ref_type_node;
  tree va_list_arg_type_node;

  if (TREE_CODE (va_list_type_node) == ARRAY_TYPE)
    {
      va_list_arg_type_node = va_list_ref_type_node =
	build_pointer_type (TREE_TYPE (va_list_type_node));
    }
  else
    {
      va_list_arg_type_node = va_list_type_node;
      va_list_ref_type_node = build_reference_type (va_list_type_node);
    }

#define DEF_PRIMITIVE_TYPE(ENUM, VALUE) \
  builtin_types[ENUM] = VALUE;
#define DEF_FUNCTION_TYPE_0(ENUM, RETURN) \
  def_fn_type (ENUM, RETURN, 0, 0);
#define DEF_FUNCTION_TYPE_1(ENUM, RETURN, ARG1) \
  def_fn_type (ENUM, RETURN, 0, 1, ARG1);
#define DEF_FUNCTION_TYPE_2(ENUM, RETURN, ARG1, ARG2) \
  def_fn_type (ENUM, RETURN, 0, 2, ARG1, ARG2);
#define DEF_FUNCTION_TYPE_3(ENUM, RETURN, ARG1, ARG2, ARG3) \
  def_fn_type (ENUM, RETURN, 0, 3, ARG1, ARG2, ARG3);
#define DEF_FUNCTION_TYPE_4(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4) \
  def_fn_type (ENUM, RETURN, 0, 4, ARG1, ARG2, ARG3, ARG4);
#define DEF_FUNCTION_TYPE_5(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5)	\
  def_fn_type (ENUM, RETURN, 0, 5, ARG1, ARG2, ARG3, ARG4, ARG5);
#define DEF_FUNCTION_TYPE_6(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
			    ARG6)					\
  def_fn_type (ENUM, RETURN, 0, 6, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);
#define DEF_FUNCTION_TYPE_7(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5, \
			    ARG6, ARG7)					\
  def_fn_type (ENUM, RETURN, 0, 7, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7);
#define DEF_FUNCTION_TYPE_VAR_0(ENUM, RETURN) \
  def_fn_type (ENUM, RETURN, 1, 0);
#define DEF_FUNCTION_TYPE_VAR_1(ENUM, RETURN, ARG1) \
  def_fn_type (ENUM, RETURN, 1, 1, ARG1);
#define DEF_FUNCTION_TYPE_VAR_2(ENUM, RETURN, ARG1, ARG2) \
  def_fn_type (ENUM, RETURN, 1, 2, ARG1, ARG2);
#define DEF_FUNCTION_TYPE_VAR_3(ENUM, RETURN, ARG1, ARG2, ARG3) \
  def_fn_type (ENUM, RETURN, 1, 3, ARG1, ARG2, ARG3);
#define DEF_FUNCTION_TYPE_VAR_4(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4) \
  def_fn_type (ENUM, RETURN, 1, 4, ARG1, ARG2, ARG3, ARG4);
#define DEF_FUNCTION_TYPE_VAR_5(ENUM, RETURN, ARG1, ARG2, ARG3, ARG4, ARG5) \
  def_fn_type (ENUM, RETURN, 1, 5, ARG1, ARG2, ARG3, ARG4, ARG5);
#define DEF_POINTER_TYPE(ENUM, TYPE) \
  builtin_types[(int) ENUM] = build_pointer_type (builtin_types[(int) TYPE]);

#include "builtin-types.def"

#undef DEF_PRIMITIVE_TYPE
#undef DEF_FUNCTION_TYPE_1
#undef DEF_FUNCTION_TYPE_2
#undef DEF_FUNCTION_TYPE_3
#undef DEF_FUNCTION_TYPE_4
#undef DEF_FUNCTION_TYPE_5
#undef DEF_FUNCTION_TYPE_6
#undef DEF_FUNCTION_TYPE_VAR_0
#undef DEF_FUNCTION_TYPE_VAR_1
#undef DEF_FUNCTION_TYPE_VAR_2
#undef DEF_FUNCTION_TYPE_VAR_3
#undef DEF_FUNCTION_TYPE_VAR_4
#undef DEF_FUNCTION_TYPE_VAR_5
#undef DEF_POINTER_TYPE
  builtin_types[(int) BT_LAST] = NULL_TREE;
}

/* ----------------------------------------------------------------------- *
 *                            BUILTIN ATTRIBUTES                           *
 * ----------------------------------------------------------------------- */

enum built_in_attribute
{
#define DEF_ATTR_NULL_TREE(ENUM) ENUM,
#define DEF_ATTR_INT(ENUM, VALUE) ENUM,
#define DEF_ATTR_IDENT(ENUM, STRING) ENUM,
#define DEF_ATTR_TREE_LIST(ENUM, PURPOSE, VALUE, CHAIN) ENUM,
#include "builtin-attrs.def"
#undef DEF_ATTR_NULL_TREE
#undef DEF_ATTR_INT
#undef DEF_ATTR_IDENT
#undef DEF_ATTR_TREE_LIST
  ATTR_LAST
};

static GTY(()) tree built_in_attributes[(int) ATTR_LAST];

static void
install_builtin_attributes (void)
{
  /* Fill in the built_in_attributes array.  */
#define DEF_ATTR_NULL_TREE(ENUM)				\
  built_in_attributes[(int) ENUM] = NULL_TREE;
#define DEF_ATTR_INT(ENUM, VALUE)				\
  built_in_attributes[(int) ENUM] = build_int_cst (NULL_TREE, VALUE);
#define DEF_ATTR_IDENT(ENUM, STRING)				\
  built_in_attributes[(int) ENUM] = get_identifier (STRING);
#define DEF_ATTR_TREE_LIST(ENUM, PURPOSE, VALUE, CHAIN)	\
  built_in_attributes[(int) ENUM]			\
    = tree_cons (built_in_attributes[(int) PURPOSE],	\
		 built_in_attributes[(int) VALUE],	\
		 built_in_attributes[(int) CHAIN]);
#include "builtin-attrs.def"
#undef DEF_ATTR_NULL_TREE
#undef DEF_ATTR_INT
#undef DEF_ATTR_IDENT
#undef DEF_ATTR_TREE_LIST
}

/* Handle a "const" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_const_attribute (tree *node, tree ARG_UNUSED (name),
			tree ARG_UNUSED (args), int ARG_UNUSED (flags),
			bool *no_add_attrs)
{
  if (TREE_CODE (*node) == FUNCTION_DECL)
    TREE_READONLY (*node) = 1;
  else
    *no_add_attrs = true;

  return NULL_TREE;
}

/* Handle a "nothrow" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_nothrow_attribute (tree *node, tree ARG_UNUSED (name),
			  tree ARG_UNUSED (args), int ARG_UNUSED (flags),
			  bool *no_add_attrs)
{
  if (TREE_CODE (*node) == FUNCTION_DECL)
    TREE_NOTHROW (*node) = 1;
  else
    *no_add_attrs = true;

  return NULL_TREE;
}

/* Handle a "pure" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_pure_attribute (tree *node, tree name, tree ARG_UNUSED (args),
		       int ARG_UNUSED (flags), bool *no_add_attrs)
{
  if (TREE_CODE (*node) == FUNCTION_DECL)
    DECL_PURE_P (*node) = 1;
  /* ??? TODO: Support types.  */
  else
    {
      warning (OPT_Wattributes, "%qs attribute ignored",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle a "no vops" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_novops_attribute (tree *node, tree ARG_UNUSED (name),
			 tree ARG_UNUSED (args), int ARG_UNUSED (flags),
			 bool *ARG_UNUSED (no_add_attrs))
{
  gcc_assert (TREE_CODE (*node) == FUNCTION_DECL);
  DECL_IS_NOVOPS (*node) = 1;
  return NULL_TREE;
}

/* Helper for nonnull attribute handling; fetch the operand number
   from the attribute argument list.  */

static bool
get_nonnull_operand (tree arg_num_expr, unsigned HOST_WIDE_INT *valp)
{
  /* Verify the arg number is a constant.  */
  if (TREE_CODE (arg_num_expr) != INTEGER_CST
      || TREE_INT_CST_HIGH (arg_num_expr) != 0)
    return false;

  *valp = TREE_INT_CST_LOW (arg_num_expr);
  return true;
}

/* Handle the "nonnull" attribute.  */
static tree
handle_nonnull_attribute (tree *node, tree ARG_UNUSED (name),
			  tree args, int ARG_UNUSED (flags),
			  bool *no_add_attrs)
{
  tree type = *node;
  unsigned HOST_WIDE_INT attr_arg_num;

  /* If no arguments are specified, all pointer arguments should be
     non-null.  Verify a full prototype is given so that the arguments
     will have the correct types when we actually check them later.  */
  if (!args)
    {
      if (!prototype_p (type))
	{
	  error ("nonnull attribute without arguments on a non-prototype");
	  *no_add_attrs = true;
	}
      return NULL_TREE;
    }

  /* Argument list specified.  Verify that each argument number references
     a pointer argument.  */
  for (attr_arg_num = 1; args; args = TREE_CHAIN (args))
    {
      tree argument;
      unsigned HOST_WIDE_INT arg_num = 0, ck_num;

      if (!get_nonnull_operand (TREE_VALUE (args), &arg_num))
	{
	  error ("nonnull argument has invalid operand number (argument %lu)",
		 (unsigned long) attr_arg_num);
	  *no_add_attrs = true;
	  return NULL_TREE;
	}

      argument = TYPE_ARG_TYPES (type);
      if (argument)
	{
	  for (ck_num = 1; ; ck_num++)
	    {
	      if (!argument || ck_num == arg_num)
		break;
	      argument = TREE_CHAIN (argument);
	    }

	  if (!argument
	      || TREE_CODE (TREE_VALUE (argument)) == VOID_TYPE)
	    {
	      error ("nonnull argument with out-of-range operand number "
		     "(argument %lu, operand %lu)",
		     (unsigned long) attr_arg_num, (unsigned long) arg_num);
	      *no_add_attrs = true;
	      return NULL_TREE;
	    }

	  if (TREE_CODE (TREE_VALUE (argument)) != POINTER_TYPE)
	    {
	      error ("nonnull argument references non-pointer operand "
		     "(argument %lu, operand %lu)",
		   (unsigned long) attr_arg_num, (unsigned long) arg_num);
	      *no_add_attrs = true;
	      return NULL_TREE;
	    }
	}
    }

  return NULL_TREE;
}

/* Handle a "sentinel" attribute.  */

static tree
handle_sentinel_attribute (tree *node, tree name, tree args,
			   int ARG_UNUSED (flags), bool *no_add_attrs)
{
  tree params = TYPE_ARG_TYPES (*node);

  if (!prototype_p (*node))
    {
      warning (OPT_Wattributes,
	       "%qs attribute requires prototypes with named arguments",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else
    {
      while (TREE_CHAIN (params))
	params = TREE_CHAIN (params);

      if (VOID_TYPE_P (TREE_VALUE (params)))
        {
	  warning (OPT_Wattributes,
		   "%qs attribute only applies to variadic functions",
		   IDENTIFIER_POINTER (name));
	  *no_add_attrs = true;
	}
    }

  if (args)
    {
      tree position = TREE_VALUE (args);

      if (TREE_CODE (position) != INTEGER_CST)
        {
	  warning (0, "requested position is not an integer constant");
	  *no_add_attrs = true;
	}
      else
        {
	  if (tree_int_cst_lt (position, integer_zero_node))
	    {
	      warning (0, "requested position is less than zero");
	      *no_add_attrs = true;
	    }
	}
    }

  return NULL_TREE;
}

/* Handle a "noreturn" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_noreturn_attribute (tree *node, tree name, tree ARG_UNUSED (args),
			   int ARG_UNUSED (flags), bool *no_add_attrs)
{
  tree type = TREE_TYPE (*node);

  /* See FIXME comment in c_common_attribute_table.  */
  if (TREE_CODE (*node) == FUNCTION_DECL)
    TREE_THIS_VOLATILE (*node) = 1;
  else if (TREE_CODE (type) == POINTER_TYPE
	   && TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE)
    TREE_TYPE (*node)
      = build_pointer_type
	(build_type_variant (TREE_TYPE (type),
			     TYPE_READONLY (TREE_TYPE (type)), 1));
  else
    {
      warning (OPT_Wattributes, "%qs attribute ignored",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle a "leaf" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_leaf_attribute (tree *node, tree name,
		       tree ARG_UNUSED (args),
		       int ARG_UNUSED (flags), bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute ignored", name);
      *no_add_attrs = true;
    }
  if (!TREE_PUBLIC (*node))
    {
      warning (OPT_Wattributes, "%qE attribute has no effect", name);
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle a "malloc" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_malloc_attribute (tree *node, tree name, tree ARG_UNUSED (args),
			 int ARG_UNUSED (flags), bool *no_add_attrs)
{
  if (TREE_CODE (*node) == FUNCTION_DECL
      && POINTER_TYPE_P (TREE_TYPE (TREE_TYPE (*node))))
    DECL_IS_MALLOC (*node) = 1;
  else
    {
      warning (OPT_Wattributes, "%qs attribute ignored",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Fake handler for attributes we don't properly support.  */

tree
fake_attribute_handler (tree * ARG_UNUSED (node),
			tree ARG_UNUSED (name),
			tree ARG_UNUSED (args),
			int  ARG_UNUSED (flags),
			bool * ARG_UNUSED (no_add_attrs))
{
  return NULL_TREE;
}

/* Handle a "type_generic" attribute.  */

static tree
handle_type_generic_attribute (tree *node, tree ARG_UNUSED (name),
			       tree ARG_UNUSED (args), int ARG_UNUSED (flags),
			       bool * ARG_UNUSED (no_add_attrs))
{
  tree params;

  /* Ensure we have a function type.  */
  gcc_assert (TREE_CODE (*node) == FUNCTION_TYPE);

  params = TYPE_ARG_TYPES (*node);
  while (params && ! VOID_TYPE_P (TREE_VALUE (params)))
    params = TREE_CHAIN (params);

  /* Ensure we have a variadic function.  */
  gcc_assert (!params);

  return NULL_TREE;
}

/* Handle a "vector_size" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_vector_size_attribute (tree *node, tree name, tree args,
			      int ARG_UNUSED (flags),
			      bool *no_add_attrs)
{
  unsigned HOST_WIDE_INT vecsize, nunits;
  enum machine_mode orig_mode;
  tree type = *node, new_type, size;

  *no_add_attrs = true;

  size = TREE_VALUE (args);

  if (!host_integerp (size, 1))
    {
      warning (OPT_Wattributes, "%qs attribute ignored",
	       IDENTIFIER_POINTER (name));
      return NULL_TREE;
    }

  /* Get the vector size (in bytes).  */
  vecsize = tree_low_cst (size, 1);

  /* We need to provide for vector pointers, vector arrays, and
     functions returning vectors.  For example:

       __attribute__((vector_size(16))) short *foo;

     In this case, the mode is SI, but the type being modified is
     HI, so we need to look further.  */

  while (POINTER_TYPE_P (type)
	 || TREE_CODE (type) == FUNCTION_TYPE
	 || TREE_CODE (type) == ARRAY_TYPE)
    type = TREE_TYPE (type);

  /* Get the mode of the type being modified.  */
  orig_mode = TYPE_MODE (type);

  if ((!INTEGRAL_TYPE_P (type)
       && !SCALAR_FLOAT_TYPE_P (type)
       && !FIXED_POINT_TYPE_P (type))
      || (!SCALAR_FLOAT_MODE_P (orig_mode)
	  && GET_MODE_CLASS (orig_mode) != MODE_INT
	  && !ALL_SCALAR_FIXED_POINT_MODE_P (orig_mode))
      || !host_integerp (TYPE_SIZE_UNIT (type), 1)
      || TREE_CODE (type) == BOOLEAN_TYPE)
    {
      error ("invalid vector type for attribute %qs",
	     IDENTIFIER_POINTER (name));
      return NULL_TREE;
    }

  if (vecsize % tree_low_cst (TYPE_SIZE_UNIT (type), 1))
    {
      error ("vector size not an integral multiple of component size");
      return NULL;
    }

  if (vecsize == 0)
    {
      error ("zero vector size");
      return NULL;
    }

  /* Calculate how many units fit in the vector.  */
  nunits = vecsize / tree_low_cst (TYPE_SIZE_UNIT (type), 1);
  if (nunits & (nunits - 1))
    {
      error ("number of components of the vector not a power of two");
      return NULL_TREE;
    }

  new_type = build_vector_type (type, nunits);

  /* Build back pointers if needed.  */
  *node = reconstruct_complex_type (*node, new_type);

  return NULL_TREE;
}

/* Handle a "vector_type" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
handle_vector_type_attribute (tree *node, tree name, tree ARG_UNUSED (args),
			      int ARG_UNUSED (flags),
			      bool *no_add_attrs)
{
  /* Vector representative type and size.  */
  tree rep_type = *node;
  tree rep_size = TYPE_SIZE_UNIT (rep_type);
  tree rep_name;

  /* Vector size in bytes and number of units.  */
  unsigned HOST_WIDE_INT vec_bytes, vec_units;

  /* Vector element type and mode.  */
  tree elem_type;
  enum machine_mode elem_mode;

  *no_add_attrs = true;

  /* Get the representative array type, possibly nested within a
     padding record e.g. for alignment purposes.  */

  if (TYPE_IS_PADDING_P (rep_type))
    rep_type = TREE_TYPE (TYPE_FIELDS (rep_type));

  if (TREE_CODE (rep_type) != ARRAY_TYPE)
    {
      error ("attribute %qs applies to array types only",
	     IDENTIFIER_POINTER (name));
      return NULL_TREE;
    }

  /* Silently punt on variable sizes.  We can't make vector types for them,
     need to ignore them on front-end generated subtypes of unconstrained
     bases, and this attribute is for binding implementors, not end-users, so
     we should never get there from legitimate explicit uses.  */

  if (!host_integerp (rep_size, 1))
    return NULL_TREE;

  /* Get the element type/mode and check this is something we know
     how to make vectors of.  */

  elem_type = TREE_TYPE (rep_type);
  elem_mode = TYPE_MODE (elem_type);

  if ((!INTEGRAL_TYPE_P (elem_type)
       && !SCALAR_FLOAT_TYPE_P (elem_type)
       && !FIXED_POINT_TYPE_P (elem_type))
      || (!SCALAR_FLOAT_MODE_P (elem_mode)
	  && GET_MODE_CLASS (elem_mode) != MODE_INT
	  && !ALL_SCALAR_FIXED_POINT_MODE_P (elem_mode))
      || !host_integerp (TYPE_SIZE_UNIT (elem_type), 1))
    {
      error ("invalid element type for attribute %qs",
	     IDENTIFIER_POINTER (name));
      return NULL_TREE;
    }

  /* Sanity check the vector size and element type consistency.  */

  vec_bytes = tree_low_cst (rep_size, 1);

  if (vec_bytes % tree_low_cst (TYPE_SIZE_UNIT (elem_type), 1))
    {
      error ("vector size not an integral multiple of component size");
      return NULL;
    }

  if (vec_bytes == 0)
    {
      error ("zero vector size");
      return NULL;
    }

  vec_units = vec_bytes / tree_low_cst (TYPE_SIZE_UNIT (elem_type), 1);
  if (vec_units & (vec_units - 1))
    {
      error ("number of components of the vector not a power of two");
      return NULL_TREE;
    }

  /* Build the vector type and replace.  */

  *node = build_vector_type (elem_type, vec_units);
  rep_name = TYPE_NAME (rep_type);
  if (TREE_CODE (rep_name) == TYPE_DECL)
    rep_name = DECL_NAME (rep_name);
  TYPE_NAME (*node) = rep_name;
  TYPE_REPRESENTATIVE_ARRAY (*node) = rep_type;

  return NULL_TREE;
}

/* ----------------------------------------------------------------------- *
 *                              BUILTIN FUNCTIONS                          *
 * ----------------------------------------------------------------------- */

/* Worker for DEF_BUILTIN.  Possibly define a builtin function with one or two
   names.  Does not declare a non-__builtin_ function if flag_no_builtin, or
   if nonansi_p and flag_no_nonansi_builtin.  */

static void
def_builtin_1 (enum built_in_function fncode,
	       const char *name,
	       enum built_in_class fnclass,
	       tree fntype, tree libtype,
	       bool both_p, bool fallback_p,
	       bool nonansi_p ATTRIBUTE_UNUSED,
	       tree fnattrs, bool implicit_p)
{
  tree decl;
  const char *libname;

  /* Preserve an already installed decl.  It most likely was setup in advance
     (e.g. as part of the internal builtins) for specific reasons.  */
  if (built_in_decls[(int) fncode] != NULL_TREE)
    return;

  gcc_assert ((!both_p && !fallback_p)
	      || !strncmp (name, "__builtin_",
			   strlen ("__builtin_")));

  libname = name + strlen ("__builtin_");
  decl = add_builtin_function (name, fntype, fncode, fnclass,
			       (fallback_p ? libname : NULL),
			       fnattrs);
  if (both_p)
    /* ??? This is normally further controlled by command-line options
       like -fno-builtin, but we don't have them for Ada.  */
    add_builtin_function (libname, libtype, fncode, fnclass,
			  NULL, fnattrs);

  built_in_decls[(int) fncode] = decl;
  if (implicit_p)
    implicit_built_in_decls[(int) fncode] = decl;
}

static int flag_isoc94 = 0;
static int flag_isoc99 = 0;

/* Install what the common builtins.def offers.  */

static void
install_builtin_functions (void)
{
#define DEF_BUILTIN(ENUM, NAME, CLASS, TYPE, LIBTYPE, BOTH_P, FALLBACK_P, \
		    NONANSI_P, ATTRS, IMPLICIT, COND)			\
  if (NAME && COND)							\
    def_builtin_1 (ENUM, NAME, CLASS,                                   \
                   builtin_types[(int) TYPE],                           \
                   builtin_types[(int) LIBTYPE],                        \
                   BOTH_P, FALLBACK_P, NONANSI_P,                       \
                   built_in_attributes[(int) ATTRS], IMPLICIT);
#include "builtins.def"
#undef DEF_BUILTIN
}

/* ----------------------------------------------------------------------- *
 *                              BUILTIN FUNCTIONS                          *
 * ----------------------------------------------------------------------- */

/* Install the builtin functions we might need.  */

void
gnat_install_builtins (void)
{
  install_builtin_elementary_types ();
  install_builtin_function_types ();
  install_builtin_attributes ();

  /* Install builtins used by generic middle-end pieces first.  Some of these
     know about internal specificities and control attributes accordingly, for
     instance __builtin_alloca vs no-throw and -fstack-check.  We will ignore
     the generic definition from builtins.def.  */
  build_common_builtin_nodes ();

  /* Now, install the target specific builtins, such as the AltiVec family on
     ppc, and the common set as exposed by builtins.def.  */
  targetm.init_builtins ();
  install_builtin_functions ();
}

#include "gt-ada-utils.h"
#include "gtype-ada.h"
