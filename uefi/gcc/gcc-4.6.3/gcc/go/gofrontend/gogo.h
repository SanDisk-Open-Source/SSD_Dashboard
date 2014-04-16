// gogo.h -- Go frontend parsed representation.     -*- C++ -*-

// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef GO_GOGO_H
#define GO_GOGO_H

class Traverse;
class Type;
class Type_hash_identical;
class Type_equal;
class Type_identical;
class Typed_identifier;
class Typed_identifier_list;
class Function_type;
class Expression;
class Statement;
class Block;
class Function;
class Bindings;
class Package;
class Variable;
class Pointer_type;
class Struct_type;
class Struct_field;
class Struct_field_list;
class Array_type;
class Map_type;
class Channel_type;
class Interface_type;
class Named_type;
class Forward_declaration_type;
class Method;
class Methods;
class Named_object;
class Label;
class Translate_context;
class Export;
class Import;

// This file declares the basic classes used to hold the internal
// representation of Go which is built by the parser.

// An initialization function for an imported package.  This is a
// magic function which initializes variables and runs the "init"
// function.

class Import_init
{
 public:
  Import_init(const std::string& package_name, const std::string& init_name,
	      int priority)
    : package_name_(package_name), init_name_(init_name), priority_(priority)
  { }

  // The name of the package being imported.
  const std::string&
  package_name() const
  { return this->package_name_; }

  // The name of the package's init function.
  const std::string&
  init_name() const
  { return this->init_name_; }

  // The priority of the initialization function.  Functions with a
  // lower priority number must be run first.
  int
  priority() const
  { return this->priority_; }

 private:
  // The name of the package being imported.
  std::string package_name_;
  // The name of the package's init function.
  std::string init_name_;
  // The priority.
  int priority_;
};

// For sorting purposes.

inline bool
operator<(const Import_init& i1, const Import_init& i2)
{
  if (i1.priority() < i2.priority())
    return true;
  if (i1.priority() > i2.priority())
    return false;
  if (i1.package_name() != i2.package_name())
    return i1.package_name() < i2.package_name();
  return i1.init_name() < i2.init_name();
}

// The holder for the internal representation of the entire
// compilation unit.

class Gogo
{
 public:
  // Create the IR, passing in the sizes of the types "int" and
  // "uintptr" in bits.
  Gogo(int int_type_size, int pointer_size);

  // Get the package name.
  const std::string&
  package_name() const;

  // Set the package name.
  void
  set_package_name(const std::string&, source_location);

  // Return whether this is the "main" package.
  bool
  is_main_package() const;

  // If necessary, adjust the name to use for a hidden symbol.  We add
  // a prefix of the package name, so that hidden symbols in different
  // packages do not collide.
  std::string
  pack_hidden_name(const std::string& name, bool is_exported) const
  {
    return (is_exported
	    ? name
	    : ('.' + this->unique_prefix()
	       + '.' + this->package_name()
	       + '.' + name));
  }

  // Unpack a name which may have been hidden.  Returns the
  // user-visible name of the object.
  static std::string
  unpack_hidden_name(const std::string& name)
  { return name[0] != '.' ? name : name.substr(name.rfind('.') + 1); }

  // Return whether a possibly packed name is hidden.
  static bool
  is_hidden_name(const std::string& name)
  { return name[0] == '.'; }

  // Return the package prefix of a hidden name.
  static std::string
  hidden_name_prefix(const std::string& name)
  {
    gcc_assert(Gogo::is_hidden_name(name));
    return name.substr(1, name.rfind('.') - 1);
  }

  // Given a name which may or may not have been hidden, return the
  // name to use in an error message.
  static std::string
  message_name(const std::string& name);

  // Return whether a name is the blank identifier _.
  static bool
  is_sink_name(const std::string& name)
  {
    return (name[0] == '.'
	    && name[name.length() - 1] == '_'
	    && name[name.length() - 2] == '.');
  }

  // Return the unique prefix to use for all exported symbols.
  const std::string&
  unique_prefix() const;

  // Set the unique prefix.
  void
  set_unique_prefix(const std::string&);

  // Return the priority to use for the package we are compiling.
  // This is two more than the largest priority of any package we
  // import.
  int
  package_priority() const;

  // Import a package.  FILENAME is the file name argument, LOCAL_NAME
  // is the local name to give to the package.  If LOCAL_NAME is empty
  // the declarations are added to the global scope.
  void
  import_package(const std::string& filename, const std::string& local_name,
		 bool is_local_name_exported, source_location);

  // Whether we are the global binding level.
  bool
  in_global_scope() const;

  // Look up a name in the current binding contours.
  Named_object*
  lookup(const std::string&, Named_object** pfunction) const;

  // Look up a name in the current block.
  Named_object*
  lookup_in_block(const std::string&) const;

  // Look up a name in the global namespace--the universal scope.
  Named_object*
  lookup_global(const char*) const;

  // Add a new imported package.  REAL_NAME is the real name of the
  // package.  ALIAS is the alias of the package; this may be the same
  // as REAL_NAME.  This sets *PADD_TO_GLOBALS if symbols added to
  // this package should be added to the global namespace; this is
  // true if the alias is ".".  LOCATION is the location of the import
  // statement.  This returns the new package, or NULL on error.
  Package*
  add_imported_package(const std::string& real_name, const std::string& alias,
		       bool is_alias_exported,
		       const std::string& unique_prefix,
		       source_location location,
		       bool* padd_to_globals);

  // Register a package.  This package may or may not be imported.
  // This returns the Package structure for the package, creating if
  // it necessary.
  Package*
  register_package(const std::string& name, const std::string& unique_prefix,
		   source_location);

  // Start compiling a function.  ADD_METHOD_TO_TYPE is true if a
  // method function should be added to the type of its receiver.
  Named_object*
  start_function(const std::string& name, Function_type* type,
		 bool add_method_to_type, source_location);

  // Finish compiling a function.
  void
  finish_function(source_location);

  // Return the current function.
  Named_object*
  current_function() const;

  // Start a new block.  This is not initially associated with a
  // function.
  void
  start_block(source_location);

  // Finish the current block and return it.
  Block*
  finish_block(source_location);

  // Declare an unknown name.  This is used while parsing.  The name
  // must be resolved by the end of the parse.  Unknown names are
  // always added at the package level.
  Named_object*
  add_unknown_name(const std::string& name, source_location);

  // Declare a function.
  Named_object*
  declare_function(const std::string&, Function_type*, source_location);

  // Add a label.
  Label*
  add_label_definition(const std::string&, source_location);

  // Add a label reference.
  Label*
  add_label_reference(const std::string&);

  // Add a statement to the current block.
  void
  add_statement(Statement*);

  // Add a block to the current block.
  void
  add_block(Block*, source_location);

  // Add a constant.
  Named_object*
  add_constant(const Typed_identifier&, Expression*, int iota_value);

  // Add a type.
  void
  add_type(const std::string&, Type*, source_location);

  // Add a named type.  This is used for builtin types, and to add an
  // imported type to the global scope.
  void
  add_named_type(Named_type*);

  // Declare a type.
  Named_object*
  declare_type(const std::string&, source_location);

  // Declare a type at the package level.  This is used when the
  // parser sees an unknown name where a type name is required.
  Named_object*
  declare_package_type(const std::string&, source_location);

  // Define a type which was already declared.
  void
  define_type(Named_object*, Named_type*);

  // Add a variable.
  Named_object*
  add_variable(const std::string&, Variable*);

  // Add a sink--a reference to the blank identifier _.
  Named_object*
  add_sink();

  // Add a named object to the current namespace.  This is used for
  // import . "package".
  void
  add_named_object(Named_object*);

