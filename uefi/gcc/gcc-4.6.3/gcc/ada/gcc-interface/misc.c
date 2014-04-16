/****************************************************************************
 *                                                                          *
 *                         GNAT COMPILER COMPONENTS                         *
 *                                                                          *
 *                                 M I S C                                  *
 *                                                                          *
 *                           C Implementation File                          *
 *                                                                          *
 *          Copyright (C) 1992-2010, Free Software Foundation, Inc.         *
 *                                                                          *
 * GNAT is free software;  you can  redistribute it  and/or modify it under *
 * terms of the  GNU General Public License as published  by the Free Soft- *
 * ware  Foundation;  either version 3,  or (at your option) any later ver- *
 * sion.  GNAT is distributed in the hope that it will be useful, but WITH- *
 * OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License *
 * for  more details.  You should have  received  a copy of the GNU General *
 * Public License  distributed  with GNAT;  see file  COPYING3.  If not see *
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
#include "diagnostic.h"
#include "target.h"
#include "ggc.h"
#include "flags.h"
#include "debug.h"
#include "toplev.h"
#include "langhooks.h"
#include "langhooks-def.h"
#include "opts.h"
#include "options.h"
#include "plugin.h"
#include "function.h"	/* For pass_by_reference.  */

#include "ada.h"
#include "adadecode.h"
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

/* This symbol needs to be defined for the front-end.  */
void *callgraph_info_file = NULL;

/* Command-line argc and argv.  These variables are global since they are
   imported in back_end.adb.  */
unsigned int save_argc;
const char **save_argv;

/* GNAT argc and argv.  */
extern int gnat_argc;
extern char **gnat_argv;

/* Declare functions we use as part of startup.  */
extern void __gnat_initialize (void *);
extern void __gnat_install_SEH_handler (void *);
extern void adainit (void);
extern void _ada_gnat1drv (void);

/* The parser for the language.  For us, we process the GNAT tree.  */

static void
gnat_parse_file (void)
{
  int seh[2];

  /* Call the target specific initializations.  */
  __gnat_initialize (NULL);

  /* ??? Call the SEH initialization routine.  This is to workaround
  a bootstrap path problem.  The call below should be removed at some
  point and the SEH pointer passed to __gnat_initialize() above.  */
  __gnat_install_SEH_handler((void *)seh);

  /* Call the front-end elaboration procedures.  */
  adainit ();

  /* Call the front end.  */
  _ada_gnat1drv ();
}

/* Decode all the language specific options that cannot be decoded by GCC.
   The option decoding phase of GCC calls this routine on the flags that
   are marked as Ada-specific.  Return true on success or false on failure.  */

static bool
gnat_handle_option (size_t scode, const char *arg ATTRIBUTE_UNUSED, int value,
		    int kind ATTRIBUTE_UNUSED, location_t loc ATTRIBUTE_UNUSED,
		    const struct cl_option_handlers *handlers ATTRIBUTE_UNUSED)
{
  enum opt_code code = (enum opt_code) scode;

  switch (code)
    {
    case OPT_Wall:
      warn_unused = value;
      warn_uninitialized = value;
      break;

    case OPT_Wmissing_prototypes:
    case OPT_Wstrict_prototypes:
    case OPT_Wwrite_strings:
    case OPT_Wlong_long:
    case OPT_Wvariadic_macros:
    case OPT_Wold_style_definition:
    case OPT_Wmissing_format_attribute:
    case OPT_Woverlength_strings:
      /* These are used in the GCC Makefile.  */
      break;

    case OPT_feliminate_unused_debug_types:
      /* We arrange for post_option to be able to only set the corresponding
	 flag to 1 when explicitly requested by the user.  We expect the
	 default flag value to be either 0 or positive, and expose a positive
	 -f as a negative value to post_option.  */
      flag_eliminate_unused_debug_types = -value;
      break;

    case OPT_gant:
      warning (0, "%<-gnat%> misspelled as %<-gant%>");

      /* ... fall through ... */

    case OPT_gnat:
    case OPT_gnatO:
    case OPT_fRTS_:
    case OPT_I:
    case OPT_nostdinc:
    case OPT_nostdlib:
      /* These are handled by the front-end.  */
      break;

    default:
      gcc_unreachable ();
    }

  return true;
}