  // Return a name to use for a thunk function.  A thunk function is
  // one we create during the compilation, for a go statement or a
  // defer statement or a method expression.
  static std::string
  thunk_name();

  // Return whether an object is a thunk.
  static bool
  is_thunk(const Named_object*);

  // Note that we've seen an interface type.  This is used to build
  // all required interface method tables.
  void
  record_interface_type(Interface_type*);

  // Note that we need an initialization function.
  void
  set_need_init_fn()
  { this->need_init_fn_ = true; }

  // Clear out all names in file scope.  This is called when we start
  // parsing a new file.
  void
  clear_file_scope();

  // Traverse the tree.  See the Traverse class.
  void
  traverse(Traverse*);

  // Define the predeclared global names.
  void
  define_global_names();

  // Verify and complete all types.
  void
  verify_types();

  // Lower the parse tree.
  void
  lower_parse_tree();

  // Lower an expression.
  void
  lower_expression(Named_object* function, Expression**);

  // Lower a constant.
  void
  lower_constant(Named_object*);

  // Finalize the method lists and build stub methods for named types.
  void
  finalize_methods();

  // Work out the types to use for unspecified variables and
  // constants.
  void
  determine_types();

  // Type check the program.
  void
  check_types();

  // Check the types in a single block.  This is used for complicated
  // go statements.
  void
  check_types_in_block(Block*);

  // Check for return statements.
  void
  check_return_statements();

  // Do all exports.
  void
  do_exports();

  // Add an import control function for an imported package to the
  // list.
  void
  add_import_init_fn(const std::string& package_name,
		     const std::string& init_name, int prio);

  // Turn short-cut operators (&&, ||) into explicit if statements.
  void
  remove_shortcuts();

  // Use temporary variables to force order of evaluation.
  void
  order_evaluations();

  // Build thunks for functions which call recover.
  void
  build_recover_thunks();

  // Simplify statements which might use thunks: go and defer
  // statements.
  void
  simplify_thunk_statements();

  // Convert named types to the backend representation.
  void
  convert_named_types();

  // Convert named types in a list of bindings.
  void
  convert_named_types_in_bindings(Bindings*);

  // True if named types have been converted to the backend
  // representation.
  bool
  named_types_are_converted() const
  { return this->named_types_are_converted_; }

  // Write out the global values.
  void
  write_globals();

  // Build a call to a builtin function.  PDECL should point to a NULL
  // initialized static pointer which will hold the fndecl.  NAME is
  // the name of the function.  NARGS is the number of arguments.
  // RETTYPE is the return type.  It is followed by NARGS pairs of
  // type and argument (both trees).
  static tree
  call_builtin(tree* pdecl, source_location, const char* name, int nargs,
	       tree rettype, ...);

  // Build a call to the runtime error function.
  static tree
  runtime_error(int code, source_location);

  // Build a builtin struct with a list of fields.
  static tree
  builtin_struct(tree* ptype, const char* struct_name, tree struct_type,
		 int nfields, ...);

  // Mark a function declaration as a builtin library function.
  static void
  mark_fndecl_as_builtin_library(tree fndecl);

  // Build the type of the struct that holds a slice for the given
  // element type.
  tree
  slice_type_tree(tree element_type_tree);

  // Given a tree for a slice type, return the tree for the element
  // type.
  static tree
  slice_element_type_tree(tree slice_type_tree);

  // Build a constructor for a slice.  SLICE_TYPE_TREE is the type of
  // the slice.  VALUES points to the values.  COUNT is the size,
  // CAPACITY is the capacity.  If CAPACITY is NULL, it is set to
  // COUNT.
  static tree
  slice_constructor(tree slice_type_tree, tree values, tree count,
		    tree capacity);

  // Build a constructor for an empty slice.  SLICE_TYPE_TREE is the
  // type of the slice.
  static tree
  empty_slice_constructor(tree slice_type_tree);

  // Build a map descriptor.
  tree
  map_descriptor(Map_type*);

  // Return a tree for the type of a map descriptor.  This is struct
  // __go_map_descriptor in libgo/runtime/map.h.  This is the same for
  // all map types.
  tree
  map_descriptor_type();

  // Build a type descriptor for TYPE using INITIALIZER as the type
  // descriptor.  This builds a new decl stored in *PDECL.
  void
  build_type_descriptor_decl(const Type*, Expression* initializer,
			     tree* pdecl);

  // Build required interface method tables.
  void
  build_interface_method_tables();

  // Build an interface method table for a type: a list of function
  // pointers, one for each interface method.  This returns a decl.
  tree
  interface_method_table_for_type(const Interface_type*, Named_type*,
				  bool is_pointer);

  // Return a tree which allocate SIZE bytes to hold values of type
  // TYPE.
  tree
  allocate_memory(Type *type, tree size, source_location);

  // Return a type to use for pointer to const char.
  static tree
  const_char_pointer_type_tree();

  // Build a string constant with the right type.
  static tree
  string_constant_tree(const std::string&);

  // Build a Go string constant.  This returns a pointer to the
  // constant.
  tree
  go_string_constant_tree(const std::string&);

  // Send a value on a channel.
  static tree
  send_on_channel(tree channel, tree val, bool blocking, bool for_select,
		  source_location);

  // Receive a value from a channel.
  static tree
  receive_from_channel(tree type_tree, tree channel, bool for_select,
		       source_location);

  // Return a tree for receiving an integer on a channel.
  static tree
  receive_as_64bit_integer(tree type, tree channel, bool blocking,
			   bool for_select);


  // Make a trampoline which calls FNADDR passing CLOSURE.
  tree
  make_trampoline(tree fnaddr, tree closure, source_location);

 private:
  // During parsing, we keep a stack of functions.  Each function on
  // the stack is one that we are currently parsing.  For each
  // function, we keep track of the current stack of blocks.
  struct Open_function
  {
    // The function.
    Named_object* function;
    // The stack of active blocks in the function.
    std::vector<Block*> blocks;
  };

  // The stack of functions.
  typedef std::vector<Open_function> Open_functions;

  // Create trees for implicit builtin functions.
  void
  define_builtin_function_trees();

  // Set up the built-in unsafe package.
  void
  import_unsafe(const std::string&, bool is_exported, source_location);

  // Add a new imported package.
  Named_object*
  add_package(const std::string& real_name, const std::string& alias,
	      const std::string& unique_prefix, source_location location);

  // Return the current binding contour.
  Bindings*
  current_bindings();

  const Bindings*
  current_bindings() const;

  // Return the current block.
  Block*
  current_block();

  // Get the name of the magic initialization function.
  const std::string&
  get_init_fn_name();

  // Get the decl for the magic initialization function.
  tree
  initialization_function_decl();

  // Write the magic initialization function.
  void
  write_initialization_function(tree fndecl, tree init_stmt_list);

  // Initialize imported packages.
  void
  init_imports(tree*);

  // Register variables with the garbage collector.
  void
  register_gc_vars(const std::vector<Named_object*>&, tree*);

  // Build a pointer to a Go string constant.  This returns a pointer
  // to the pointer.
  tree
  ptr_go_string_constant_tree(const std::string&);

  // Return the name to use for a type descriptor decl for an unnamed
  // type.
  std::string
  unnamed_type_descriptor_decl_name(const Type* type);

  // Return the name to use for a type descriptor decl for a type
  // named NO, defined in IN_FUNCTION.
  std::string
  type_descriptor_decl_name(const Named_object* no,
			    const Named_object* in_function);

  // Where a type descriptor should be defined.
  enum Type_descriptor_location
    {
      // Defined in this file.
      TYPE_DESCRIPTOR_DEFINED,
      // Defined in some other file.
      TYPE_DESCRIPTOR_UNDEFINED,
      // Common definition which may occur in multiple files.
      TYPE_DESCRIPTOR_COMMON
    };

  // Return where the decl for TYPE should be defined.
  Type_descriptor_location
  type_descriptor_location(const Type* type);

  // Return the type of a trampoline.
  static tree
  trampoline_type_tree();

  // Type used to map import names to packages.
  typedef std::map<std::string, Package*> Imports;

  // Type used to map package names to packages.
  typedef std::map<std::string, Package*> Packages;

  // Type used to map special names in the sys package.
  typedef std::map<std::string, std::string> Sys_names;

  // Hash table mapping map types to map descriptor decls.
  typedef Unordered_map_hash(const Map_type*, tree, Type_hash_identical,
			     Type_identical) Map_descriptors;

  // Map unnamed types to type descriptor decls.
  typedef Unordered_map_hash(const Type*, tree, Type_hash_identical,
			     Type_identical) Type_descriptor_decls;

  // The package we are compiling.
  Package* package_;
  // The list of currently open functions during parsing.
  Open_functions functions_;
  // The global binding contour.  This includes the builtin functions
  // and the package we are compiling.
  Bindings* globals_;
  // Mapping from import file names to packages.
  Imports imports_;
  // Whether the magic unsafe package was imported.
  bool imported_unsafe_;
  // Mapping from package names we have seen to packages.  This does
  // not include the package we are compiling.
  Packages packages_;
  // Mapping from map types to map descriptors.
  Map_descriptors* map_descriptors_;
  // Mapping from unnamed types to type descriptor decls.
  Type_descriptor_decls* type_descriptor_decls_;
  // The functions named "init", if there are any.
  std::vector<Named_object*> init_functions_;
  // Whether we need a magic initialization function.
  bool need_init_fn_;
  // The name of the magic initialization function.
  std::string init_fn_name_;
  // A list of import control variables for packages that we import.
  std::set<Import_init> imported_init_fns_;
  // The unique prefix used for all global symbols.
  std::string unique_prefix_;
  // Whether an explicit unique prefix was set by -fgo-prefix.
  bool unique_prefix_specified_;
  // A list of interface types defined while parsing.
  std::vector<Interface_type*> interface_types_;
  // Whether named types have been converted.
  bool named_types_are_converted_;
};

// A block of statements.

class Block
{
 public:
  Block(Block* enclosing, source_location);

  // Return the enclosing block.
  const Block*
  enclosing() const
  { return this->enclosing_; }

  // Return the bindings of the block.
  Bindings*
  bindings()
  { return this->bindings_; }

  const Bindings*
  bindings() const
  { return this->bindings_; }

  // Look at the block's statements.
  const std::vector<Statement*>*
  statements() const
  { return &this->statements_; }

  // Return the start location.  This is normally the location of the
  // left curly brace which starts the block.
  source_location
  start_location() const
  { return this->start_location_; }

  // Return the end location.  This is normally the location of the
  // right curly brace which ends the block.
  source_location
  end_location() const
  { return this->end_location_; }

  // Add a statement to the block.
  void
  add_statement(Statement*);

  // Add a statement to the front of the block.
  void
  add_statement_at_front(Statement*);

  // Replace a statement in a block.
  void
  replace_statement(size_t index, Statement*);

  // Add a Statement before statement number INDEX.
  void
  insert_statement_before(size_t index, Statement*);

  // Add a Statement after statement number INDEX.
  void
  insert_statement_after(size_t index, Statement*);

  // Set the end location of the block.
  void
  set_end_location(source_location location)
  { this->end_location_ = location; }

  // Traverse the tree.
  int
  traverse(Traverse*);

  // Set final types for unspecified variables and constants.
  void
  determine_types();

  // Return true if execution of this block may fall through to the
  // next block.
  bool
  may_fall_through() const;

  // Return a tree of the code in this block.
  tree
  get_tree(Translate_context*);

  // Iterate over statements.

  typedef std::vector<Statement*>::iterator iterator;

  iterator
  begin()
  { return this->statements_.begin(); }

  iterator
  end()
  { return this->statements_.end(); }

 private:
  // Enclosing block.
  Block* enclosing_;
  // Statements in the block.
  std::vector<Statement*> statements_;
  // Binding contour.
  Bindings* bindings_;
  // Location of start of block.
  source_location start_location_;
  // Location of end of block.
  source_location end_location_;
};

// A function.

class Function
{
 public:
  Function(Function_type* type, Function*, Block*, source_location);

  // Return the function's type.
  Function_type*
  type() const
  { return this->type_; }

  // Return the enclosing function if there is one.
  Function*
  enclosing()
  { return this->enclosing_; }

  // Set the enclosing function.  This is used when building thunks
  // for functions which call recover.
  void
  set_enclosing(Function* enclosing)
  {
    gcc_assert(this->enclosing_ == NULL);
    this->enclosing_ = enclosing;
  }

  // Create the named result variables in the outer block.
  void
  create_named_result_variables(Gogo*);

  // Update the named result variables when cloning a function which
  // calls recover.
  void
  update_named_result_variables();

  // Add a new field to the closure variable.
  void
  add_closure_field(Named_object* var, source_location loc)
  { this->closure_fields_.push_back(std::make_pair(var, loc)); }

  // Whether this function needs a closure.
  bool
  needs_closure() const
  { return !this->closure_fields_.empty(); }

  // Return the closure variable, creating it if necessary.  This is
  // passed to the function as a static chain parameter.
  Named_object*
  closure_var();

  // Set the closure variable.  This is used when building thunks for
  // functions which call recover.
  void
  set_closure_var(Named_object* v)
  {
    gcc_assert(this->closure_var_ == NULL);
    this->closure_var_ = v;
  }

  // Return the variable for a reference to field INDEX in the closure
  // variable.
  Named_object*
  enclosing_var(unsigned int index)
  {
    gcc_assert(index < this->closure_fields_.size());
    return closure_fields_[index].first;
  }

  // Set the type of the closure variable if there is one.
  void
  set_closure_type();

  // Get the block of statements associated with the function.
  Block*
  block() const
  { return this->block_; }

  // Get the location of the start of the function.
  source_location
  location() const
  { return this->location_; }

  // Return whether this function is actually a method.
  bool
  is_method() const;

  // Add a label definition to the function.
  Label*
  add_label_definition(const std::string& label_name, source_location);

  // Add a label reference to a function.
  Label*
  add_label_reference(const std::string& label_name);

  // Whether this function calls the predeclared recover function.
  bool
  calls_recover() const
  { return this->calls_recover_; }

  // Record that this function calls the predeclared recover function.
  // This is set during the lowering pass.
  void
  set_calls_recover()
  { this->calls_recover_ = true; }

  // Whether this is a recover thunk function.
  bool
  is_recover_thunk() const
  { return this->is_recover_thunk_; }

  // Record that this is a thunk built for a function which calls
  // recover.
  void
  set_is_recover_thunk()
  { this->is_recover_thunk_ = true; }

  // Whether this function already has a recover thunk.
  bool
  has_recover_thunk() const
  { return this->has_recover_thunk_; }

  // Record that this function already has a recover thunk.
  void
  set_has_recover_thunk()
  { this->has_recover_thunk_ = true; }

  // Swap with another function.  Used only for the thunk which calls
  // recover.
  void
  swap_for_recover(Function *);

  // Traverse the tree.
  int
  traverse(Traverse*);

  // Determine types in the function.
  void
  determine_types();

  // Return the function's decl given an identifier.
  tree
  get_or_make_decl(Gogo*, Named_object*, tree id);