/* Return language mask for option processing.  */

static unsigned int
gnat_option_lang_mask (void)
{
  return CL_Ada;
}

/* Initialize options structure OPTS.  */

static void
gnat_init_options_struct (struct gcc_options *opts)
{
  /* Uninitialized really means uninitialized in Ada.  */
  opts->x_flag_zero_initialized_in_bss = 0;
}

/* Initialize for option processing.  */

static void
gnat_init_options (unsigned int decoded_options_count,
		   struct cl_decoded_option *decoded_options)
{
  /* Reconstruct an argv array for use of back_end.adb.

     ??? back_end.adb should not rely on this; instead, it should work with
     decoded options without such reparsing, to ensure consistency in how
     options are decoded.  */
  unsigned int i;

  save_argv = XNEWVEC (const char *, 2 * decoded_options_count + 1);
  save_argc = 0;
  for (i = 0; i < decoded_options_count; i++)
    {
      size_t num_elements = decoded_options[i].canonical_option_num_elements;

      if (decoded_options[i].errors
	  || decoded_options[i].opt_index == OPT_SPECIAL_unknown
	  || num_elements == 0)
	continue;

      /* Deal with -I- specially since it must be a single switch.  */
      if (decoded_options[i].opt_index == OPT_I
	  && num_elements == 2
	  && decoded_options[i].canonical_option[1][0] == '-'
	  && decoded_options[i].canonical_option[1][1] == '\0')
	save_argv[save_argc++] = "-I-";
      else
	{
	  gcc_assert (num_elements >= 1 && num_elements <= 2);
	  save_argv[save_argc++] = decoded_options[i].canonical_option[0];
	  if (num_elements >= 2)
	    save_argv[save_argc++] = decoded_options[i].canonical_option[1];
	}
    }
  save_argv[save_argc] = NULL;

  gnat_argv = (char **) xmalloc (sizeof (save_argv[0]));
  gnat_argv[0] = xstrdup (save_argv[0]);     /* name of the command */
  gnat_argc = 1;
}

/* Ada code requires variables for these settings rather than elements
   of the global_options structure.  */
#undef optimize
#undef optimize_size
#undef flag_compare_debug
#undef flag_stack_check
int optimize;
int optimize_size;
int flag_compare_debug;
enum stack_check_type flag_stack_check = NO_STACK_CHECK;

/* Post-switch processing.  */

static bool
gnat_post_options (const char **pfilename ATTRIBUTE_UNUSED)
{
  /* Excess precision other than "fast" requires front-end
     support.  */
  if (flag_excess_precision_cmdline == EXCESS_PRECISION_STANDARD
      && TARGET_FLT_EVAL_METHOD_NON_DEFAULT)
    sorry ("-fexcess-precision=standard for Ada");
  flag_excess_precision_cmdline = EXCESS_PRECISION_FAST;

  /* ??? The warning machinery is outsmarted by Ada.  */
  warn_unused_parameter = 0;

  /* No psABI change warnings for Ada.  */
  warn_psabi = 0;

  /* Force eliminate_unused_debug_types to 0 unless an explicit positive
     -f has been passed.  This forces the default to 0 for Ada, which might
     differ from the common default.  */
  if (flag_eliminate_unused_debug_types < 0)
    flag_eliminate_unused_debug_types = 1;
  else
    flag_eliminate_unused_debug_types = 0;

  optimize = global_options.x_optimize;
  optimize_size = global_options.x_optimize_size;
  flag_compare_debug = global_options.x_flag_compare_debug;
  flag_stack_check = global_options.x_flag_stack_check;

  return false;
}

/* Here is the function to handle the compiler error processing in GCC.  */

static void
internal_error_function (diagnostic_context *context,
			 const char *msgid, va_list *ap)
{
  text_info tinfo;
  char *buffer, *p, *loc;
  String_Template temp, temp_loc;
  Fat_Pointer fp, fp_loc;
  expanded_location s;

  /* Warn if plugins present.  */
  warn_if_plugins ();

  /* Reset the pretty-printer.  */
  pp_clear_output_area (context->printer);

  /* Format the message into the pretty-printer.  */
  tinfo.format_spec = msgid;
  tinfo.args_ptr = ap;
  tinfo.err_no = errno;
  pp_format_verbatim (context->printer, &tinfo);

  /* Extract a (writable) pointer to the formatted text.  */
  buffer = xstrdup (pp_formatted_text (context->printer));

  /* Go up to the first newline.  */
  for (p = buffer; *p; p++)
    if (*p == '\n')
      {
	*p = '\0';
	break;
      }

  temp.Low_Bound = 1;
  temp.High_Bound = p - buffer;
  fp.Bounds = &temp;
  fp.Array = buffer;

  s = expand_location (input_location);
  if (context->show_column && s.column != 0)
    asprintf (&loc, "%s:%d:%d", s.file, s.line, s.column);
  else
    asprintf (&loc, "%s:%d", s.file, s.line);
  temp_loc.Low_Bound = 1;
  temp_loc.High_Bound = strlen (loc);
  fp_loc.Bounds = &temp_loc;
  fp_loc.Array = loc;

  Current_Error_Node = error_gnat_node;
  Compiler_Abort (fp, -1, fp_loc);
}

/* Perform all the initialization steps that are language-specific.  */

static bool
gnat_init (void)
{
  /* Do little here, most of the standard declarations are set up after the
     front-end has been run.  Use the same `char' as C, this doesn't really
     matter since we'll use the explicit `unsigned char' for Character.  */
  build_common_tree_nodes (flag_signed_char);

  /* In Ada, we use the unsigned type corresponding to the width of Pmode as
     SIZETYPE.  In most cases when ptr_mode and Pmode differ, C will use the
     width of ptr_mode for SIZETYPE, but we get better code using the width
     of Pmode.  Note that, although we manipulate negative offsets for some
     internal constructs and rely on compile time overflow detection in size
     computations, using unsigned types for SIZETYPEs is fine since they are
     treated specially by the middle-end, in particular sign-extended.  */
  size_type_node = gnat_type_for_mode (Pmode, 1);
  set_sizetype (size_type_node);
  TYPE_NAME (sizetype) = get_identifier ("size_type");

  /* In Ada, we use an unsigned 8-bit type for the default boolean type.  */
  boolean_type_node = make_unsigned_type (8);
  TREE_SET_CODE (boolean_type_node, BOOLEAN_TYPE);
  SET_TYPE_RM_MAX_VALUE (boolean_type_node,
			 build_int_cst (boolean_type_node, 1));
  SET_TYPE_RM_SIZE (boolean_type_node, bitsize_int (1));

  build_common_tree_nodes_2 (0);
  sbitsize_one_node = sbitsize_int (1);
  sbitsize_unit_node = sbitsize_int (BITS_PER_UNIT);
  boolean_true_node = TYPE_MAX_VALUE (boolean_type_node);

  ptr_void_type_node = build_pointer_type (void_type_node);

  /* Show that REFERENCE_TYPEs are internal and should be Pmode.  */
  internal_reference_types ();

  /* Register our internal error function.  */
  global_dc->internal_error = &internal_error_function;

  return true;
}

/* If we are using the GCC mechanism to process exception handling, we
   have to register the personality routine for Ada and to initialize
   various language dependent hooks.  */