  // Return the function's decl after it has been built.
  tree
  get_decl() const
  {
    gcc_assert(this->fndecl_ != NULL);
    return this->fndecl_;
  }

  // Set the function decl to hold a tree of the function code.
  void
  build_tree(Gogo*, Named_object*);

  // Get the value to return when not explicitly specified.  May also
  // add statements to execute first to STMT_LIST.
  tree
  return_value(Gogo*, Named_object*, source_location, tree* stmt_list) const;

  // Get a tree for the variable holding the defer stack.
  tree
  defer_stack(source_location);

  // Export the function.
  void
  export_func(Export*, const std::string& name) const;

  // Export a function with a type.
  static void
  export_func_with_type(Export*, const std::string& name,
			const Function_type*);

  // Import a function.
  static void
  import_func(Import*, std::string* pname, Typed_identifier** receiver,
	      Typed_identifier_list** pparameters,
	      Typed_identifier_list** presults, bool* is_varargs);

 private:
  // Type for mapping from label names to Label objects.
  typedef Unordered_map(std::string, Label*) Labels;

  tree
  make_receiver_parm_decl(Gogo*, Named_object*, tree);

  tree
  copy_parm_to_heap(Gogo*, Named_object*, tree);

  void
  build_defer_wrapper(Gogo*, Named_object*, tree*, tree*);

  typedef std::vector<Named_object*> Named_results;

  typedef std::vector<std::pair<Named_object*,
				source_location> > Closure_fields;

  // The function's type.
  Function_type* type_;
  // The enclosing function.  This is NULL when there isn't one, which
  // is the normal case.
  Function* enclosing_;
  // The named result variables, if any.
  Named_results* named_results_;
  // If there is a closure, this is the list of variables which appear
  // in the closure.  This is created by the parser, and then resolved
  // to a real type when we lower parse trees.
  Closure_fields closure_fields_;
  // The closure variable, passed as a parameter using the static
  // chain parameter.  Normally NULL.
  Named_object* closure_var_;
  // The outer block of statements in the function.
  Block* block_;
  // The source location of the start of the function.
  source_location location_;
  // Labels defined or referenced in the function.
  Labels labels_;
  // The function decl.
  tree fndecl_;
  // A variable holding the defer stack variable.  This is NULL unless
  // we actually need a defer stack.
  tree defer_stack_;
  // True if this function calls the predeclared recover function.
  bool calls_recover_;
  // True if this a thunk built for a function which calls recover.
  bool is_recover_thunk_;
  // True if this function already has a recover thunk.
  bool has_recover_thunk_;
};

// A function declaration.

class Function_declaration
{
 public:
  Function_declaration(Function_type* fntype, source_location location)
    : fntype_(fntype), location_(location), asm_name_(), fndecl_(NULL)
  { }

  Function_type*
  type() const
  { return this->fntype_; }

  source_location
  location() const
  { return this->location_; }

  const std::string&
  asm_name() const
  { return this->asm_name_; }

  // Set the assembler name.
  void
  set_asm_name(const std::string& asm_name)
  { this->asm_name_ = asm_name; }

  // Return a decl for the function given an identifier.
  tree
  get_or_make_decl(Gogo*, Named_object*, tree id);

  // Export a function declaration.
  void
  export_func(Export* exp, const std::string& name) const
  { Function::export_func_with_type(exp, name, this->fntype_); }

 private:
  // The type of the function.
  Function_type* fntype_;
  // The location of the declaration.
  source_location location_;
  // The assembler name: this is the name to use in references to the
  // function.  This is normally empty.
  std::string asm_name_;
  // The function decl if needed.
  tree fndecl_;
};

// A variable.

class Variable
{
 public:
  Variable(Type*, Expression*, bool is_global, bool is_parameter,
	   bool is_receiver, source_location);

  // Get the type of the variable.
  Type*
  type();

  Type*
  type() const;

  // Return whether the type is defined yet.
  bool
  has_type() const
  { return this->type_ != NULL; }

  // Get the initial value.
  Expression*
  init() const
  { return this->init_; }

  // Return whether there are any preinit statements.
  bool
  has_pre_init() const
  { return this->preinit_ != NULL; }

  // Return the preinit statements if any.
  Block*
  preinit() const
  { return this->preinit_; }

  // Return whether this is a global variable.
  bool
  is_global() const
  { return this->is_global_; }

  // Return whether this is a function parameter.
  bool
  is_parameter() const
  { return this->is_parameter_; }

  // Return whether this is the receiver parameter of a method.
  bool
  is_receiver() const
  { return this->is_receiver_; }

  // Change this parameter to be a receiver.  This is used when
  // creating the thunks created for functions which call recover.
  void
  set_is_receiver()
  {
    gcc_assert(this->is_parameter_);
    this->is_receiver_ = true;
  }

  // Change this parameter to not be a receiver.  This is used when
  // creating the thunks created for functions which call recover.
  void
  set_is_not_receiver()
  {
    gcc_assert(this->is_parameter_);
    this->is_receiver_ = false;
  }

  // Return whether this is the varargs parameter of a function.
  bool
  is_varargs_parameter() const
  { return this->is_varargs_parameter_; }

  // Whether this variable's address is taken.
  bool
  is_address_taken() const
  { return this->is_address_taken_; }

  // Whether this variable should live in the heap.
  bool
  is_in_heap() const
  { return this->is_address_taken_ && !this->is_global_; }

  // Get the source location of the variable's declaration.
  source_location
  location() const
  { return this->location_; }

  // Record that this is the varargs parameter of a function.
  void
  set_is_varargs_parameter()
  {
    gcc_assert(this->is_parameter_);
    this->is_varargs_parameter_ = true;
  }

  // Clear the initial value; used for error handling.
  void
  clear_init()
  { this->init_ = NULL; }

  // Set the initial value; used for converting shortcuts.
  void
  set_init(Expression* init)
  { this->init_ = init; }

  // Get the preinit block, a block of statements to be run before the
  // initialization expression.
  Block*
  preinit_block(Gogo*);

  // Add a statement to be run before the initialization expression.
  // This is only used for global variables.
  void
  add_preinit_statement(Gogo*, Statement*);

  // Lower the initialization expression after parsing is complete.
  void
  lower_init_expression(Gogo*, Named_object*);

  // A special case: the init value is used only to determine the
  // type.  This is used if the variable is defined using := with the
  // comma-ok form of a map index or a receive expression.  The init
  // value is actually the map index expression or receive expression.
  // We use this because we may not know the right type at parse time.
  void
  set_type_from_init_tuple()
  { this->type_from_init_tuple_ = true; }

  // Another special case: the init value is used only to determine
  // the type.  This is used if the variable is defined using := with
  // a range clause.  The init value is the range expression.  The
  // type of the variable is the index type of the range expression
  // (i.e., the first value returned by a range).
  void
  set_type_from_range_index()
  { this->type_from_range_index_ = true; }

  // Another special case: like set_type_from_range_index, but the
  // type is the value type of the range expression (i.e., the second
  // value returned by a range).
  void
  set_type_from_range_value()
  { this->type_from_range_value_ = true; }

  // Another special case: the init value is used only to determine
  // the type.  This is used if the variable is defined using := with
  // a case in a select statement.  The init value is the channel.
  // The type of the variable is the channel's element type.
  void
  set_type_from_chan_element()
  { this->type_from_chan_element_ = true; }

  // After we lower the select statement, we once again set the type
  // from the initialization expression.
  void
  clear_type_from_chan_element()
  {
    gcc_assert(this->type_from_chan_element_);
    this->type_from_chan_element_ = false;
  }

  // Note that this variable was created for a type switch clause.
  void
  set_is_type_switch_var()
  { this->is_type_switch_var_ = true; }

  // Traverse the initializer expression.
  int
  traverse_expression(Traverse*);

  // Determine the type of the variable if necessary.
  void
  determine_type();

  // Note that something takes the address of this variable.
  void
  set_address_taken()
  { this->is_address_taken_ = true; }

  // Get the initial value of the variable as a tree.  This may only
  // be called if has_pre_init() returns false.
  tree
  get_init_tree(Gogo*, Named_object* function);

  // Return a series of statements which sets the value of the
  // variable in DECL.  This should only be called is has_pre_init()
  // returns true.  DECL may be NULL for a sink variable.
  tree
  get_init_block(Gogo*, Named_object* function, tree decl);

  // Export the variable.
  void
  export_var(Export*, const std::string& name) const;

  // Import a variable.
  static void
  import_var(Import*, std::string* pname, Type** ptype);

 private:
  // The type of a tuple.
  Type*
  type_from_tuple(Expression*, bool) const;

  // The type of a range.
  Type*
  type_from_range(Expression*, bool, bool) const;

  // The element type of a channel.
  Type*
  type_from_chan_element(Expression*, bool) const;

  // The variable's type.  This may be NULL if the type is set from
  // the expression.
  Type* type_;
  // The initial value.  This may be NULL if the variable should be
  // initialized to the default value for the type.
  Expression* init_;
  // Statements to run before the init statement.
  Block* preinit_;
  // Location of variable definition.
  source_location location_;
  // Whether this is a global variable.
  bool is_global_ : 1;
  // Whether this is a function parameter.
  bool is_parameter_ : 1;
  // Whether this is the receiver parameter of a method.
  bool is_receiver_ : 1;
  // Whether this is the varargs parameter of a function.
  bool is_varargs_parameter_ : 1;
  // Whether something takes the address of this variable.
  bool is_address_taken_ : 1;
  // True if we have seen this variable in a traversal.
  bool seen_ : 1;
  // True if we have lowered the initialization expression.
  bool init_is_lowered_ : 1;
  // True if init is a tuple used to set the type.
  bool type_from_init_tuple_ : 1;
  // True if init is a range clause and the type is the index type.
  bool type_from_range_index_ : 1;
  // True if init is a range clause and the type is the value type.
  bool type_from_range_value_ : 1;
  // True if init is a channel and the type is the channel's element type.
  bool type_from_chan_element_ : 1;
  // True if this is a variable created for a type switch case.
  bool is_type_switch_var_ : 1;
  // True if we have determined types.
  bool determined_type_ : 1;
};

// A variable which is really the name for a function return value, or
// part of one.

class Result_variable
{
 public:
  Result_variable(Type* type, Function* function, int index)
    : type_(type), function_(function), index_(index),
      is_address_taken_(false)
  { }

  // Get the type of the result variable.
  Type*
  type() const
  { return this->type_; }

  // Get the function that this is associated with.
  Function*
  function() const
  { return this->function_; }

  // Index in the list of function results.
  int
  index() const
  { return this->index_; }

  // Whether this variable's address is taken.
  bool
  is_address_taken() const
  { return this->is_address_taken_; }

  // Note that something takes the address of this variable.
  void
  set_address_taken()
  { this->is_address_taken_ = true; }

  // Whether this variable should live in the heap.
  bool
  is_in_heap() const
  { return this->is_address_taken_; }

  // Set the function.  This is used when cloning functions which call
  // recover.
  void
  set_function(Function* function)
  { this->function_ = function; }

 private:
  // Type of result variable.
  Type* type_;
  // Function with which this is associated.
  Function* function_;
  // Index in list of results.
  int index_;
  // Whether something takes the address of this variable.
  bool is_address_taken_;
};

// The value we keep for a named constant.  This lets us hold a type
// and an expression.

class Named_constant
{
 public:
  Named_constant(Type* type, Expression* expr, int iota_value,
		 source_location location)
    : type_(type), expr_(expr), iota_value_(iota_value), location_(location),
      lowering_(false)
  { }

  Type*
  type() const
  { return this->type_; }

  Expression*
  expr() const
  { return this->expr_; }

  int
  iota_value() const
  { return this->iota_value_; }

  source_location
  location() const
  { return this->location_; }

  // Whether we are lowering.
  bool
  lowering() const
  { return this->lowering_; }

  // Set that we are lowering.
  void
  set_lowering()
  { this->lowering_ = true; }

  // We are no longer lowering.
  void
  clear_lowering()
  { this->lowering_ = false; }

  // Traverse the expression.
  int
  traverse_expression(Traverse*);

  // Determine the type of the constant if necessary.
  void
  determine_type();

  // Indicate that we found and reported an error for this constant.
  void
  set_error();

  // Export the constant.
  void
  export_const(Export*, const std::string& name) const;

  // Import a constant.
  static void
  import_const(Import*, std::string*, Type**, Expression**);

 private:
  // The type of the constant.
  Type* type_;
  // The expression for the constant.
  Expression* expr_;
  // If the predeclared constant iota is used in EXPR_, this is the
  // value it will have.  We do this because at parse time we don't
  // know whether the name "iota" will refer to the predeclared
  // constant or to something else.  We put in the right value in when
  // we lower.
  int iota_value_;
  // The location of the definition.
  source_location location_;
  // Whether we are currently lowering this constant.
  bool lowering_;
};

// A type declaration.

class Type_declaration
{
 public:
  Type_declaration(source_location location)
    : location_(location), in_function_(NULL), methods_(),
      issued_warning_(false)
  { }

  // Return the location.
  source_location
  location() const
  { return this->location_; }

  // Return the function in which this type is declared.  This will
  // return NULL for a type declared in global scope.
  Named_object*
  in_function()
  { return this->in_function_; }

  // Set the function in which this type is declared.
  void
  set_in_function(Named_object* f)
  { this->in_function_ = f; }

  // Add a method to this type.  This is used when methods are defined
  // before the type.
  Named_object*
  add_method(const std::string& name, Function* function);

  // Add a method declaration to this type.
  Named_object*
  add_method_declaration(const std::string& name, Function_type* type,
			 source_location location);

  // Return whether any methods were defined.
  bool
  has_methods() const;

  // Define methods when the real type is known.
  void
  define_methods(Named_type*);

  // This is called if we are trying to use this type.  It returns
  // true if we should issue a warning.
  bool
  using_type();

 private:
  typedef std::vector<Named_object*> Methods;

  // The location of the type declaration.
  source_location location_;
  // If this type is declared in a function, a pointer back to the
  // function in which it is defined.
  Named_object* in_function_;
  // Methods defined before the type is defined.
  Methods methods_;
  // True if we have issued a warning about a use of this type
  // declaration when it is undefined.
  bool issued_warning_;
};

// An unknown object.  These are created by the parser for forward
// references to names which have not been seen before.  In a correct
// program, these will always point to a real definition by the end of
// the parse.  Because they point to another Named_object, these may
// only be referenced by Unknown_expression objects.

class Unknown_name
{
 public:
  Unknown_name(source_location location)
    : location_(location), real_named_object_(NULL)
  { }

  // Return the location where this name was first seen.
  source_location
  location() const
  { return this->location_; }

  // Return the real named object that this points to, or NULL if it
  // was never resolved.
  Named_object*
  real_named_object() const
  { return this->real_named_object_; }

  // Set the real named object that this points to.
  void
  set_real_named_object(Named_object* no);

 private:
  // The location where this name was first seen.
  source_location location_;
  // The real named object when it is known.
  Named_object*
  real_named_object_;
};

// A named object named.  This is the result of a declaration.  We
// don't use a superclass because they all have to be handled
// differently.