void
gnat_init_gcc_eh (void)
{
#ifdef DWARF2_UNWIND_INFO
  /* lang_dependent_init already called dwarf2out_frame_init if true.  */
  int dwarf2out_frame_initialized = dwarf2out_do_frame ();
#endif

  /* We shouldn't do anything if the No_Exceptions_Handler pragma is set,
     though. This could for instance lead to the emission of tables with
     references to symbols (such as the Ada eh personality routine) within
     libraries we won't link against.  */
  if (No_Exception_Handlers_Set ())
    return;

  /* Tell GCC we are handling cleanup actions through exception propagation.
     This opens possibilities that we don't take advantage of yet, but is
     nonetheless necessary to ensure that fixup code gets assigned to the
     right exception regions.  */
  using_eh_for_cleanups ();

  /* Turn on -fexceptions and -fnon-call-exceptions.  The first one triggers
     the generation of the necessary exception tables.  The second one is
     useful for two reasons: 1/ we map some asynchronous signals like SEGV to
     exceptions, so we need to ensure that the insns which can lead to such
     signals are correctly attached to the exception region they pertain to,
     2/ Some calls to pure subprograms are handled as libcall blocks and then
     marked as "cannot trap" if the flag is not set (see emit_libcall_block).
     We should not let this be since it is possible for such calls to actually
     raise in Ada.  */
  flag_exceptions = 1;
  flag_non_call_exceptions = 1;

  init_eh ();

#ifdef DWARF2_UNWIND_INFO
  if (!dwarf2out_frame_initialized && dwarf2out_do_frame ())
    dwarf2out_frame_init ();
#endif
}

/* Print language-specific items in declaration NODE.  */

static void
gnat_print_decl (FILE *file, tree node, int indent)
{
  switch (TREE_CODE (node))
    {
    case CONST_DECL:
      print_node (file, "corresponding var",
		  DECL_CONST_CORRESPONDING_VAR (node), indent + 4);
      break;

    case FIELD_DECL:
      print_node (file, "original field", DECL_ORIGINAL_FIELD (node),
		  indent + 4);
      break;

    case VAR_DECL:
      print_node (file, "renamed object", DECL_RENAMED_OBJECT (node),
		  indent + 4);
      break;

    default:
      break;
    }
}

/* Print language-specific items in type NODE.  */

static void
gnat_print_type (FILE *file, tree node, int indent)
{
  switch (TREE_CODE (node))
    {
    case FUNCTION_TYPE:
      print_node (file, "ci/co list", TYPE_CI_CO_LIST (node), indent + 4);
      break;

    case INTEGER_TYPE:
      if (TYPE_MODULAR_P (node))
	print_node_brief (file, "modulus", TYPE_MODULUS (node), indent + 4);
      else if (TYPE_HAS_ACTUAL_BOUNDS_P (node))
	print_node (file, "actual bounds", TYPE_ACTUAL_BOUNDS (node),
		    indent + 4);
      else if (TYPE_VAX_FLOATING_POINT_P (node))
	;
      else
	print_node (file, "index type", TYPE_INDEX_TYPE (node), indent + 4);

      /* ... fall through ... */

    case ENUMERAL_TYPE:
    case BOOLEAN_TYPE:
      print_node_brief (file, "RM size", TYPE_RM_SIZE (node), indent + 4);

      /* ... fall through ... */

    case REAL_TYPE:
      print_node_brief (file, "RM min", TYPE_RM_MIN_VALUE (node), indent + 4);
      print_node_brief (file, "RM max", TYPE_RM_MAX_VALUE (node), indent + 4);
      break;

    case ARRAY_TYPE:
      print_node (file,"actual bounds", TYPE_ACTUAL_BOUNDS (node), indent + 4);
      break;

    case VECTOR_TYPE:
      print_node (file,"representative array",
		  TYPE_REPRESENTATIVE_ARRAY (node), indent + 4);
      break;

    case RECORD_TYPE:
      if (TYPE_FAT_POINTER_P (node) || TYPE_CONTAINS_TEMPLATE_P (node))
	print_node (file, "unconstrained array",
		    TYPE_UNCONSTRAINED_ARRAY (node), indent + 4);
      else
	print_node (file, "Ada size", TYPE_ADA_SIZE (node), indent + 4);
      break;

    case UNION_TYPE:
    case QUAL_UNION_TYPE:
      print_node (file, "Ada size", TYPE_ADA_SIZE (node), indent + 4);
      break;

    default:
      break;
    }
}