class Named_object
{
 public:
  enum Classification
  {
    // An uninitialized Named_object.  We should never see this.
    NAMED_OBJECT_UNINITIALIZED,
    // An unknown name.  This is used for forward references.  In a
    // correct program, these will all be resolved by the end of the
    // parse.
    NAMED_OBJECT_UNKNOWN,
    // A const.
    NAMED_OBJECT_CONST,
    // A type.
    NAMED_OBJECT_TYPE,
    // A forward type declaration.
    NAMED_OBJECT_TYPE_DECLARATION,
    // A var.
    NAMED_OBJECT_VAR,
    // A result variable in a function.
    NAMED_OBJECT_RESULT_VAR,
    // The blank identifier--the special variable named _.
    NAMED_OBJECT_SINK,
    // A func.
    NAMED_OBJECT_FUNC,
    // A forward func declaration.
    NAMED_OBJECT_FUNC_DECLARATION,
    // A package.
    NAMED_OBJECT_PACKAGE
  };

  // Return the classification.
  Classification
  classification() const
  { return this->classification_; }

  // Classifiers.

  bool
  is_unknown() const
  { return this->classification_ == NAMED_OBJECT_UNKNOWN; }

  bool
  is_const() const
  { return this->classification_ == NAMED_OBJECT_CONST; }

  bool
  is_type() const
  { return this->classification_ == NAMED_OBJECT_TYPE; }

  bool
  is_type_declaration() const
  { return this->classification_ == NAMED_OBJECT_TYPE_DECLARATION; }

  bool
  is_variable() const
  { return this->classification_ == NAMED_OBJECT_VAR; }

  bool
  is_result_variable() const
  { return this->classification_ == NAMED_OBJECT_RESULT_VAR; }

  bool
  is_sink() const
  { return this->classification_ == NAMED_OBJECT_SINK; }

  bool
  is_function() const
  { return this->classification_ == NAMED_OBJECT_FUNC; }

  bool
  is_function_declaration() const
  { return this->classification_ == NAMED_OBJECT_FUNC_DECLARATION; }

  bool
  is_package() const
  { return this->classification_ == NAMED_OBJECT_PACKAGE; }

  // Creators.

  static Named_object*
  make_unknown_name(const std::string& name, source_location);

  static Named_object*
  make_constant(const Typed_identifier&, const Package*, Expression*,
		int iota_value);

  static Named_object*
  make_type(const std::string&, const Package*, Type*, source_location);

  static Named_object*
  make_type_declaration(const std::string&, const Package*, source_location);

  static Named_object*
  make_variable(const std::string&, const Package*, Variable*);

  static Named_object*
  make_result_variable(const std::string&, Result_variable*);

  static Named_object*
  make_sink();

  static Named_object*
  make_function(const std::string&, const Package*, Function*);

  static Named_object*
  make_function_declaration(const std::string&, const Package*, Function_type*,
			    source_location);

  static Named_object*
  make_package(const std::string& alias, Package* package);

  // Getters.

  Unknown_name*
  unknown_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_UNKNOWN);
    return this->u_.unknown_value;
  }

  const Unknown_name*
  unknown_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_UNKNOWN);
    return this->u_.unknown_value;
  }

  Named_constant*
  const_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_CONST);
    return this->u_.const_value;
  }

  const Named_constant*
  const_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_CONST);
    return this->u_.const_value;
  }

  Named_type*
  type_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_TYPE);
    return this->u_.type_value;
  }

  const Named_type*
  type_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_TYPE);
    return this->u_.type_value;
  }

  Type_declaration*
  type_declaration_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_TYPE_DECLARATION);
    return this->u_.type_declaration;
  }

  const Type_declaration*
  type_declaration_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_TYPE_DECLARATION);
    return this->u_.type_declaration;
  }

  Variable*
  var_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_VAR);
    return this->u_.var_value;
  }

  const Variable*
  var_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_VAR);
    return this->u_.var_value;
  }

  Result_variable*
  result_var_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_RESULT_VAR);
    return this->u_.result_var_value;
  }

  const Result_variable*
  result_var_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_RESULT_VAR);
    return this->u_.result_var_value;
  }

  Function*
  func_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_FUNC);
    return this->u_.func_value;
  }

  const Function*
  func_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_FUNC);
    return this->u_.func_value;
  }

  Function_declaration*
  func_declaration_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_FUNC_DECLARATION);
    return this->u_.func_declaration_value;
  }

  const Function_declaration*
  func_declaration_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_FUNC_DECLARATION);
    return this->u_.func_declaration_value;
  }

  Package*
  package_value()
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_PACKAGE);
    return this->u_.package_value;
  }

  const Package*
  package_value() const
  {
    gcc_assert(this->classification_ == NAMED_OBJECT_PACKAGE);
    return this->u_.package_value;
  }

  const std::string&
  name() const
  { return this->name_; }

  // Return the name to use in an error message.  The difference is
  // that if this Named_object is defined in a different package, this
  // will return PACKAGE.NAME.
  std::string
  message_name() const;

  const Package*
  package() const
  { return this->package_; }

  // Resolve an unknown value if possible.  This returns the same
  // Named_object or a new one.
  Named_object*
  resolve()
  {
    Named_object* ret = this;
    if (this->is_unknown())
      {
	Named_object* r = this->unknown_value()->real_named_object();
	if (r != NULL)
	  ret = r;
      }
    return ret;
  }

  const Named_object*
  resolve() const
  {
    const Named_object* ret = this;
    if (this->is_unknown())
      {
	const Named_object* r = this->unknown_value()->real_named_object();
	if (r != NULL)
	  ret = r;
      }
    return ret;
  }

  // The location where this object was defined or referenced.
  source_location
  location() const;

  // Return a tree for the external identifier for this object.
  tree
  get_id(Gogo*);

  // Return a tree representing this object.
  tree
  get_tree(Gogo*, Named_object* function);

  // Define a type declaration.
  void
  set_type_value(Named_type*);

  // Define a function declaration.
  void
  set_function_value(Function*);

  // Declare an unknown name as a type declaration.
  void
  declare_as_type();

  // Export this object.
  void
  export_named_object(Export*) const;

 private:
  Named_object(const std::string&, const Package*, Classification);

  // The name of the object.
  std::string name_;
  // The package that this object is in.  This is NULL if it is in the
  // file we are compiling.
  const Package* package_;
  // The type of object this is.
  Classification classification_;
  // The real data.
  union
  {
    Unknown_name* unknown_value;
    Named_constant* const_value;
    Named_type* type_value;
    Type_declaration* type_declaration;
    Variable* var_value;
    Result_variable* result_var_value;
    Function* func_value;
    Function_declaration* func_declaration_value;
    Package* package_value;
  } u_;
  // The DECL tree for this object if we have already converted it.
  tree tree_;
};

// A binding contour.  This binds names to objects.

class Bindings
{
 public:
  // Type for mapping from names to objects.
  typedef Unordered_map(std::string, Named_object*) Contour;

  Bindings(Bindings* enclosing);

  // Add an unknown name.
  Named_object*
  add_unknown_name(const std::string& name, source_location location)
  {
    return this->add_named_object(Named_object::make_unknown_name(name,
								  location));
  }

  // Add a constant.
  Named_object*
  add_constant(const Typed_identifier& tid, const Package* package,
	       Expression* expr, int iota_value)
  {
    return this->add_named_object(Named_object::make_constant(tid, package,
							      expr,
							      iota_value));
  }

  // Add a type.
  Named_object*
  add_type(const std::string& name, const Package* package, Type* type,
	   source_location location)
  {
    return this->add_named_object(Named_object::make_type(name, package, type,
							  location));
  }