/* Return the name to be printed for DECL.  */

static const char *
gnat_printable_name (tree decl, int verbosity)
{
  const char *coded_name = IDENTIFIER_POINTER (DECL_NAME (decl));
  char *ada_name = (char *) ggc_alloc_atomic (strlen (coded_name) * 2 + 60);

  __gnat_decode (coded_name, ada_name, 0);

  if (verbosity == 2 && !DECL_IS_BUILTIN (decl))
    {
      Set_Identifier_Casing (ada_name, DECL_SOURCE_FILE (decl));
      return ggc_strdup (Name_Buffer);
    }

  return ada_name;
}

/* Return the name to be used in DWARF debug info for DECL.  */

static const char *
gnat_dwarf_name (tree decl, int verbosity ATTRIBUTE_UNUSED)
{
  gcc_assert (DECL_P (decl));
  return (const char *) IDENTIFIER_POINTER (DECL_NAME (decl));
}

/* Return true if types T1 and T2 are identical for type hashing purposes.
   Called only after doing all language independent checks.  At present,
   this function is only called when both types are FUNCTION_TYPE.  */

static bool
gnat_type_hash_eq (const_tree t1, const_tree t2)
{
  gcc_assert (TREE_CODE (t1) == FUNCTION_TYPE);
  return fntype_same_flags_p (t1, TYPE_CI_CO_LIST (t2),
			      TYPE_RETURN_UNCONSTRAINED_P (t2),
			      TYPE_RETURN_BY_DIRECT_REF_P (t2),
			      TREE_ADDRESSABLE (t2));
}

/* Do nothing (return the tree node passed).  */

static tree
gnat_return_tree (tree t)
{
  return t;
}

/* Get the alias set corresponding to a type or expression.  */

static alias_set_type
gnat_get_alias_set (tree type)
{
  /* If this is a padding type, use the type of the first field.  */
  if (TYPE_IS_PADDING_P (type))
    return get_alias_set (TREE_TYPE (TYPE_FIELDS (type)));

  /* If the type is an unconstrained array, use the type of the
     self-referential array we make.  */
  else if (TREE_CODE (type) == UNCONSTRAINED_ARRAY_TYPE)
    return
      get_alias_set (TREE_TYPE (TREE_TYPE (TYPE_FIELDS (TREE_TYPE (type)))));

  /* If the type can alias any other types, return the alias set 0.  */
  else if (TYPE_P (type)
	   && TYPE_UNIVERSAL_ALIASING_P (TYPE_MAIN_VARIANT (type)))
    return 0;

  return -1;
}

/* GNU_TYPE is a type.  Return its maximum size in bytes, if known,
   as a constant when possible.  */

static tree
gnat_type_max_size (const_tree gnu_type)
{
  /* First see what we can get from TYPE_SIZE_UNIT, which might not
     be constant even for simple expressions if it has already been
     elaborated and possibly replaced by a VAR_DECL.  */
  tree max_unitsize = max_size (TYPE_SIZE_UNIT (gnu_type), true);

  /* If we don't have a constant, see what we can get from TYPE_ADA_SIZE,
     which should stay untouched.  */
  if (!host_integerp (max_unitsize, 1)
      && (TREE_CODE (gnu_type) == RECORD_TYPE
	  || TREE_CODE (gnu_type) == UNION_TYPE
	  || TREE_CODE (gnu_type) == QUAL_UNION_TYPE)
      && TYPE_ADA_SIZE (gnu_type))
    {
      tree max_adasize = max_size (TYPE_ADA_SIZE (gnu_type), true);

      /* If we have succeeded in finding a constant, round it up to the
	 type's alignment and return the result in units.  */
      if (host_integerp (max_adasize, 1))
	max_unitsize
	  = size_binop (CEIL_DIV_EXPR,
			round_up (max_adasize, TYPE_ALIGN (gnu_type)),
			bitsize_unit_node);
    }

  return max_unitsize;
}

/* GNU_TYPE is a subtype of an integral type.  Set LOWVAL to the low bound
   and HIGHVAL to the high bound, respectively.  */

static void
gnat_get_subrange_bounds (const_tree gnu_type, tree *lowval, tree *highval)
{
  *lowval = TYPE_MIN_VALUE (gnu_type);
  *highval = TYPE_MAX_VALUE (gnu_type);
}

/* GNU_TYPE is the type of a subprogram parameter.  Determine if it should be
   passed by reference by default.  */

bool
default_pass_by_ref (tree gnu_type)
{
  /* We pass aggregates by reference if they are sufficiently large.  The
     choice of constant here is somewhat arbitrary.  We also pass by
     reference if the target machine would either pass or return by
     reference.  Strictly speaking, we need only check the return if this
     is an In Out parameter, but it's probably best to err on the side of
     passing more things by reference.  */

  if (pass_by_reference (NULL, TYPE_MODE (gnu_type), gnu_type, true))
    return true;

  if (targetm.calls.return_in_memory (gnu_type, NULL_TREE))
    return true;

  if (AGGREGATE_TYPE_P (gnu_type)
      && (!host_integerp (TYPE_SIZE (gnu_type), 1)
	  || 0 < compare_tree_int (TYPE_SIZE (gnu_type),
				   8 * TYPE_ALIGN (gnu_type))))
    return true;

  return false;
}

/* GNU_TYPE is the type of a subprogram parameter.  Determine if it must be
   passed by reference.  */

bool
must_pass_by_ref (tree gnu_type)
{
  /* We pass only unconstrained objects, those required by the language
     to be passed by reference, and objects of variable size.  The latter
     is more efficient, avoids problems with variable size temporaries,
     and does not produce compatibility problems with C, since C does
     not have such objects.  */
  return (TREE_CODE (gnu_type) == UNCONSTRAINED_ARRAY_TYPE
	  || TREE_ADDRESSABLE (gnu_type)
	  || (TYPE_SIZE (gnu_type)
	      && TREE_CODE (TYPE_SIZE (gnu_type)) != INTEGER_CST));
}

/* Return the size of the FP mode with precision PREC.  */

int
fp_prec_to_size (int prec)
{
  enum machine_mode mode;

  for (mode = GET_CLASS_NARROWEST_MODE (MODE_FLOAT); mode != VOIDmode;
       mode = GET_MODE_WIDER_MODE (mode))
    if (GET_MODE_PRECISION (mode) == prec)
      return GET_MODE_BITSIZE (mode);

  gcc_unreachable ();
}

/* Return the precision of the FP mode with size SIZE.  */

int
fp_size_to_prec (int size)
{
  enum machine_mode mode;

  for (mode = GET_CLASS_NARROWEST_MODE (MODE_FLOAT); mode != VOIDmode;
       mode = GET_MODE_WIDER_MODE (mode))
    if (GET_MODE_BITSIZE (mode) == size)
      return GET_MODE_PRECISION (mode);

  gcc_unreachable ();
}

static GTY(()) tree gnat_eh_personality_decl;

/* Return the GNAT personality function decl.  */

static tree
gnat_eh_personality (void)
{
  if (!gnat_eh_personality_decl)
    gnat_eh_personality_decl = build_personality_function ("gnat");
  return gnat_eh_personality_decl;
}

/* Definitions for our language-specific hooks.  */