  // Add a named type.  This is used for builtin types, and to add an
  // imported type to the global scope.
  Named_object*
  add_named_type(Named_type* named_type);

  // Add a type declaration.
  Named_object*
  add_type_declaration(const std::string& name, const Package* package,
		       source_location location)
  {
    Named_object* no = Named_object::make_type_declaration(name, package,
							   location);
    return this->add_named_object(no);
  }

  // Add a variable.
  Named_object*
  add_variable(const std::string& name, const Package* package,
	       Variable* variable)
  {
    return this->add_named_object(Named_object::make_variable(name, package,
							      variable));
  }

  // Add a result variable.
  Named_object*
  add_result_variable(const std::string& name, Result_variable* result)
  {
    return this->add_named_object(Named_object::make_result_variable(name,
								     result));
  }

  // Add a function.
  Named_object*
  add_function(const std::string& name, const Package*, Function* function);

  // Add a function declaration.
  Named_object*
  add_function_declaration(const std::string& name, const Package* package,
			   Function_type* type, source_location location);

  // Add a package.  The location is the location of the import
  // statement.
  Named_object*
  add_package(const std::string& alias, Package* package)
  {
    Named_object* no = Named_object::make_package(alias, package);
    return this->add_named_object(no);
  }

  // Define a type which was already declared.
  void
  define_type(Named_object*, Named_type*);

  // Add a method to the list of objects.  This is not added to the
  // lookup table.
  void
  add_method(Named_object*);

  // Add a named object to this binding.
  Named_object*
  add_named_object(Named_object* no)
  { return this->add_named_object_to_contour(&this->bindings_, no); }

  // Clear all names in file scope from the bindings.
  void
  clear_file_scope();

  // Look up a name in this binding contour and in any enclosing
  // binding contours.  This returns NULL if the name is not found.
  Named_object*
  lookup(const std::string&) const;

  // Look up a name in this binding contour without looking in any
  // enclosing binding contours.  Returns NULL if the name is not found.
  Named_object*
  lookup_local(const std::string&) const;

  // Remove a name.
  void
  remove_binding(Named_object*);

  // Traverse the tree.  See the Traverse class.
  int
  traverse(Traverse*, bool is_global);

  // Iterate over definitions.  This does not include things which
  // were only declared.

  typedef std::vector<Named_object*>::const_iterator
    const_definitions_iterator;

  const_definitions_iterator
  begin_definitions() const
  { return this->named_objects_.begin(); }

  const_definitions_iterator
  end_definitions() const
  { return this->named_objects_.end(); }

  // Return the number of definitions.
  size_t
  size_definitions() const
  { return this->named_objects_.size(); }

  // Return whether there are no definitions.
  bool
  empty_definitions() const
  { return this->named_objects_.empty(); }

  // Iterate over declarations.  This is everything that has been
  // declared, which includes everything which has been defined.

  typedef Contour::const_iterator const_declarations_iterator;

  const_declarations_iterator
  begin_declarations() const
  { return this->bindings_.begin(); }

  const_declarations_iterator
  end_declarations() const
  { return this->bindings_.end(); }

  // Return the number of declarations.
  size_t
  size_declarations() const
  { return this->bindings_.size(); }

  // Return whether there are no declarations.
  bool
  empty_declarations() const
  { return this->bindings_.empty(); }

  // Return the first declaration.
  Named_object*
  first_declaration()
  { return this->bindings_.empty() ? NULL : this->bindings_.begin()->second; }

 private:
  Named_object*
  add_named_object_to_contour(Contour*, Named_object*);

  Named_object*
  new_definition(Named_object*, Named_object*);

  // Enclosing bindings.
  Bindings* enclosing_;
  // The list of objects.
  std::vector<Named_object*> named_objects_;
  // The mapping from names to objects.
  Contour bindings_;
};

// A label.

class Label
{
 public:
  Label(const std::string& name)
    : name_(name), location_(0), decl_(NULL)
  { }

  // Return the label's name.
  const std::string&
  name() const
  { return this->name_; }

  // Return whether the label has been defined.
  bool
  is_defined() const
  { return this->location_ != 0; }

  // Return the location of the definition.
  source_location
  location() const
  { return this->location_; }

  // Define the label at LOCATION.
  void
  define(source_location location)
  {
    gcc_assert(this->location_ == 0);
    this->location_ = location;
  }

  // Return the LABEL_DECL for this decl.
  tree
  get_decl();

  // Return an expression for the address of this label.
  tree
  get_addr(source_location location);

 private:
  // The name of the label.
  std::string name_;
  // The location of the definition.  This is 0 if the label has not
  // yet been defined.
  source_location location_;
  // The LABEL_DECL.
  tree decl_;
};

// An unnamed label.  These are used when lowering loops.

class Unnamed_label
{
 public:
  Unnamed_label(source_location location)
    : location_(location), decl_(NULL)
  { }

  // Get the location where the label is defined.
  source_location
  location() const
  { return this->location_; }

  // Set the location where the label is defined.
  void
  set_location(source_location location)
  { this->location_ = location; }

  // Return a statement which defines this label.
  tree
  get_definition();

  // Return a goto to this label from LOCATION.
  tree
  get_goto(source_location location);

 private:
  // Return the LABEL_DECL to use with GOTO_EXPR.
  tree
  get_decl();

  // The location where the label is defined.
  source_location location_;
  // The LABEL_DECL.
  tree decl_;
};

// An imported package.

class Package
{
 public:
  Package(const std::string& name, const std::string& unique_prefix,
	  source_location location);

  // The real name of this package.  This may be different from the
  // name in the associated Named_object if the import statement used
  // an alias.
  const std::string&
  name() const
  { return this->name_; }

  // Return the location of the import statement.
  source_location
  location() const
  { return this->location_; }

  // Get the unique prefix used for all symbols exported from this
  // package.
  const std::string&
  unique_prefix() const
  {
    gcc_assert(!this->unique_prefix_.empty());
    return this->unique_prefix_;
  }

  // The priority of this package.  The init function of packages with
  // lower priority must be run before the init function of packages
  // with higher priority.
  int
  priority() const
  { return this->priority_; }

  // Set the priority.
  void
  set_priority(int priority);

  // Return the bindings.
  Bindings*
  bindings()
  { return this->bindings_; }

  // Whether some symbol from the package was used.
  bool
  used() const
  { return this->used_; }

  // Note that some symbol from this package was used.
  void
  set_used() const
  { this->used_ = true; }

  // Clear the used field for the next file.
  void
  clear_used()
  { this->used_ = false; }

  // Whether this package was imported in the current file.
  bool
  is_imported() const
  { return this->is_imported_; }

  // Note that this package was imported in the current file.
  void
  set_is_imported()
  { this->is_imported_ = true; }

  // Clear the imported field for the next file.
  void
  clear_is_imported()
  { this->is_imported_ = false; }

  // Whether this package was imported with a name of "_".
  bool
  uses_sink_alias() const
  { return this->uses_sink_alias_; }

  // Note that this package was imported with a name of "_".
  void
  set_uses_sink_alias()
  { this->uses_sink_alias_ = true; }

  // Clear the sink alias field for the next file.
  void
  clear_uses_sink_alias()
  { this->uses_sink_alias_ = false; }

  // Look up a name in the package.  Returns NULL if the name is not
  // found.
  Named_object*
  lookup(const std::string& name) const
  { return this->bindings_->lookup(name); }

  // Set the location of the package.  This is used if it is seen in a
  // different import before it is really imported.
  void
  set_location(source_location location)
  { this->location_ = location; }

  // Add a constant to the package.
  Named_object*
  add_constant(const Typed_identifier& tid, Expression* expr)
  { return this->bindings_->add_constant(tid, this, expr, 0); }