#undef  LANG_HOOKS_NAME
#define LANG_HOOKS_NAME			"GNU Ada"
#undef  LANG_HOOKS_IDENTIFIER_SIZE
#define LANG_HOOKS_IDENTIFIER_SIZE	sizeof (struct tree_identifier)
#undef  LANG_HOOKS_INIT
#define LANG_HOOKS_INIT			gnat_init
#undef  LANG_HOOKS_OPTION_LANG_MASK
#define LANG_HOOKS_OPTION_LANG_MASK	gnat_option_lang_mask
#undef  LANG_HOOKS_INIT_OPTIONS_STRUCT
#define LANG_HOOKS_INIT_OPTIONS_STRUCT	gnat_init_options_struct
#undef  LANG_HOOKS_INIT_OPTIONS
#define LANG_HOOKS_INIT_OPTIONS		gnat_init_options
#undef  LANG_HOOKS_HANDLE_OPTION
#define LANG_HOOKS_HANDLE_OPTION	gnat_handle_option
#undef  LANG_HOOKS_POST_OPTIONS
#define LANG_HOOKS_POST_OPTIONS		gnat_post_options
#undef  LANG_HOOKS_PARSE_FILE
#define LANG_HOOKS_PARSE_FILE		gnat_parse_file
#undef  LANG_HOOKS_TYPE_HASH_EQ
#define LANG_HOOKS_TYPE_HASH_EQ		gnat_type_hash_eq
#undef  LANG_HOOKS_GETDECLS
#define LANG_HOOKS_GETDECLS		lhd_return_null_tree_v
#undef  LANG_HOOKS_PUSHDECL
#define LANG_HOOKS_PUSHDECL		gnat_return_tree
#undef  LANG_HOOKS_WRITE_GLOBALS
#define LANG_HOOKS_WRITE_GLOBALS	gnat_write_global_declarations
#undef  LANG_HOOKS_GET_ALIAS_SET
#define LANG_HOOKS_GET_ALIAS_SET	gnat_get_alias_set
#undef  LANG_HOOKS_PRINT_DECL
#define LANG_HOOKS_PRINT_DECL		gnat_print_decl
#undef  LANG_HOOKS_PRINT_TYPE
#define LANG_HOOKS_PRINT_TYPE		gnat_print_type
#undef  LANG_HOOKS_TYPE_MAX_SIZE
#define LANG_HOOKS_TYPE_MAX_SIZE	gnat_type_max_size
#undef  LANG_HOOKS_DECL_PRINTABLE_NAME
#define LANG_HOOKS_DECL_PRINTABLE_NAME	gnat_printable_name
#undef  LANG_HOOKS_DWARF_NAME
#define LANG_HOOKS_DWARF_NAME		gnat_dwarf_name
#undef  LANG_HOOKS_GIMPLIFY_EXPR
#define LANG_HOOKS_GIMPLIFY_EXPR	gnat_gimplify_expr
#undef  LANG_HOOKS_TYPE_FOR_MODE
#define LANG_HOOKS_TYPE_FOR_MODE	gnat_type_for_mode
#undef  LANG_HOOKS_TYPE_FOR_SIZE
#define LANG_HOOKS_TYPE_FOR_SIZE	gnat_type_for_size
#undef  LANG_HOOKS_TYPES_COMPATIBLE_P
#define LANG_HOOKS_TYPES_COMPATIBLE_P	gnat_types_compatible_p
#undef  LANG_HOOKS_GET_SUBRANGE_BOUNDS
#define LANG_HOOKS_GET_SUBRANGE_BOUNDS  gnat_get_subrange_bounds
#undef  LANG_HOOKS_ATTRIBUTE_TABLE
#define LANG_HOOKS_ATTRIBUTE_TABLE	gnat_internal_attribute_table
#undef  LANG_HOOKS_BUILTIN_FUNCTION
#define LANG_HOOKS_BUILTIN_FUNCTION	gnat_builtin_function
#undef  LANG_HOOKS_EH_PERSONALITY
#define LANG_HOOKS_EH_PERSONALITY	gnat_eh_personality
#undef  LANG_HOOKS_DEEP_UNSHARING
#define LANG_HOOKS_DEEP_UNSHARING	true

struct lang_hooks lang_hooks = LANG_HOOKS_INITIALIZER;

#include "gt-ada-misc.h"