  // Add a type to the package.
  Named_object*
  add_type(const std::string& name, Type* type, source_location location)
  { return this->bindings_->add_type(name, this, type, location); }

  // Add a type declaration to the package.
  Named_object*
  add_type_declaration(const std::string& name, source_location location)
  { return this->bindings_->add_type_declaration(name, this, location); }

  // Add a variable to the package.
  Named_object*
  add_variable(const std::string& name, Variable* variable)
  { return this->bindings_->add_variable(name, this, variable); }

  // Add a function declaration to the package.
  Named_object*
  add_function_declaration(const std::string& name, Function_type* type,
			   source_location loc)
  { return this->bindings_->add_function_declaration(name, this, type, loc); }

  // Determine types of constants.
  void
  determine_types();

 private:
  // The real name of this package.
  std::string name_;
  // The unique prefix for all exported global symbols.
  std::string unique_prefix_;
  // The names in this package.
  Bindings* bindings_;
  // The priority of this package.  A package has a priority higher
  // than the priority of all of the packages that it imports.  This
  // is used to run init functions in the right order.
  int priority_;
  // The location of the import statement.
  source_location location_;
  // True if some name from this package was used.  This is mutable
  // because we can use a package even if we have a const pointer to
  // it.
  mutable bool used_;
  // True if this package was imported in the current file.
  bool is_imported_;
  // True if this package was imported with a name of "_".
  bool uses_sink_alias_;
};

// Return codes for the traversal functions.  This is not an enum
// because we want to be able to declare traversal functions in other
// header files without including this one.

// Continue traversal as usual.
const int TRAVERSE_CONTINUE = -1;

// Exit traversal.
const int TRAVERSE_EXIT = 0;

// Continue traversal, but skip components of the current object.
// E.g., if this is returned by Traverse::statement, we do not
// traverse the expressions in the statement even if
// traverse_expressions is set in the traverse_mask.
const int TRAVERSE_SKIP_COMPONENTS = 1;

// This class is used when traversing the parse tree.  The caller uses
// a subclass which overrides functions as desired.

class Traverse
{
 public:
  // These bitmasks say what to traverse.
  static const unsigned int traverse_variables =    0x1;
  static const unsigned int traverse_constants =    0x2;
  static const unsigned int traverse_functions =    0x4;
  static const unsigned int traverse_blocks =       0x8;
  static const unsigned int traverse_statements =  0x10;
  static const unsigned int traverse_expressions = 0x20;
  static const unsigned int traverse_types =       0x40;

  Traverse(unsigned int traverse_mask)
    : traverse_mask_(traverse_mask), types_seen_(NULL), expressions_seen_(NULL)
  { }

  virtual ~Traverse();

  // The bitmask of what to traverse.
  unsigned int
  traverse_mask() const
  { return this->traverse_mask_; }

  // Record that we are going to traverse a type.  This returns true
  // if the type has already been seen in this traversal.  This is
  // required because types, unlike expressions, can form a circular
  // graph.
  bool
  remember_type(const Type*);

  // Record that we are going to see an expression.  This returns true
  // if the expression has already been seen in this traversal.  This
  // is only needed for cases where multiple expressions can point to
  // a single one.
  bool
  remember_expression(const Expression*);

  // These functions return one of the TRAVERSE codes defined above.

  // If traverse_variables is set in the mask, this is called for
  // every variable in the tree.
  virtual int
  variable(Named_object*);

  // If traverse_constants is set in the mask, this is called for
  // every named constant in the tree.  The bool parameter is true for
  // a global constant.
  virtual int
  constant(Named_object*, bool);

  // If traverse_functions is set in the mask, this is called for
  // every function in the tree.
  virtual int
  function(Named_object*);

  // If traverse_blocks is set in the mask, this is called for every
  // block in the tree.
  virtual int
  block(Block*);

  // If traverse_statements is set in the mask, this is called for
  // every statement in the tree.
  virtual int
  statement(Block*, size_t* index, Statement*);

  // If traverse_expressions is set in the mask, this is called for
  // every expression in the tree.
  virtual int
  expression(Expression**);

  // If traverse_types is set in the mask, this is called for every
  // type in the tree.
  virtual int
  type(Type*);

 private:
  typedef Unordered_set_hash(const Type*, Type_hash_identical,
			     Type_identical) Types_seen;

  typedef Unordered_set(const Expression*) Expressions_seen;

  // Bitmask of what sort of objects to traverse.
  unsigned int traverse_mask_;
  // Types which have been seen in this traversal.
  Types_seen* types_seen_;
  // Expressions which have been seen in this traversal.
  Expressions_seen* expressions_seen_;
};

// When translating the gogo IR into trees, this is the context we
// pass down the blocks and statements.

class Translate_context
{
 public:
  Translate_context(Gogo* gogo, Named_object* function, Block* block,
		    tree block_tree)
    : gogo_(gogo), function_(function), block_(block), block_tree_(block_tree),
      is_const_(false)
  { }

  // Accessors.

  Gogo*
  gogo()
  { return this->gogo_; }

  Named_object*
  function()
  { return this->function_; }

  Block*
  block()
  { return this->block_; }

  tree
  block_tree()
  { return this->block_tree_; }

  bool
  is_const()
  { return this->is_const_; }

  // Make a constant context.
  void
  set_is_const()
  { this->is_const_ = true; }

 private:
  // The IR for the entire compilation unit.
  Gogo* gogo_;
  // The function we are currently translating.
  Named_object* function_;
  // The block we are currently translating.
  Block *block_;
  // The BLOCK node for the current block.
  tree block_tree_;
  // Whether this is being evaluated in a constant context.  This is
  // used for type descriptor initializers.
  bool is_const_;
};

// Runtime error codes.  These must match the values in
// libgo/runtime/go-runtime-error.c.

// Slice index out of bounds: negative or larger than the length of
// the slice.
static const int RUNTIME_ERROR_SLICE_INDEX_OUT_OF_BOUNDS = 0;

// Array index out of bounds.
static const int RUNTIME_ERROR_ARRAY_INDEX_OUT_OF_BOUNDS = 1;

// String index out of bounds.
static const int RUNTIME_ERROR_STRING_INDEX_OUT_OF_BOUNDS = 2;

// Slice slice out of bounds: negative or larger than the length of
// the slice or high bound less than low bound.
static const int RUNTIME_ERROR_SLICE_SLICE_OUT_OF_BOUNDS = 3;

// Array slice out of bounds.
static const int RUNTIME_ERROR_ARRAY_SLICE_OUT_OF_BOUNDS = 4;

// String slice out of bounds.
static const int RUNTIME_ERROR_STRING_SLICE_OUT_OF_BOUNDS = 5;

// Dereference of nil pointer.  This is used when there is a
// dereference of a pointer to a very large struct or array, to ensure
// that a gigantic array is not used a proxy to access random memory
// locations.
static const int RUNTIME_ERROR_NIL_DEREFERENCE = 6;

// Slice length or capacity out of bounds in make: negative or
// overflow or length greater than capacity.
static const int RUNTIME_ERROR_MAKE_SLICE_OUT_OF_BOUNDS = 7;

// Map capacity out of bounds in make: negative or overflow.
static const int RUNTIME_ERROR_MAKE_MAP_OUT_OF_BOUNDS = 8;

// Channel capacity out of bounds in make: negative or overflow.
static const int RUNTIME_ERROR_MAKE_CHAN_OUT_OF_BOUNDS = 9;

// This is used by some of the langhooks.
extern Gogo* go_get_gogo();

// Whether we have seen any errors.  FIXME: Replace with a backend
// interface.
extern bool saw_errors();

#endif // !defined(GO_GOGO_H)
