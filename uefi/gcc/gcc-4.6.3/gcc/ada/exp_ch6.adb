------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                              E X P _ C H 6                               --
--                                                                          --
--                                 B o d y                                  --
--                                                                          --
--          Copyright (C) 1992-2010, Free Software Foundation, Inc.         --
--                                                                          --
-- GNAT is free software;  you can  redistribute it  and/or modify it under --
-- terms of the  GNU General Public License as published  by the Free Soft- --
-- ware  Foundation;  either version 3,  or (at your option) any later ver- --
-- sion.  GNAT is distributed in the hope that it will be useful, but WITH- --
-- OUT ANY WARRANTY;  without even the  implied warranty of MERCHANTABILITY --
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License --
-- for  more details.  You should have  received  a copy of the GNU General --
-- Public License  distributed with GNAT; see file COPYING3.  If not, go to --
-- http://www.gnu.org/licenses for a complete copy of the license.          --
--                                                                          --
-- GNAT was originally developed  by the GNAT team at  New York University. --
-- Extensive contributions were provided by Ada Core Technologies Inc.      --
--                                                                          --
------------------------------------------------------------------------------

with Atree;    use Atree;
with Checks;   use Checks;
with Debug;    use Debug;
with Einfo;    use Einfo;
with Errout;   use Errout;
with Elists;   use Elists;
with Exp_Atag; use Exp_Atag;
with Exp_Ch2;  use Exp_Ch2;
with Exp_Ch3;  use Exp_Ch3;
with Exp_Ch7;  use Exp_Ch7;
with Exp_Ch9;  use Exp_Ch9;
with Exp_Dbug; use Exp_Dbug;
with Exp_Disp; use Exp_Disp;
with Exp_Dist; use Exp_Dist;
with Exp_Intr; use Exp_Intr;
with Exp_Pakd; use Exp_Pakd;
with Exp_Tss;  use Exp_Tss;
with Exp_Util; use Exp_Util;
with Exp_VFpt; use Exp_VFpt;
with Fname;    use Fname;
with Freeze;   use Freeze;
with Inline;   use Inline;
with Lib;      use Lib;
with Namet;    use Namet;
with Nlists;   use Nlists;
with Nmake;    use Nmake;
with Opt;      use Opt;
with Restrict; use Restrict;
with Rident;   use Rident;
with Rtsfind;  use Rtsfind;
with Sem;      use Sem;
with Sem_Aux;  use Sem_Aux;
with Sem_Ch6;  use Sem_Ch6;
with Sem_Ch8;  use Sem_Ch8;
with Sem_Ch12; use Sem_Ch12;
with Sem_Ch13; use Sem_Ch13;
with Sem_Eval; use Sem_Eval;
with Sem_Disp; use Sem_Disp;
with Sem_Dist; use Sem_Dist;
with Sem_Mech; use Sem_Mech;
with Sem_Res;  use Sem_Res;
with Sem_SCIL; use Sem_SCIL;
with Sem_Util; use Sem_Util;
with Sinfo;    use Sinfo;
with Snames;   use Snames;
with Stand;    use Stand;
with Targparm; use Targparm;
with Tbuild;   use Tbuild;
with Uintp;    use Uintp;
with Validsw;  use Validsw;

package body Exp_Ch6 is

   -----------------------
   -- Local Subprograms --
   -----------------------

   procedure Add_Access_Actual_To_Build_In_Place_Call
     (Function_Call : Node_Id;
      Function_Id   : Entity_Id;
      Return_Object : Node_Id;
      Is_Access     : Boolean := False);
   --  Ada 2005 (AI-318-02): Apply the Unrestricted_Access attribute to the
   --  object name given by Return_Object and add the attribute to the end of
   --  the actual parameter list associated with the build-in-place function
   --  call denoted by Function_Call. However, if Is_Access is True, then
   --  Return_Object is already an access expression, in which case it's passed
   --  along directly to the build-in-place function. Finally, if Return_Object
   --  is empty, then pass a null literal as the actual.

   procedure Add_Alloc_Form_Actual_To_Build_In_Place_Call
     (Function_Call  : Node_Id;
      Function_Id    : Entity_Id;
      Alloc_Form     : BIP_Allocation_Form := Unspecified;
      Alloc_Form_Exp : Node_Id             := Empty);
   --  Ada 2005 (AI-318-02): Add an actual indicating the form of allocation,
   --  if any, to be done by a build-in-place function. If Alloc_Form_Exp is
   --  present, then use it, otherwise pass a literal corresponding to the
   --  Alloc_Form parameter (which must not be Unspecified in that case).

   procedure Add_Extra_Actual_To_Call
     (Subprogram_Call : Node_Id;
      Extra_Formal    : Entity_Id;
      Extra_Actual    : Node_Id);
   --  Adds Extra_Actual as a named parameter association for the formal
   --  Extra_Formal in Subprogram_Call.

   procedure Add_Final_List_Actual_To_Build_In_Place_Call
     (Function_Call : Node_Id;
      Function_Id   : Entity_Id;
      Acc_Type      : Entity_Id;
      Sel_Comp      : Node_Id := Empty);
   --  Ada 2005 (AI-318-02): For a build-in-place call, if the result type has
   --  controlled parts, add an actual parameter that is a pointer to
   --  appropriate finalization list. The finalization list is that of the
   --  current scope, except for "new Acc'(F(...))" in which case it's the
   --  finalization list of the access type returned by the allocator. Acc_Type
   --  is that type in the allocator case; Empty otherwise. If Sel_Comp is
   --  not Empty, then it denotes a selected component and the finalization
   --  list is obtained from the _controller list of the prefix object.

   procedure Add_Task_Actuals_To_Build_In_Place_Call
     (Function_Call : Node_Id;
      Function_Id   : Entity_Id;
      Master_Actual : Node_Id);
   --  Ada 2005 (AI-318-02): For a build-in-place call, if the result type
   --  contains tasks, add two actual parameters: the master, and a pointer to
   --  the caller's activation chain. Master_Actual is the actual parameter
   --  expression to pass for the master. In most cases, this is the current
   --  master (_master). The two exceptions are: If the function call is the
   --  initialization expression for an allocator, we pass the master of the
   --  access type. If the function call is the initialization expression for a
   --  return object, we pass along the master passed in by the caller. The
   --  activation chain to pass is always the local one. Note: Master_Actual
   --  can be Empty, but only if there are no tasks.

   procedure Check_Overriding_Operation (Subp : Entity_Id);
   --  Subp is a dispatching operation. Check whether it may override an
   --  inherited private operation, in which case its DT entry is that of
   --  the hidden operation, not the one it may have received earlier.
   --  This must be done before emitting the code to set the corresponding
   --  DT to the address of the subprogram. The actual placement of Subp in
   --  the proper place in the list of primitive operations is done in
   --  Declare_Inherited_Private_Subprograms, which also has to deal with
   --  implicit operations. This duplication is unavoidable for now???

   procedure Detect_Infinite_Recursion (N : Node_Id; Spec : Entity_Id);
   --  This procedure is called only if the subprogram body N, whose spec
   --  has the given entity Spec, contains a parameterless recursive call.
   --  It attempts to generate runtime code to detect if this a case of
   --  infinite recursion.
   --
   --  The body is scanned to determine dependencies. If the only external
   --  dependencies are on a small set of scalar variables, then the values
   --  of these variables are captured on entry to the subprogram, and if
   --  the values are not changed for the call, we know immediately that
   --  we have an infinite recursion.

   procedure Expand_Actuals (N : Node_Id; Subp : Entity_Id);
   --  For each actual of an in-out or out parameter which is a numeric
   --  (view) conversion of the form T (A), where A denotes a variable,
   --  we insert the declaration:
   --
   --    Temp : T[ := T (A)];
   --
   --  prior to the call. Then we replace the actual with a reference to Temp,
   --  and append the assignment:
   --
   --    A := TypeA (Temp);
   --
   --  after the call. Here TypeA is the actual type of variable A. For out
   --  parameters, the initial declaration has no expression. If A is not an
   --  entity name, we generate instead:
   --
   --    Var  : TypeA renames A;
   --    Temp : T := Var;       --  omitting expression for out parameter.
   --    ...
   --    Var := TypeA (Temp);
   --
   --  For other in-out parameters, we emit the required constraint checks
   --  before and/or after the call.
   --
   --  For all parameter modes, actuals that denote components and slices of
   --  packed arrays are expanded into suitable temporaries.
   --
   --  For non-scalar objects that are possibly unaligned, add call by copy
   --  code (copy in for IN and IN OUT, copy out for OUT and IN OUT).

   procedure Expand_Inlined_Call
    (N         : Node_Id;
     Subp      : Entity_Id;
     Orig_Subp : Entity_Id);
   --  If called subprogram can be inlined by the front-end, retrieve the
   --  analyzed body, replace formals with actuals and expand call in place.
   --  Generate thunks for actuals that are expressions, and insert the
   --  corresponding constant declarations before the call. If the original
   --  call is to a derived operation, the return type is the one of the
   --  derived operation, but the body is that of the original, so return
   --  expressions in the body must be converted to the desired type (which
   --  is simply not noted in the tree without inline expansion).

   procedure Expand_Non_Function_Return (N : Node_Id);
   --  Called by Expand_N_Simple_Return_Statement in case we're returning from
   --  a procedure body, entry body, accept statement, or extended return
   --  statement.  Note that all non-function returns are simple return
   --  statements.

   function Expand_Protected_Object_Reference
     (N    : Node_Id;
      Scop : Entity_Id) return Node_Id;

   procedure Expand_Protected_Subprogram_Call
     (N    : Node_Id;
      Subp : Entity_Id;
      Scop : Entity_Id);
   --  A call to a protected subprogram within the protected object may appear
   --  as a regular call. The list of actuals must be expanded to contain a
   --  reference to the object itself, and the call becomes a call to the
   --  corresponding protected subprogram.

   function Is_Null_Procedure (Subp : Entity_Id) return Boolean;
   --  Predicate to recognize stubbed procedures and null procedures, which
   --  can be inlined unconditionally in all cases.

   procedure Expand_Simple_Function_Return (N : Node_Id);
   --  Expand simple return from function. In the case where we are returning
   --  from a function body this is called by Expand_N_Simple_Return_Statement.

   ----------------------------------------------
   -- Add_Access_Actual_To_Build_In_Place_Call --
   ----------------------------------------------

   procedure Add_Access_Actual_To_Build_In_Place_Call
     (Function_Call : Node_Id;
      Function_Id   : Entity_Id;
      Return_Object : Node_Id;
      Is_Access     : Boolean := False)
   is
      Loc            : constant Source_Ptr := Sloc (Function_Call);
      Obj_Address    : Node_Id;
      Obj_Acc_Formal : Entity_Id;

   begin
      --  Locate the implicit access parameter in the called function

      Obj_Acc_Formal := Build_In_Place_Formal (Function_Id, BIP_Object_Access);

      --  If no return object is provided, then pass null

      if not Present (Return_Object) then
         Obj_Address := Make_Null (Loc);
         Set_Parent (Obj_Address, Function_Call);

      --  If Return_Object is already an expression of an access type, then use
      --  it directly, since it must be an access value denoting the return
      --  object, and couldn't possibly be the return object itself.

      elsif Is_Access then
         Obj_Address := Return_Object;
         Set_Parent (Obj_Address, Function_Call);

      --  Apply Unrestricted_Access to caller's return object

      else
         Obj_Address :=
            Make_Attribute_Reference (Loc,
              Prefix         => Return_Object,
              Attribute_Name => Name_Unrestricted_Access);

         Set_Parent (Return_Object, Obj_Address);
         Set_Parent (Obj_Address, Function_Call);
      end if;

      Analyze_And_Resolve (Obj_Address, Etype (Obj_Acc_Formal));

      --  Build the parameter association for the new actual and add it to the
      --  end of the function's actuals.

      Add_Extra_Actual_To_Call (Function_Call, Obj_Acc_Formal, Obj_Address);
   end Add_Access_Actual_To_Build_In_Place_Call;

   --------------------------------------------------
   -- Add_Alloc_Form_Actual_To_Build_In_Place_Call --
   --------------------------------------------------

   procedure Add_Alloc_Form_Actual_To_Build_In_Place_Call
     (Function_Call  : Node_Id;
      Function_Id    : Entity_Id;
      Alloc_Form     : BIP_Allocation_Form := Unspecified;
      Alloc_Form_Exp : Node_Id             := Empty)
   is
      Loc               : constant Source_Ptr := Sloc (Function_Call);
      Alloc_Form_Actual : Node_Id;
      Alloc_Form_Formal : Node_Id;

   begin
      --  The allocation form generally doesn't need to be passed in the case
      --  of a constrained result subtype, since normally the caller performs
      --  the allocation in that case. However this formal is still needed in
      --  the case where the function has a tagged result, because generally
      --  such functions can be called in a dispatching context and such calls
      --  must be handled like calls to class-wide functions.

      if Is_Constrained (Underlying_Type (Etype (Function_Id)))
        and then not Is_Tagged_Type (Underlying_Type (Etype (Function_Id)))
      then
         return;
      end if;

      --  Locate the implicit allocation form parameter in the called function.
      --  Maybe it would be better for each implicit formal of a build-in-place
      --  function to have a flag or a Uint attribute to identify it. ???

      Alloc_Form_Formal := Build_In_Place_Formal (Function_Id, BIP_Alloc_Form);

      if Present (Alloc_Form_Exp) then
         pragma Assert (Alloc_Form = Unspecified);

         Alloc_Form_Actual := Alloc_Form_Exp;

      else
         pragma Assert (Alloc_Form /= Unspecified);

         Alloc_Form_Actual :=
           Make_Integer_Literal (Loc,
             Intval => UI_From_Int (BIP_Allocation_Form'Pos (Alloc_Form)));
      end if;

      Analyze_And_Resolve (Alloc_Form_Actual, Etype (Alloc_Form_Formal));

      --  Build the parameter association for the new actual and add it to the
      --  end of the function's actuals.

      Add_Extra_Actual_To_Call
        (Function_Call, Alloc_Form_Formal, Alloc_Form_Actual);
   end Add_Alloc_Form_Actual_To_Build_In_Place_Call;

   ------------------------------
   -- Add_Extra_Actual_To_Call --
   ------------------------------

   procedure Add_Extra_Actual_To_Call
     (Subprogram_Call : Node_Id;
      Extra_Formal    : Entity_Id;
      Extra_Actual    : Node_Id)
   is
      Loc         : constant Source_Ptr := Sloc (Subprogram_Call);
      Param_Assoc : Node_Id;

   begin
      Param_Assoc :=
        Make_Parameter_Association (Loc,
          Selector_Name             => New_Occurrence_Of (Extra_Formal, Loc),
          Explicit_Actual_Parameter => Extra_Actual);

      Set_Parent (Param_Assoc, Subprogram_Call);
      Set_Parent (Extra_Actual, Param_Assoc);

      if Present (Parameter_Associations (Subprogram_Call)) then
         if Nkind (Last (Parameter_Associations (Subprogram_Call))) =
              N_Parameter_Association
         then

            --  Find last named actual, and append

            declare
               L : Node_Id;
            begin
               L := First_Actual (Subprogram_Call);
               while Present (L) loop
                  if No (Next_Actual (L)) then
                     Set_Next_Named_Actual (Parent (L), Extra_Actual);
                     exit;
                  end if;
                  Next_Actual (L);
               end loop;
            end;

         else
            Set_First_Named_Actual (Subprogram_Call, Extra_Actual);
         end if;

         Append (Param_Assoc, To => Parameter_Associations (Subprogram_Call));

      else
         Set_Parameter_Associations (Subprogram_Call, New_List (Param_Assoc));
         Set_First_Named_Actual (Subprogram_Call, Extra_Actual);
      end if;
   end Add_Extra_Actual_To_Call;

   --------------------------------------------------
   -- Add_Final_List_Actual_To_Build_In_Place_Call --
   --------------------------------------------------

   procedure Add_Final_List_Actual_To_Build_In_Place_Call
     (Function_Call : Node_Id;
      Function_Id   : Entity_Id;
      Acc_Type      : Entity_Id;
      Sel_Comp      : Node_Id := Empty)
   is
      Loc               : constant Source_Ptr := Sloc (Function_Call);
      Final_List        : Node_Id;
      Final_List_Actual : Node_Id;
      Final_List_Formal : Node_Id;
      Is_Ctrl_Result    : constant Boolean :=
                            Needs_Finalization
                              (Underlying_Type (Etype (Function_Id)));

   begin
      --  No such extra parameter is needed if there are no controlled parts.
      --  The test for Needs_Finalization accounts for class-wide results
      --  (which potentially have controlled parts, even if the root type
      --  doesn't), and the test for a tagged result type is needed because
      --  calls to such a function can in general occur in dispatching
      --  contexts, which must be treated the same as a call to class-wide
      --  functions. Both of these situations require that a finalization list
      --  be passed.

      if not Needs_BIP_Final_List (Function_Id) then
         return;
      end if;

      --  Locate implicit finalization list parameter in the called function

      Final_List_Formal := Build_In_Place_Formal (Function_Id, BIP_Final_List);

      --  Create the actual which is a pointer to the appropriate finalization
      --  list. Acc_Type is present if and only if this call is the
      --  initialization of an allocator. Use the Current_Scope or the
      --  Acc_Type as appropriate.

      if Present (Acc_Type)
        and then (Ekind (Acc_Type) = E_Anonymous_Access_Type
                   or else
                     Present (Associated_Final_Chain (Base_Type (Acc_Type))))
      then
         Final_List := Find_Final_List (Acc_Type);

      --  If Sel_Comp is present and the function result is controlled, then
      --  the finalization list will be obtained from the _controller list of
      --  the selected component's prefix object.

      elsif Present (Sel_Comp) and then Is_Ctrl_Result then
         Final_List := Find_Final_List (Current_Scope, Sel_Comp);

      else
         Final_List := Find_Final_List (Current_Scope);
      end if;

      Final_List_Actual :=
        Make_Attribute_Reference (Loc,
          Prefix         => Final_List,
          Attribute_Name => Name_Unrestricted_Access);

      Analyze_And_Resolve (Final_List_Actual, Etype (Final_List_Formal));

      --  Build the parameter association for the new actual and add it to the
      --  end of the function's actuals.

      Add_Extra_Actual_To_Call
        (Function_Call, Final_List_Formal, Final_List_Actual);
   end Add_Final_List_Actual_To_Build_In_Place_Call;

   ---------------------------------------------
   -- Add_Task_Actuals_To_Build_In_Place_Call --
   ---------------------------------------------

   procedure Add_Task_Actuals_To_Build_In_Place_Call
     (Function_Call : Node_Id;
      Function_Id   : Entity_Id;
      Master_Actual : Node_Id)
   is
      Loc    : constant Source_Ptr := Sloc (Function_Call);
      Actual : Node_Id := Master_Actual;

   begin
      --  No such extra parameters are needed if there are no tasks

      if not Has_Task (Etype (Function_Id)) then
         return;
      end if;

      --  Use a dummy _master actual in case of No_Task_Hierarchy

      if Restriction_Active (No_Task_Hierarchy) then
         Actual := New_Occurrence_Of (RTE (RE_Library_Task_Level), Loc);
      end if;

      --  The master

      declare
         Master_Formal : Node_Id;
      begin
         --  Locate implicit master parameter in the called function

         Master_Formal := Build_In_Place_Formal (Function_Id, BIP_Master);

         Analyze_And_Resolve (Actual, Etype (Master_Formal));

         --  Build the parameter association for the new actual and add it to
         --  the end of the function's actuals.

         Add_Extra_Actual_To_Call
           (Function_Call, Master_Formal, Actual);
      end;

      --  The activation chain

      declare
         Activation_Chain_Actual : Node_Id;
         Activation_Chain_Formal : Node_Id;

      begin
         --  Locate implicit activation chain parameter in the called function

         Activation_Chain_Formal := Build_In_Place_Formal
           (Function_Id, BIP_Activation_Chain);

         --  Create the actual which is a pointer to the current activation
         --  chain

         Activation_Chain_Actual :=
           Make_Attribute_Reference (Loc,
             Prefix         => Make_Identifier (Loc, Name_uChain),
             Attribute_Name => Name_Unrestricted_Access);

         Analyze_And_Resolve
           (Activation_Chain_Actual, Etype (Activation_Chain_Formal));

         --  Build the parameter association for the new actual and add it to
         --  the end of the function's actuals.

         Add_Extra_Actual_To_Call
           (Function_Call, Activation_Chain_Formal, Activation_Chain_Actual);
      end;
   end Add_Task_Actuals_To_Build_In_Place_Call;

   -----------------------
   -- BIP_Formal_Suffix --
   -----------------------

   function BIP_Formal_Suffix (Kind : BIP_Formal_Kind) return String is
   begin
      case Kind is
         when BIP_Alloc_Form       =>
            return "BIPalloc";
         when BIP_Final_List       =>
            return "BIPfinallist";
         when BIP_Master           =>
            return "BIPmaster";
         when BIP_Activation_Chain =>
            return "BIPactivationchain";
         when BIP_Object_Access    =>
            return "BIPaccess";
      end case;
   end BIP_Formal_Suffix;

   ---------------------------
   -- Build_In_Place_Formal --
   ---------------------------

   function Build_In_Place_Formal
     (Func : Entity_Id;
      Kind : BIP_Formal_Kind) return Entity_Id
   is
      Extra_Formal : Entity_Id := Extra_Formals (Func);

   begin
      --  Maybe it would be better for each implicit formal of a build-in-place
      --  function to have a flag or a Uint attribute to identify it. ???

      loop
         pragma Assert (Present (Extra_Formal));
         exit when
           Chars (Extra_Formal) =
             New_External_Name (Chars (Func), BIP_Formal_Suffix (Kind));
         Next_Formal_With_Extras (Extra_Formal);
      end loop;

      return Extra_Formal;
   end Build_In_Place_Formal;

   --------------------------------
   -- Check_Overriding_Operation --
   --------------------------------

   procedure Check_Overriding_Operation (Subp : Entity_Id) is
      Typ     : constant Entity_Id := Find_Dispatching_Type (Subp);
      Op_List : constant Elist_Id  := Primitive_Operations (Typ);
      Op_Elmt : Elmt_Id;
      Prim_Op : Entity_Id;
      Par_Op  : Entity_Id;

   begin
      if Is_Derived_Type (Typ)
        and then not Is_Private_Type (Typ)
        and then In_Open_Scopes (Scope (Etype (Typ)))
        and then Is_Base_Type (Typ)
      then
         --  Subp overrides an inherited private operation if there is an
         --  inherited operation with a different name than Subp (see
         --  Derive_Subprogram) whose Alias is a hidden subprogram with the
         --  same name as Subp.

         Op_Elmt := First_Elmt (Op_List);
         while Present (Op_Elmt) loop
            Prim_Op := Node (Op_Elmt);
            Par_Op  := Alias (Prim_Op);

            if Present (Par_Op)
              and then not Comes_From_Source (Prim_Op)
              and then Chars (Prim_Op) /= Chars (Par_Op)
              and then Chars (Par_Op) = Chars (Subp)
              and then Is_Hidden (Par_Op)
              and then Type_Conformant (Prim_Op, Subp)
            then
               Set_DT_Position (Subp, DT_Position (Prim_Op));
            end if;

            Next_Elmt (Op_Elmt);
         end loop;
      end if;
   end Check_Overriding_Operation;

   -------------------------------
   -- Detect_Infinite_Recursion --
   -------------------------------

   procedure Detect_Infinite_Recursion (N : Node_Id; Spec : Entity_Id) is
      Loc : constant Source_Ptr := Sloc (N);

      Var_List : constant Elist_Id := New_Elmt_List;
      --  List of globals referenced by body of procedure

      Call_List : constant Elist_Id := New_Elmt_List;
      --  List of recursive calls in body of procedure

      Shad_List : constant Elist_Id := New_Elmt_List;
      --  List of entity id's for entities created to capture the value of
      --  referenced globals on entry to the procedure.

      Scop : constant Uint := Scope_Depth (Spec);
      --  This is used to record the scope depth of the current procedure, so
      --  that we can identify global references.

      Max_Vars : constant := 4;
      --  Do not test more than four global variables

      Count_Vars : Natural := 0;
      --  Count variables found so far

      Var  : Entity_Id;
      Elm  : Elmt_Id;
      Ent  : Entity_Id;
      Call : Elmt_Id;
      Decl : Node_Id;
      Test : Node_Id;
      Elm1 : Elmt_Id;
      Elm2 : Elmt_Id;
      Last : Node_Id;

      function Process (Nod : Node_Id) return Traverse_Result;
      --  Function to traverse the subprogram body (using Traverse_Func)

      -------------
      -- Process --
      -------------

      function Process (Nod : Node_Id) return Traverse_Result is
      begin
         --  Procedure call

         if Nkind (Nod) = N_Procedure_Call_Statement then

            --  Case of one of the detected recursive calls

            if Is_Entity_Name (Name (Nod))
              and then Has_Recursive_Call (Entity (Name (Nod)))
              and then Entity (Name (Nod)) = Spec
            then
               Append_Elmt (Nod, Call_List);
               return Skip;

            --  Any other procedure call may have side effects

            else
               return Abandon;
            end if;

         --  A call to a pure function can always be ignored

         elsif Nkind (Nod) = N_Function_Call
           and then Is_Entity_Name (Name (Nod))
           and then Is_Pure (Entity (Name (Nod)))
         then
            return Skip;

         --  Case of an identifier reference

         elsif Nkind (Nod) = N_Identifier then
            Ent := Entity (Nod);

            --  If no entity, then ignore the reference

            --  Not clear why this can happen. To investigate, remove this
            --  test and look at the crash that occurs here in 3401-004 ???

            if No (Ent) then
               return Skip;

            --  Ignore entities with no Scope, again not clear how this
            --  can happen, to investigate, look at 4108-008 ???

            elsif No (Scope (Ent)) then
               return Skip;

            --  Ignore the reference if not to a more global object

            elsif Scope_Depth (Scope (Ent)) >= Scop then
               return Skip;

            --  References to types, exceptions and constants are always OK

            elsif Is_Type (Ent)
              or else Ekind (Ent) = E_Exception
              or else Ekind (Ent) = E_Constant
            then
               return Skip;

            --  If other than a non-volatile scalar variable, we have some
            --  kind of global reference (e.g. to a function) that we cannot
            --  deal with so we forget the attempt.

            elsif Ekind (Ent) /= E_Variable
              or else not Is_Scalar_Type (Etype (Ent))
              or else Treat_As_Volatile (Ent)
            then
               return Abandon;

            --  Otherwise we have a reference to a global scalar

            else
               --  Loop through global entities already detected

               Elm := First_Elmt (Var_List);
               loop
                  --  If not detected before, record this new global reference

                  if No (Elm) then
                     Count_Vars := Count_Vars + 1;

                     if Count_Vars <= Max_Vars then
                        Append_Elmt (Entity (Nod), Var_List);
                     else
                        return Abandon;
                     end if;

                     exit;

                  --  If recorded before, ignore

                  elsif Node (Elm) = Entity (Nod) then
                     return Skip;

                  --  Otherwise keep looking

                  else
                     Next_Elmt (Elm);
                  end if;
               end loop;

               return Skip;
            end if;

         --  For all other node kinds, recursively visit syntactic children

         else
            return OK;
         end if;
      end Process;

      function Traverse_Body is new Traverse_Func (Process);

   --  Start of processing for Detect_Infinite_Recursion

   begin
      --  Do not attempt detection in No_Implicit_Conditional mode, since we
      --  won't be able to generate the code to handle the recursion in any
      --  case.

      if Restriction_Active (No_Implicit_Conditionals) then
         return;
      end if;

      --  Otherwise do traversal and quit if we get abandon signal

      if Traverse_Body (N) = Abandon then
         return;

      --  We must have a call, since Has_Recursive_Call was set. If not just
      --  ignore (this is only an error check, so if we have a funny situation,
      --  due to bugs or errors, we do not want to bomb!)

      elsif Is_Empty_Elmt_List (Call_List) then
         return;
      end if;

      --  Here is the case where we detect recursion at compile time

      --  Push our current scope for analyzing the declarations and code that
      --  we will insert for the checking.

      Push_Scope (Spec);

      --  This loop builds temporary variables for each of the referenced
      --  globals, so that at the end of the loop the list Shad_List contains
      --  these temporaries in one-to-one correspondence with the elements in
      --  Var_List.

      Last := Empty;
      Elm := First_Elmt (Var_List);
      while Present (Elm) loop
         Var := Node (Elm);
         Ent := Make_Temporary (Loc, 'S');
         Append_Elmt (Ent, Shad_List);

         --  Insert a declaration for this temporary at the start of the
         --  declarations for the procedure. The temporaries are declared as
         --  constant objects initialized to the current values of the
         --  corresponding temporaries.

         Decl :=
           Make_Object_Declaration (Loc,
             Defining_Identifier => Ent,
             Object_Definition   => New_Occurrence_Of (Etype (Var), Loc),
             Constant_Present    => True,
             Expression          => New_Occurrence_Of (Var, Loc));

         if No (Last) then
            Prepend (Decl, Declarations (N));
         else
            Insert_After (Last, Decl);
         end if;

         Last := Decl;
         Analyze (Decl);
         Next_Elmt (Elm);
      end loop;

      --  Loop through calls

      Call := First_Elmt (Call_List);
      while Present (Call) loop

         --  Build a predicate expression of the form

         --    True
         --      and then global1 = temp1
         --      and then global2 = temp2
         --      ...

         --  This predicate determines if any of the global values
         --  referenced by the procedure have changed since the
         --  current call, if not an infinite recursion is assured.

         Test := New_Occurrence_Of (Standard_True, Loc);

         Elm1 := First_Elmt (Var_List);
         Elm2 := First_Elmt (Shad_List);
         while Present (Elm1) loop
            Test :=
              Make_And_Then (Loc,
                Left_Opnd  => Test,
                Right_Opnd =>
                  Make_Op_Eq (Loc,
                    Left_Opnd  => New_Occurrence_Of (Node (Elm1), Loc),
                    Right_Opnd => New_Occurrence_Of (Node (Elm2), Loc)));

            Next_Elmt (Elm1);
            Next_Elmt (Elm2);
         end loop;

         --  Now we replace the call with the sequence

         --    if no-changes (see above) then
         --       raise Storage_Error;
         --    else
         --       original-call
         --    end if;

         Rewrite (Node (Call),
           Make_If_Statement (Loc,
             Condition       => Test,
             Then_Statements => New_List (
               Make_Raise_Storage_Error (Loc,
                 Reason => SE_Infinite_Recursion)),

             Else_Statements => New_List (
               Relocate_Node (Node (Call)))));

         Analyze (Node (Call));

         Next_Elmt (Call);
      end loop;

      --  Remove temporary scope stack entry used for analysis

      Pop_Scope;
   end Detect_Infinite_Recursion;

   --------------------
   -- Expand_Actuals --
   --------------------

   procedure Expand_Actuals (N : Node_Id; Subp : Entity_Id) is
      Loc       : constant Source_Ptr := Sloc (N);
      Actual    : Node_Id;
      Formal    : Entity_Id;
      N_Node    : Node_Id;
      Post_Call : List_Id;
      E_Formal  : Entity_Id;

      procedure Add_Call_By_Copy_Code;
      --  For cases where the parameter must be passed by copy, this routine
      --  generates a temporary variable into which the actual is copied and
      --  then passes this as the parameter. For an OUT or IN OUT parameter,
      --  an assignment is also generated to copy the result back. The call
      --  also takes care of any constraint checks required for the type
      --  conversion case (on both the way in and the way out).

      procedure Add_Simple_Call_By_Copy_Code;
      --  This is similar to the above, but is used in cases where we know
      --  that all that is needed is to simply create a temporary and copy
      --  the value in and out of the temporary.

      procedure Check_Fortran_Logical;
      --  A value of type Logical that is passed through a formal parameter
      --  must be normalized because .TRUE. usually does not have the same
      --  representation as True. We assume that .FALSE. = False = 0.
      --  What about functions that return a logical type ???

      function Is_Legal_Copy return Boolean;
      --  Check that an actual can be copied before generating the temporary
      --  to be used in the call. If the actual is of a by_reference type then
      --  the program is illegal (this can only happen in the presence of
      --  rep. clauses that force an incorrect alignment). If the formal is
      --  a by_reference parameter imposed by a DEC pragma, emit a warning to
      --  the effect that this might lead to unaligned arguments.

      function Make_Var (Actual : Node_Id) return Entity_Id;
      --  Returns an entity that refers to the given actual parameter,
      --  Actual (not including any type conversion). If Actual is an
      --  entity name, then this entity is returned unchanged, otherwise
      --  a renaming is created to provide an entity for the actual.

      procedure Reset_Packed_Prefix;
      --  The expansion of a packed array component reference is delayed in
      --  the context of a call. Now we need to complete the expansion, so we
      --  unmark the analyzed bits in all prefixes.

      ---------------------------
      -- Add_Call_By_Copy_Code --
      ---------------------------

      procedure Add_Call_By_Copy_Code is
         Expr  : Node_Id;
         Init  : Node_Id;
         Temp  : Entity_Id;
         Indic : Node_Id;
         Var   : Entity_Id;
         F_Typ : constant Entity_Id := Etype (Formal);
         V_Typ : Entity_Id;
         Crep  : Boolean;

      begin
         if not Is_Legal_Copy then
            return;
         end if;

         Temp := Make_Temporary (Loc, 'T', Actual);

         --  Use formal type for temp, unless formal type is an unconstrained
         --  array, in which case we don't have to worry about bounds checks,
         --  and we use the actual type, since that has appropriate bounds.

         if Is_Array_Type (F_Typ) and then not Is_Constrained (F_Typ) then
            Indic := New_Occurrence_Of (Etype (Actual), Loc);
         else
            Indic := New_Occurrence_Of (Etype (Formal), Loc);
         end if;

         if Nkind (Actual) = N_Type_Conversion then
            V_Typ := Etype (Expression (Actual));

            --  If the formal is an (in-)out parameter, capture the name
            --  of the variable in order to build the post-call assignment.

            Var := Make_Var (Expression (Actual));

            Crep := not Same_Representation
                          (F_Typ, Etype (Expression (Actual)));

         else
            V_Typ := Etype (Actual);
            Var   := Make_Var (Actual);
            Crep  := False;
         end if;

         --  Setup initialization for case of in out parameter, or an out
         --  parameter where the formal is an unconstrained array (in the
         --  latter case, we have to pass in an object with bounds).

         --  If this is an out parameter, the initial copy is wasteful, so as
         --  an optimization for the one-dimensional case we extract the
         --  bounds of the actual and build an uninitialized temporary of the
         --  right size.

         if Ekind (Formal) = E_In_Out_Parameter
           or else (Is_Array_Type (F_Typ) and then not Is_Constrained (F_Typ))
         then
            if Nkind (Actual) = N_Type_Conversion then
               if Conversion_OK (Actual) then
                  Init := OK_Convert_To (F_Typ, New_Occurrence_Of (Var, Loc));
               else
                  Init := Convert_To (F_Typ, New_Occurrence_Of (Var, Loc));
               end if;

            elsif Ekind (Formal) = E_Out_Parameter
              and then Is_Array_Type (F_Typ)
              and then Number_Dimensions (F_Typ) = 1
              and then not Has_Non_Null_Base_Init_Proc (F_Typ)
            then
               --  Actual is a one-dimensional array or slice, and the type
               --  requires no initialization. Create a temporary of the
               --  right size, but do not copy actual into it (optimization).

               Init := Empty;
               Indic :=
                 Make_Subtype_Indication (Loc,
                   Subtype_Mark =>
                     New_Occurrence_Of (F_Typ, Loc),
                   Constraint   =>
                     Make_Index_Or_Discriminant_Constraint (Loc,
                       Constraints => New_List (
                         Make_Range (Loc,
                           Low_Bound  =>
                             Make_Attribute_Reference (Loc,
                               Prefix => New_Occurrence_Of (Var, Loc),
                               Attribute_Name => Name_First),
                           High_Bound =>
                             Make_Attribute_Reference (Loc,
                               Prefix => New_Occurrence_Of (Var, Loc),
                               Attribute_Name => Name_Last)))));

            else
               Init := New_Occurrence_Of (Var, Loc);
            end if;

         --  An initialization is created for packed conversions as
         --  actuals for out parameters to enable Make_Object_Declaration
         --  to determine the proper subtype for N_Node. Note that this
         --  is wasteful because the extra copying on the call side is
         --  not required for such out parameters. ???

         elsif Ekind (Formal) = E_Out_Parameter
           and then Nkind (Actual) = N_Type_Conversion
           and then (Is_Bit_Packed_Array (F_Typ)
                       or else
                     Is_Bit_Packed_Array (Etype (Expression (Actual))))
         then
            if Conversion_OK (Actual) then
               Init := OK_Convert_To (F_Typ, New_Occurrence_Of (Var, Loc));
            else
               Init := Convert_To (F_Typ, New_Occurrence_Of (Var, Loc));
            end if;

         elsif Ekind (Formal) = E_In_Parameter then

            --  Handle the case in which the actual is a type conversion

            if Nkind (Actual) = N_Type_Conversion then
               if Conversion_OK (Actual) then
                  Init := OK_Convert_To (F_Typ, New_Occurrence_Of (Var, Loc));
               else
                  Init := Convert_To (F_Typ, New_Occurrence_Of (Var, Loc));
               end if;
            else
               Init := New_Occurrence_Of (Var, Loc);
            end if;

         else
            Init := Empty;
         end if;

         N_Node :=
           Make_Object_Declaration (Loc,
             Defining_Identifier => Temp,
             Object_Definition   => Indic,
             Expression          => Init);
         Set_Assignment_OK (N_Node);
         Insert_Action (N, N_Node);

         --  Now, normally the deal here is that we use the defining
         --  identifier created by that object declaration. There is
         --  one exception to this. In the change of representation case
         --  the above declaration will end up looking like:

         --    temp : type := identifier;

         --  And in this case we might as well use the identifier directly
         --  and eliminate the temporary. Note that the analysis of the
         --  declaration was not a waste of time in that case, since it is
         --  what generated the necessary change of representation code. If
         --  the change of representation introduced additional code, as in
         --  a fixed-integer conversion, the expression is not an identifier
         --  and must be kept.

         if Crep
           and then Present (Expression (N_Node))
           and then Is_Entity_Name (Expression (N_Node))
         then
            Temp := Entity (Expression (N_Node));
            Rewrite (N_Node, Make_Null_Statement (Loc));
         end if;

         --  For IN parameter, all we do is to replace the actual

         if Ekind (Formal) = E_In_Parameter then
            Rewrite (Actual, New_Reference_To (Temp, Loc));
            Analyze (Actual);

         --  Processing for OUT or IN OUT parameter

         else
            --  Kill current value indications for the temporary variable we
            --  created, since we just passed it as an OUT parameter.

            Kill_Current_Values (Temp);
            Set_Is_Known_Valid (Temp, False);

            --  If type conversion, use reverse conversion on exit

            if Nkind (Actual) = N_Type_Conversion then
               if Conversion_OK (Actual) then
                  Expr := OK_Convert_To (V_Typ, New_Occurrence_Of (Temp, Loc));
               else
                  Expr := Convert_To (V_Typ, New_Occurrence_Of (Temp, Loc));
               end if;
            else
               Expr := New_Occurrence_Of (Temp, Loc);
            end if;

            Rewrite (Actual, New_Reference_To (Temp, Loc));
            Analyze (Actual);

            --  If the actual is a conversion of a packed reference, it may
            --  already have been expanded by Remove_Side_Effects, and the
            --  resulting variable is a temporary which does not designate
            --  the proper out-parameter, which may not be addressable. In
            --  that case, generate an assignment to the original expression
            --  (before expansion of the packed reference) so that the proper
            --  expansion of assignment to a packed component can take place.

            declare
               Obj : Node_Id;
               Lhs : Node_Id;

            begin
               if Is_Renaming_Of_Object (Var)
                 and then Nkind (Renamed_Object (Var)) = N_Selected_Component
                 and then Is_Entity_Name (Prefix (Renamed_Object (Var)))
                 and then Nkind (Original_Node (Prefix (Renamed_Object (Var))))
                   = N_Indexed_Component
                 and then
                   Has_Non_Standard_Rep (Etype (Prefix (Renamed_Object (Var))))
               then
                  Obj := Renamed_Object (Var);
                  Lhs :=
                    Make_Selected_Component (Loc,
                      Prefix        =>
                        New_Copy_Tree (Original_Node (Prefix (Obj))),
                      Selector_Name => New_Copy (Selector_Name (Obj)));
                  Reset_Analyzed_Flags (Lhs);

               else
                  Lhs :=  New_Occurrence_Of (Var, Loc);
               end if;

               Set_Assignment_OK (Lhs);

               Append_To (Post_Call,
                 Make_Assignment_Statement (Loc,
                   Name       => Lhs,
                   Expression => Expr));
            end;
         end if;
      end Add_Call_By_Copy_Code;

      ----------------------------------
      -- Add_Simple_Call_By_Copy_Code --
      ----------------------------------

      procedure Add_Simple_Call_By_Copy_Code is
         Temp   : Entity_Id;
         Decl   : Node_Id;
         Incod  : Node_Id;
         Outcod : Node_Id;
         Lhs    : Node_Id;
         Rhs    : Node_Id;
         Indic  : Node_Id;
         F_Typ  : constant Entity_Id := Etype (Formal);

      begin
         if not Is_Legal_Copy then
            return;
         end if;

         --  Use formal type for temp, unless formal type is an unconstrained
         --  array, in which case we don't have to worry about bounds checks,
         --  and we use the actual type, since that has appropriate bounds.

         if Is_Array_Type (F_Typ) and then not Is_Constrained (F_Typ) then
            Indic := New_Occurrence_Of (Etype (Actual), Loc);
         else
            Indic := New_Occurrence_Of (Etype (Formal), Loc);
         end if;

         --  Prepare to generate code

         Reset_Packed_Prefix;

         Temp := Make_Temporary (Loc, 'T', Actual);
         Incod  := Relocate_Node (Actual);
         Outcod := New_Copy_Tree (Incod);

         --  Generate declaration of temporary variable, initializing it
         --  with the input parameter unless we have an OUT formal or
         --  this is an initialization call.

         --  If the formal is an out parameter with discriminants, the
         --  discriminants must be captured even if the rest of the object
         --  is in principle uninitialized, because the discriminants may
         --  be read by the called subprogram.

         if Ekind (Formal) = E_Out_Parameter then
            Incod := Empty;

            if Has_Discriminants (Etype (Formal)) then
               Indic := New_Occurrence_Of (Etype (Actual), Loc);
            end if;

         elsif Inside_Init_Proc then

            --  Could use a comment here to match comment below ???

            if Nkind (Actual) /= N_Selected_Component
              or else
                not Has_Discriminant_Dependent_Constraint
                  (Entity (Selector_Name (Actual)))
            then
               Incod := Empty;

            --  Otherwise, keep the component in order to generate the proper
            --  actual subtype, that depends on enclosing discriminants.

            else
               null;
            end if;
         end if;

         Decl :=
           Make_Object_Declaration (Loc,
             Defining_Identifier => Temp,
             Object_Definition   => Indic,
             Expression          => Incod);

         if Inside_Init_Proc
           and then No (Incod)
         then
            --  If the call is to initialize a component of a composite type,
            --  and the component does not depend on discriminants, use the
            --  actual type of the component. This is required in case the
            --  component is constrained, because in general the formal of the
            --  initialization procedure will be unconstrained. Note that if
            --  the component being initialized is constrained by an enclosing
            --  discriminant, the presence of the initialization in the
            --  declaration will generate an expression for the actual subtype.

            Set_No_Initialization (Decl);
            Set_Object_Definition (Decl,
              New_Occurrence_Of (Etype (Actual), Loc));
         end if;

         Insert_Action (N, Decl);

         --  The actual is simply a reference to the temporary

         Rewrite (Actual, New_Occurrence_Of (Temp, Loc));

         --  Generate copy out if OUT or IN OUT parameter

         if Ekind (Formal) /= E_In_Parameter then
            Lhs := Outcod;
            Rhs := New_Occurrence_Of (Temp, Loc);

            --  Deal with conversion

            if Nkind (Lhs) = N_Type_Conversion then
               Lhs := Expression (Lhs);
               Rhs := Convert_To (Etype (Actual), Rhs);
            end if;

            Append_To (Post_Call,
              Make_Assignment_Statement (Loc,
                Name       => Lhs,
                Expression => Rhs));
            Set_Assignment_OK (Name (Last (Post_Call)));
         end if;
      end Add_Simple_Call_By_Copy_Code;

      ---------------------------
      -- Check_Fortran_Logical --
      ---------------------------

      procedure Check_Fortran_Logical is
         Logical : constant Entity_Id := Etype (Formal);
         Var     : Entity_Id;

      --  Note: this is very incomplete, e.g. it does not handle arrays
      --  of logical values. This is really not the right approach at all???)

      begin
         if Convention (Subp) = Convention_Fortran
           and then Root_Type (Etype (Formal)) = Standard_Boolean
           and then Ekind (Formal) /= E_In_Parameter
         then
            Var := Make_Var (Actual);
            Append_To (Post_Call,
              Make_Assignment_Statement (Loc,
                Name => New_Occurrence_Of (Var, Loc),
                Expression =>
                  Unchecked_Convert_To (
                    Logical,
                    Make_Op_Ne (Loc,
                      Left_Opnd  => New_Occurrence_Of (Var, Loc),
                      Right_Opnd =>
                        Unchecked_Convert_To (
                          Logical,
                          New_Occurrence_Of (Standard_False, Loc))))));
         end if;
      end Check_Fortran_Logical;

      -------------------
      -- Is_Legal_Copy --
      -------------------

      function Is_Legal_Copy return Boolean is
      begin
         --  An attempt to copy a value of such a type can only occur if
         --  representation clauses give the actual a misaligned address.

         if Is_By_Reference_Type (Etype (Formal)) then
            Error_Msg_N
              ("misaligned actual cannot be passed by reference", Actual);
            return False;

         --  For users of Starlet, we assume that the specification of by-
         --  reference mechanism is mandatory. This may lead to unaligned
         --  objects but at least for DEC legacy code it is known to work.
         --  The warning will alert users of this code that a problem may
         --  be lurking.

         elsif Mechanism (Formal) = By_Reference
           and then Is_Valued_Procedure (Scope (Formal))
         then
            Error_Msg_N
              ("by_reference actual may be misaligned?", Actual);
            return False;

         else
            return True;
         end if;
      end Is_Legal_Copy;

      --------------
      -- Make_Var --
      --------------

      function Make_Var (Actual : Node_Id) return Entity_Id is
         Var : Entity_Id;

      begin
         if Is_Entity_Name (Actual) then
            return Entity (Actual);

         else
            Var := Make_Temporary (Loc, 'T', Actual);

            N_Node :=
              Make_Object_Renaming_Declaration (Loc,
                Defining_Identifier => Var,
                Subtype_Mark        =>
                  New_Occurrence_Of (Etype (Actual), Loc),
                Name                => Relocate_Node (Actual));

            Insert_Action (N, N_Node);
            return Var;
         end if;
      end Make_Var;

      -------------------------
      -- Reset_Packed_Prefix --
      -------------------------

      procedure Reset_Packed_Prefix is
         Pfx : Node_Id := Actual;
      begin
         loop
            Set_Analyzed (Pfx, False);
            exit when
              not Nkind_In (Pfx, N_Selected_Component, N_Indexed_Component);
            Pfx := Prefix (Pfx);
         end loop;
      end Reset_Packed_Prefix;

   --  Start of processing for Expand_Actuals

   begin
      Post_Call := New_List;

      Formal := First_Formal (Subp);
      Actual := First_Actual (N);
      while Present (Formal) loop
         E_Formal := Etype (Formal);

         if Is_Scalar_Type (E_Formal)
           or else Nkind (Actual) = N_Slice
         then
            Check_Fortran_Logical;

         --  RM 6.4.1 (11)

         elsif Ekind (Formal) /= E_Out_Parameter then

            --  The unusual case of the current instance of a protected type
            --  requires special handling. This can only occur in the context
            --  of a call within the body of a protected operation.

            if Is_Entity_Name (Actual)
              and then Ekind (Entity (Actual)) = E_Protected_Type
              and then In_Open_Scopes (Entity (Actual))
            then
               if Scope (Subp) /= Entity (Actual) then
                  Error_Msg_N ("operation outside protected type may not "
                    & "call back its protected operations?", Actual);
               end if;

               Rewrite (Actual,
                 Expand_Protected_Object_Reference (N, Entity (Actual)));
            end if;

            --  Ada 2005 (AI-318-02): If the actual parameter is a call to a
            --  build-in-place function, then a temporary return object needs
            --  to be created and access to it must be passed to the function.
            --  Currently we limit such functions to those with inherently
            --  limited result subtypes, but eventually we plan to expand the
            --  functions that are treated as build-in-place to include other
            --  composite result types.

            if Ada_Version >= Ada_2005
              and then Is_Build_In_Place_Function_Call (Actual)
            then
               Make_Build_In_Place_Call_In_Anonymous_Context (Actual);
            end if;

            Apply_Constraint_Check (Actual, E_Formal);

         --  Out parameter case. No constraint checks on access type
         --  RM 6.4.1 (13)

         elsif Is_Access_Type (E_Formal) then
            null;

         --  RM 6.4.1 (14)

         elsif Has_Discriminants (Base_Type (E_Formal))
           or else Has_Non_Null_Base_Init_Proc (E_Formal)
         then
            Apply_Constraint_Check (Actual, E_Formal);

         --  RM 6.4.1 (15)

         else
            Apply_Constraint_Check (Actual, Base_Type (E_Formal));
         end if;

         --  Processing for IN-OUT and OUT parameters

         if Ekind (Formal) /= E_In_Parameter then

            --  For type conversions of arrays, apply length/range checks

            if Is_Array_Type (E_Formal)
              and then Nkind (Actual) = N_Type_Conversion
            then
               if Is_Constrained (E_Formal) then
                  Apply_Length_Check (Expression (Actual), E_Formal);
               else
                  Apply_Range_Check (Expression (Actual), E_Formal);
               end if;
            end if;

            --  If argument is a type conversion for a type that is passed
            --  by copy, then we must pass the parameter by copy.

            if Nkind (Actual) = N_Type_Conversion
              and then
                (Is_Numeric_Type (E_Formal)
                  or else Is_Access_Type (E_Formal)
                  or else Is_Enumeration_Type (E_Formal)
                  or else Is_Bit_Packed_Array (Etype (Formal))
                  or else Is_Bit_Packed_Array (Etype (Expression (Actual)))

                  --  Also pass by copy if change of representation

                  or else not Same_Representation
                               (Etype (Formal),
                                Etype (Expression (Actual))))
            then
               Add_Call_By_Copy_Code;

            --  References to components of bit packed arrays are expanded
            --  at this point, rather than at the point of analysis of the
            --  actuals, to handle the expansion of the assignment to
            --  [in] out parameters.

            elsif Is_Ref_To_Bit_Packed_Array (Actual) then
               Add_Simple_Call_By_Copy_Code;

            --  If a non-scalar actual is possibly bit-aligned, we need a copy
            --  because the back-end cannot cope with such objects. In other
            --  cases where alignment forces a copy, the back-end generates
            --  it properly. It should not be generated unconditionally in the
            --  front-end because it does not know precisely the alignment
            --  requirements of the target, and makes too conservative an
            --  estimate, leading to superfluous copies or spurious errors
            --  on by-reference parameters.

            elsif Nkind (Actual) = N_Selected_Component
              and then
                Component_May_Be_Bit_Aligned (Entity (Selector_Name (Actual)))
              and then not Represented_As_Scalar (Etype (Formal))
            then
               Add_Simple_Call_By_Copy_Code;

            --  References to slices of bit packed arrays are expanded

            elsif Is_Ref_To_Bit_Packed_Slice (Actual) then
               Add_Call_By_Copy_Code;

            --  References to possibly unaligned slices of arrays are expanded

            elsif Is_Possibly_Unaligned_Slice (Actual) then
               Add_Call_By_Copy_Code;

            --  Deal with access types where the actual subtype and the
            --  formal subtype are not the same, requiring a check.

            --  It is necessary to exclude tagged types because of "downward
            --  conversion" errors.

            elsif Is_Access_Type (E_Formal)
              and then not Same_Type (E_Formal, Etype (Actual))
              and then not Is_Tagged_Type (Designated_Type (E_Formal))
            then
               Add_Call_By_Copy_Code;

            --  If the actual is not a scalar and is marked for volatile
            --  treatment, whereas the formal is not volatile, then pass
            --  by copy unless it is a by-reference type.

            --  Note: we use Is_Volatile here rather than Treat_As_Volatile,
            --  because this is the enforcement of a language rule that applies
            --  only to "real" volatile variables, not e.g. to the address
            --  clause overlay case.

            elsif Is_Entity_Name (Actual)
              and then Is_Volatile (Entity (Actual))
              and then not Is_By_Reference_Type (Etype (Actual))
              and then not Is_Scalar_Type (Etype (Entity (Actual)))
              and then not Is_Volatile (E_Formal)
            then
               Add_Call_By_Copy_Code;

            elsif Nkind (Actual) = N_Indexed_Component
              and then Is_Entity_Name (Prefix (Actual))
              and then Has_Volatile_Components (Entity (Prefix (Actual)))
            then
               Add_Call_By_Copy_Code;

            --  Add call-by-copy code for the case of scalar out parameters
            --  when it is not known at compile time that the subtype of the
            --  formal is a subrange of the subtype of the actual (or vice
            --  versa for in out parameters), in order to get range checks
            --  on such actuals. (Maybe this case should be handled earlier
            --  in the if statement???)

            elsif Is_Scalar_Type (E_Formal)
              and then
                (not In_Subrange_Of (E_Formal, Etype (Actual))
                  or else
                    (Ekind (Formal) = E_In_Out_Parameter
                      and then not In_Subrange_Of (Etype (Actual), E_Formal)))
            then
               --  Perhaps the setting back to False should be done within
               --  Add_Call_By_Copy_Code, since it could get set on other
               --  cases occurring above???

               if Do_Range_Check (Actual) then
                  Set_Do_Range_Check (Actual, False);
               end if;

               Add_Call_By_Copy_Code;
            end if;

         --  Processing for IN parameters

         else
            --  For IN parameters is in the packed array case, we expand an
            --  indexed component (the circuit in Exp_Ch4 deliberately left
            --  indexed components appearing as actuals untouched, so that
            --  the special processing above for the OUT and IN OUT cases
            --  could be performed. We could make the test in Exp_Ch4 more
            --  complex and have it detect the parameter mode, but it is
            --  easier simply to handle all cases here.)

            if Nkind (Actual) = N_Indexed_Component
              and then Is_Packed (Etype (Prefix (Actual)))
            then
               Reset_Packed_Prefix;
               Expand_Packed_Element_Reference (Actual);

            --  If we have a reference to a bit packed array, we copy it, since
            --  the actual must be byte aligned.

            --  Is this really necessary in all cases???

            elsif Is_Ref_To_Bit_Packed_Array (Actual) then
               Add_Simple_Call_By_Copy_Code;

            --  If a non-scalar actual is possibly unaligned, we need a copy

            elsif Is_Possibly_Unaligned_Object (Actual)
              and then not Represented_As_Scalar (Etype (Formal))
            then
               Add_Simple_Call_By_Copy_Code;

            --  Similarly, we have to expand slices of packed arrays here
            --  because the result must be byte aligned.

            elsif Is_Ref_To_Bit_Packed_Slice (Actual) then
               Add_Call_By_Copy_Code;

            --  Only processing remaining is to pass by copy if this is a
            --  reference to a possibly unaligned slice, since the caller
            --  expects an appropriately aligned argument.

            elsif Is_Possibly_Unaligned_Slice (Actual) then
               Add_Call_By_Copy_Code;

            --  An unusual case: a current instance of an enclosing task can be
            --  an actual, and must be replaced by a reference to self.

            elsif Is_Entity_Name (Actual)
              and then Is_Task_Type (Entity (Actual))
            then
               if In_Open_Scopes (Entity (Actual)) then
                  Rewrite (Actual,
                    (Make_Function_Call (Loc,
                     Name => New_Reference_To (RTE (RE_Self), Loc))));
                  Analyze (Actual);

               --  A task type cannot otherwise appear as an actual

               else
                  raise Program_Error;
               end if;
            end if;
         end if;

         Next_Formal (Formal);
         Next_Actual (Actual);
      end loop;

      --  Find right place to put post call stuff if it is present

      if not Is_Empty_List (Post_Call) then

         --  If call is not a list member, it must be the triggering statement
         --  of a triggering alternative or an entry call alternative, and we
         --  can add the post call stuff to the corresponding statement list.

         if not Is_List_Member (N) then
            declare
               P : constant Node_Id := Parent (N);

            begin
               pragma Assert (Nkind_In (P, N_Triggering_Alternative,
                                           N_Entry_Call_Alternative));

               if Is_Non_Empty_List (Statements (P)) then
                  Insert_List_Before_And_Analyze
                    (First (Statements (P)), Post_Call);
               else
                  Set_Statements (P, Post_Call);
               end if;
            end;

         --  Otherwise, normal case where N is in a statement sequence,
         --  just put the post-call stuff after the call statement.

         else
            Insert_Actions_After (N, Post_Call);
         end if;
      end if;

      --  The call node itself is re-analyzed in Expand_Call

   end Expand_Actuals;

   -----------------
   -- Expand_Call --
   -----------------

   --  This procedure handles expansion of function calls and procedure call
   --  statements (i.e. it serves as the body for Expand_N_Function_Call and
   --  Expand_N_Procedure_Call_Statement). Processing for calls includes:

   --    Replace call to Raise_Exception by Raise_Exception_Always if possible
   --    Provide values of actuals for all formals in Extra_Formals list
   --    Replace "call" to enumeration literal function by literal itself
   --    Rewrite call to predefined operator as operator
   --    Replace actuals to in-out parameters that are numeric conversions,
   --     with explicit assignment to temporaries before and after the call.
   --    Remove optional actuals if First_Optional_Parameter specified.

   --   Note that the list of actuals has been filled with default expressions
   --   during semantic analysis of the call. Only the extra actuals required
   --   for the 'Constrained attribute and for accessibility checks are added
   --   at this point.

   procedure Expand_Call (N : Node_Id) is
      Loc           : constant Source_Ptr := Sloc (N);
      Call_Node     : Node_Id := N;
      Extra_Actuals : List_Id := No_List;
      Prev          : Node_Id := Empty;

      procedure Add_Actual_Parameter (Insert_Param : Node_Id);
      --  Adds one entry to the end of the actual parameter list. Used for
      --  default parameters and for extra actuals (for Extra_Formals). The
      --  argument is an N_Parameter_Association node.

      procedure Add_Extra_Actual (Expr : Node_Id; EF : Entity_Id);
      --  Adds an extra actual to the list of extra actuals. Expr is the
      --  expression for the value of the actual, EF is the entity for the
      --  extra formal.

      function Inherited_From_Formal (S : Entity_Id) return Entity_Id;
      --  Within an instance, a type derived from a non-tagged formal derived
      --  type inherits from the original parent, not from the actual. The
      --  current derivation mechanism has the derived type inherit from the
      --  actual, which is only correct outside of the instance. If the
      --  subprogram is inherited, we test for this particular case through a
      --  convoluted tree traversal before setting the proper subprogram to be
      --  called.

      function New_Value (From : Node_Id) return Node_Id;
      --  From is the original Expression. New_Value is equivalent to a call
      --  to Duplicate_Subexpr with an explicit dereference when From is an
      --  access parameter.

      --------------------------
      -- Add_Actual_Parameter --
      --------------------------

      procedure Add_Actual_Parameter (Insert_Param : Node_Id) is
         Actual_Expr : constant Node_Id :=
                         Explicit_Actual_Parameter (Insert_Param);

      begin
         --  Case of insertion is first named actual

         if No (Prev) or else
            Nkind (Parent (Prev)) /= N_Parameter_Association
         then
            Set_Next_Named_Actual
              (Insert_Param, First_Named_Actual (Call_Node));
            Set_First_Named_Actual (Call_Node, Actual_Expr);

            if No (Prev) then
               if No (Parameter_Associations (Call_Node)) then
                  Set_Parameter_Associations (Call_Node, New_List);
                  Append (Insert_Param, Parameter_Associations (Call_Node));
               end if;
            else
               Insert_After (Prev, Insert_Param);
            end if;

         --  Case of insertion is not first named actual

         else
            Set_Next_Named_Actual
              (Insert_Param, Next_Named_Actual (Parent (Prev)));
            Set_Next_Named_Actual (Parent (Prev), Actual_Expr);
            Append (Insert_Param, Parameter_Associations (Call_Node));
         end if;

         Prev := Actual_Expr;
      end Add_Actual_Parameter;

      ----------------------
      -- Add_Extra_Actual --
      ----------------------

      procedure Add_Extra_Actual (Expr : Node_Id; EF : Entity_Id) is
         Loc : constant Source_Ptr := Sloc (Expr);

      begin
         if Extra_Actuals = No_List then
            Extra_Actuals := New_List;
            Set_Parent (Extra_Actuals, Call_Node);
         end if;

         Append_To (Extra_Actuals,
           Make_Parameter_Association (Loc,
             Selector_Name             => Make_Identifier (Loc, Chars (EF)),
             Explicit_Actual_Parameter => Expr));

         Analyze_And_Resolve (Expr, Etype (EF));

         if Nkind (Call_Node) = N_Function_Call then
            Set_Is_Accessibility_Actual (Parent (Expr));
         end if;
      end Add_Extra_Actual;

      ---------------------------
      -- Inherited_From_Formal --
      ---------------------------

      function Inherited_From_Formal (S : Entity_Id) return Entity_Id is
         Par      : Entity_Id;
         Gen_Par  : Entity_Id;
         Gen_Prim : Elist_Id;
         Elmt     : Elmt_Id;
         Indic    : Node_Id;

      begin
         --  If the operation is inherited, it is attached to the corresponding
         --  type derivation. If the parent in the derivation is a generic
         --  actual, it is a subtype of the actual, and we have to recover the
         --  original derived type declaration to find the proper parent.

         if Nkind (Parent (S)) /= N_Full_Type_Declaration
           or else not Is_Derived_Type (Defining_Identifier (Parent (S)))
           or else Nkind (Type_Definition (Original_Node (Parent (S)))) /=
                                                   N_Derived_Type_Definition
           or else not In_Instance
         then
            return Empty;

         else
            Indic :=
              Subtype_Indication
                (Type_Definition (Original_Node (Parent (S))));

            if Nkind (Indic) = N_Subtype_Indication then
               Par := Entity (Subtype_Mark (Indic));
            else
               Par := Entity (Indic);
            end if;
         end if;

         if not Is_Generic_Actual_Type (Par)
           or else Is_Tagged_Type (Par)
           or else Nkind (Parent (Par)) /= N_Subtype_Declaration
           or else not In_Open_Scopes (Scope (Par))
         then
            return Empty;
         else
            Gen_Par := Generic_Parent_Type (Parent (Par));
         end if;

         --  If the actual has no generic parent type, the formal is not
         --  a formal derived type, so nothing to inherit.

         if No (Gen_Par) then
            return Empty;
         end if;

         --  If the generic parent type is still the generic type, this is a
         --  private formal, not a derived formal, and there are no operations
         --  inherited from the formal.

         if Nkind (Parent (Gen_Par)) = N_Formal_Type_Declaration then
            return Empty;
         end if;

         Gen_Prim := Collect_Primitive_Operations (Gen_Par);

         Elmt := First_Elmt (Gen_Prim);
         while Present (Elmt) loop
            if Chars (Node (Elmt)) = Chars (S) then
               declare
                  F1 : Entity_Id;
                  F2 : Entity_Id;

               begin
                  F1 := First_Formal (S);
                  F2 := First_Formal (Node (Elmt));
                  while Present (F1)
                    and then Present (F2)
                  loop
                     if Etype (F1) = Etype (F2)
                       or else Etype (F2) = Gen_Par
                     then
                        Next_Formal (F1);
                        Next_Formal (F2);
                     else
                        Next_Elmt (Elmt);
                        exit;   --  not the right subprogram
                     end if;

                     return Node (Elmt);
                  end loop;
               end;

            else
               Next_Elmt (Elmt);
            end if;
         end loop;

         raise Program_Error;
      end Inherited_From_Formal;

      ---------------
      -- New_Value --
      ---------------

      function New_Value (From : Node_Id) return Node_Id is
         Res : constant Node_Id := Duplicate_Subexpr (From);
      begin
         if Is_Access_Type (Etype (From)) then
            return
              Make_Explicit_Dereference (Sloc (From),
                Prefix => Res);
         else
            return Res;
         end if;
      end New_Value;

      --  Local variables

      Remote        : constant Boolean := Is_Remote_Call (Call_Node);
      Actual        : Node_Id;
      Formal        : Entity_Id;
      Orig_Subp     : Entity_Id := Empty;
      Param_Count   : Natural := 0;
      Parent_Formal : Entity_Id;
      Parent_Subp   : Entity_Id;
      Scop          : Entity_Id;
      Subp          : Entity_Id;

      Prev_Orig : Node_Id;
      --  Original node for an actual, which may have been rewritten. If the
      --  actual is a function call that has been transformed from a selected
      --  component, the original node is unanalyzed. Otherwise, it carries
      --  semantic information used to generate additional actuals.

      CW_Interface_Formals_Present : Boolean := False;

   --  Start of processing for Expand_Call

   begin
      --  Ignore if previous error

      if Nkind (Call_Node) in N_Has_Etype
        and then Etype (Call_Node) = Any_Type
      then
         return;
      end if;

      --  Call using access to subprogram with explicit dereference

      if Nkind (Name (Call_Node)) = N_Explicit_Dereference then
         Subp        := Etype (Name (Call_Node));
         Parent_Subp := Empty;

      --  Case of call to simple entry, where the Name is a selected component
      --  whose prefix is the task, and whose selector name is the entry name

      elsif Nkind (Name (Call_Node)) = N_Selected_Component then
         Subp        := Entity (Selector_Name (Name (Call_Node)));
         Parent_Subp := Empty;

      --  Case of call to member of entry family, where Name is an indexed
      --  component, with the prefix being a selected component giving the
      --  task and entry family name, and the index being the entry index.

      elsif Nkind (Name (Call_Node)) = N_Indexed_Component then
         Subp        := Entity (Selector_Name (Prefix (Name (Call_Node))));
         Parent_Subp := Empty;

      --  Normal case

      else
         Subp        := Entity (Name (Call_Node));
         Parent_Subp := Alias (Subp);

         --  Replace call to Raise_Exception by call to Raise_Exception_Always
         --  if we can tell that the first parameter cannot possibly be null.
         --  This improves efficiency by avoiding a run-time test.

         --  We do not do this if Raise_Exception_Always does not exist, which
         --  can happen in configurable run time profiles which provide only a
         --  Raise_Exception.

         if Is_RTE (Subp, RE_Raise_Exception)
           and then RTE_Available (RE_Raise_Exception_Always)
         then
            declare
               FA : constant Node_Id :=
                      Original_Node (First_Actual (Call_Node));

            begin
               --  The case we catch is where the first argument is obtained
               --  using the Identity attribute (which must always be
               --  non-null).

               if Nkind (FA) = N_Attribute_Reference
                 and then Attribute_Name (FA) = Name_Identity
               then
                  Subp := RTE (RE_Raise_Exception_Always);
                  Set_Name (Call_Node, New_Occurrence_Of (Subp, Loc));
               end if;
            end;
         end if;

         if Ekind (Subp) = E_Entry then
            Parent_Subp := Empty;
         end if;
      end if;

      --  Ada 2005 (AI-345): We have a procedure call as a triggering
      --  alternative in an asynchronous select or as an entry call in
      --  a conditional or timed select. Check whether the procedure call
      --  is a renaming of an entry and rewrite it as an entry call.

      if Ada_Version >= Ada_2005
        and then Nkind (Call_Node) = N_Procedure_Call_Statement
        and then
           ((Nkind (Parent (Call_Node)) = N_Triggering_Alternative
              and then Triggering_Statement (Parent (Call_Node)) = Call_Node)
          or else
            (Nkind (Parent (Call_Node)) = N_Entry_Call_Alternative
              and then Entry_Call_Statement (Parent (Call_Node)) = Call_Node))
      then
         declare
            Ren_Decl : Node_Id;
            Ren_Root : Entity_Id := Subp;

         begin
            --  This may be a chain of renamings, find the root

            if Present (Alias (Ren_Root)) then
               Ren_Root := Alias (Ren_Root);
            end if;

            if Present (Original_Node (Parent (Parent (Ren_Root)))) then
               Ren_Decl := Original_Node (Parent (Parent (Ren_Root)));

               if Nkind (Ren_Decl) = N_Subprogram_Renaming_Declaration then
                  Rewrite (Call_Node,
                    Make_Entry_Call_Statement (Loc,
                      Name =>
                        New_Copy_Tree (Name (Ren_Decl)),
                      Parameter_Associations =>
                        New_Copy_List_Tree
                          (Parameter_Associations (Call_Node))));

                  return;
               end if;
            end if;
         end;
      end if;

      --  First step, compute extra actuals, corresponding to any Extra_Formals
      --  present. Note that we do not access Extra_Formals directly, instead
      --  we simply note the presence of the extra formals as we process the
      --  regular formals collecting corresponding actuals in Extra_Actuals.

      --  We also generate any required range checks for actuals for in formals
      --  as we go through the loop, since this is a convenient place to do it.
      --  (Though it seems that this would be better done in Expand_Actuals???)

      Formal      := First_Formal (Subp);
      Actual      := First_Actual (Call_Node);
      Param_Count := 1;
      while Present (Formal) loop

         --  Generate range check if required

         if Do_Range_Check (Actual)
           and then Ekind (Formal) = E_In_Parameter
         then
            Set_Do_Range_Check (Actual, False);
            Generate_Range_Check
              (Actual, Etype (Formal), CE_Range_Check_Failed);
         end if;

         --  Prepare to examine current entry

         Prev := Actual;
         Prev_Orig := Original_Node (Prev);

         --  Ada 2005 (AI-251): Check if any formal is a class-wide interface
         --  to expand it in a further round.

         CW_Interface_Formals_Present :=
           CW_Interface_Formals_Present
             or else
               (Ekind (Etype (Formal)) = E_Class_Wide_Type
                  and then Is_Interface (Etype (Etype (Formal))))
             or else
               (Ekind (Etype (Formal)) = E_Anonymous_Access_Type
                 and then Is_Interface (Directly_Designated_Type
                                         (Etype (Etype (Formal)))));

         --  Create possible extra actual for constrained case. Usually, the
         --  extra actual is of the form actual'constrained, but since this
         --  attribute is only available for unconstrained records, TRUE is
         --  expanded if the type of the formal happens to be constrained (for
         --  instance when this procedure is inherited from an unconstrained
         --  record to a constrained one) or if the actual has no discriminant
         --  (its type is constrained). An exception to this is the case of a
         --  private type without discriminants. In this case we pass FALSE
         --  because the object has underlying discriminants with defaults.

         if Present (Extra_Constrained (Formal)) then
            if Ekind (Etype (Prev)) in Private_Kind
              and then not Has_Discriminants (Base_Type (Etype (Prev)))
            then
               Add_Extra_Actual
                 (New_Occurrence_Of (Standard_False, Loc),
                  Extra_Constrained (Formal));

            elsif Is_Constrained (Etype (Formal))
              or else not Has_Discriminants (Etype (Prev))
            then
               Add_Extra_Actual
                 (New_Occurrence_Of (Standard_True, Loc),
                  Extra_Constrained (Formal));

            --  Do not produce extra actuals for Unchecked_Union parameters.
            --  Jump directly to the end of the loop.

            elsif Is_Unchecked_Union (Base_Type (Etype (Actual))) then
               goto Skip_Extra_Actual_Generation;

            else
               --  If the actual is a type conversion, then the constrained
               --  test applies to the actual, not the target type.

               declare
                  Act_Prev : Node_Id;

               begin
                  --  Test for unchecked conversions as well, which can occur
                  --  as out parameter actuals on calls to stream procedures.

                  Act_Prev := Prev;
                  while Nkind_In (Act_Prev, N_Type_Conversion,
                                            N_Unchecked_Type_Conversion)
                  loop
                     Act_Prev := Expression (Act_Prev);
                  end loop;

                  --  If the expression is a conversion of a dereference, this
                  --  is internally generated code that manipulates addresses,
                  --  e.g. when building interface tables. No check should
                  --  occur in this case, and the discriminated object is not
                  --  directly a hand.

                  if not Comes_From_Source (Actual)
                    and then Nkind (Actual) = N_Unchecked_Type_Conversion
                    and then Nkind (Act_Prev) = N_Explicit_Dereference
                  then
                     Add_Extra_Actual
                       (New_Occurrence_Of (Standard_False, Loc),
                        Extra_Constrained (Formal));

                  else
                     Add_Extra_Actual
                       (Make_Attribute_Reference (Sloc (Prev),
                        Prefix =>
                          Duplicate_Subexpr_No_Checks
                            (Act_Prev, Name_Req => True),
                        Attribute_Name => Name_Constrained),
                        Extra_Constrained (Formal));
                  end if;
               end;
            end if;
         end if;

         --  Create possible extra actual for accessibility level

         if Present (Extra_Accessibility (Formal)) then

            --  Ada 2005 (AI-252): If the actual was rewritten as an Access
            --  attribute, then the original actual may be an aliased object
            --  occurring as the prefix in a call using "Object.Operation"
            --  notation. In that case we must pass the level of the object,
            --  so Prev_Orig is reset to Prev and the attribute will be
            --  processed by the code for Access attributes further below.

            if Prev_Orig /= Prev
              and then Nkind (Prev) = N_Attribute_Reference
              and then
                Get_Attribute_Id (Attribute_Name (Prev)) = Attribute_Access
              and then Is_Aliased_View (Prev_Orig)
            then
               Prev_Orig := Prev;
            end if;

            --  Ada 2005 (AI-251): Thunks must propagate the extra actuals of
            --  accessibility levels.

            if Ekind (Current_Scope) in Subprogram_Kind
              and then Is_Thunk (Current_Scope)
            then
               declare
                  Parm_Ent : Entity_Id;

               begin
                  if Is_Controlling_Actual (Actual) then

                     --  Find the corresponding actual of the thunk

                     Parm_Ent := First_Entity (Current_Scope);
                     for J in 2 .. Param_Count loop
                        Next_Entity (Parm_Ent);
                     end loop;

                  else pragma Assert (Is_Entity_Name (Actual));
                     Parm_Ent := Entity (Actual);
                  end if;

                  Add_Extra_Actual
                    (New_Occurrence_Of (Extra_Accessibility (Parm_Ent), Loc),
                     Extra_Accessibility (Formal));
               end;

            elsif Is_Entity_Name (Prev_Orig) then

               --  When passing an access parameter, or a renaming of an access
               --  parameter, as the actual to another access parameter we need
               --  to pass along the actual's own access level parameter. This
               --  is done if we are within the scope of the formal access
               --  parameter (if this is an inlined body the extra formal is
               --  irrelevant).

               if (Is_Formal (Entity (Prev_Orig))
                    or else
                      (Present (Renamed_Object (Entity (Prev_Orig)))
                        and then
                          Is_Entity_Name (Renamed_Object (Entity (Prev_Orig)))
                        and then
                          Is_Formal
                            (Entity (Renamed_Object (Entity (Prev_Orig))))))
                 and then Ekind (Etype (Prev_Orig)) = E_Anonymous_Access_Type
                 and then In_Open_Scopes (Scope (Entity (Prev_Orig)))
               then
                  declare
                     Parm_Ent : constant Entity_Id := Param_Entity (Prev_Orig);

                  begin
                     pragma Assert (Present (Parm_Ent));

                     if Present (Extra_Accessibility (Parm_Ent)) then
                        Add_Extra_Actual
                          (New_Occurrence_Of
                             (Extra_Accessibility (Parm_Ent), Loc),
                           Extra_Accessibility (Formal));

                     --  If the actual access parameter does not have an
                     --  associated extra formal providing its scope level,
                     --  then treat the actual as having library-level
                     --  accessibility.

                     else
                        Add_Extra_Actual
                          (Make_Integer_Literal (Loc,
                             Intval => Scope_Depth (Standard_Standard)),
                           Extra_Accessibility (Formal));
                     end if;
                  end;

               --  The actual is a normal access value, so just pass the level
               --  of the actual's access type.

               else
                  Add_Extra_Actual
                    (Make_Integer_Literal (Loc,
                       Intval => Type_Access_Level (Etype (Prev_Orig))),
                     Extra_Accessibility (Formal));
               end if;

            --  If the actual is an access discriminant, then pass the level
            --  of the enclosing object (RM05-3.10.2(12.4/2)).

            elsif Nkind (Prev_Orig) = N_Selected_Component
              and then Ekind (Entity (Selector_Name (Prev_Orig))) =
                                                       E_Discriminant
              and then Ekind (Etype (Entity (Selector_Name (Prev_Orig)))) =
                                                       E_Anonymous_Access_Type
            then
               Add_Extra_Actual
                 (Make_Integer_Literal (Loc,
                    Intval => Object_Access_Level (Prefix (Prev_Orig))),
                  Extra_Accessibility (Formal));

            --  All other cases

            else
               case Nkind (Prev_Orig) is

                  when N_Attribute_Reference =>
                     case Get_Attribute_Id (Attribute_Name (Prev_Orig)) is

                        --  For X'Access, pass on the level of the prefix X

                        when Attribute_Access =>
                           Add_Extra_Actual
                             (Make_Integer_Literal (Loc,
                               Intval =>
                                 Object_Access_Level
                                   (Prefix (Prev_Orig))),
                                    Extra_Accessibility (Formal));

                        --  Treat the unchecked attributes as library-level

                        when Attribute_Unchecked_Access |
                           Attribute_Unrestricted_Access =>
                           Add_Extra_Actual
                             (Make_Integer_Literal (Loc,
                                Intval => Scope_Depth (Standard_Standard)),
                              Extra_Accessibility (Formal));

                        --  No other cases of attributes returning access
                        --  values that can be passed to access parameters.

                        when others =>
                           raise Program_Error;

                     end case;

                  --  For allocators we pass the level of the execution of the
                  --  called subprogram, which is one greater than the current
                  --  scope level.

                  when N_Allocator =>
                     Add_Extra_Actual
                       (Make_Integer_Literal (Loc,
                          Intval => Scope_Depth (Current_Scope) + 1),
                        Extra_Accessibility (Formal));

                  --  For other cases we simply pass the level of the actual's
                  --  access type. The type is retrieved from Prev rather than
                  --  Prev_Orig, because in some cases Prev_Orig denotes an
                  --  original expression that has not been analyzed.

                  when others =>
                     Add_Extra_Actual
                       (Make_Integer_Literal (Loc,
                          Intval => Type_Access_Level (Etype (Prev))),
                        Extra_Accessibility (Formal));
               end case;
            end if;
         end if;

         --  Perform the check of 4.6(49) that prevents a null value from being
         --  passed as an actual to an access parameter. Note that the check
         --  is elided in the common cases of passing an access attribute or
         --  access parameter as an actual. Also, we currently don't enforce
         --  this check for expander-generated actuals and when -gnatdj is set.

         if Ada_Version >= Ada_2005 then

            --  Ada 2005 (AI-231): Check null-excluding access types. Note that
            --  the intent of 6.4.1(13) is that null-exclusion checks should
            --  not be done for 'out' parameters, even though it refers only
            --  to constraint checks, and a null_exclusion is not a constraint.
            --  Note that AI05-0196-1 corrects this mistake in the RM.

            if Is_Access_Type (Etype (Formal))
              and then Can_Never_Be_Null (Etype (Formal))
              and then Ekind (Formal) /= E_Out_Parameter
              and then Nkind (Prev) /= N_Raise_Constraint_Error
              and then (Known_Null (Prev)
                          or else not Can_Never_Be_Null (Etype (Prev)))
            then
               Install_Null_Excluding_Check (Prev);
            end if;

         --  Ada_Version < Ada_2005

         else
            if Ekind (Etype (Formal)) /= E_Anonymous_Access_Type
              or else Access_Checks_Suppressed (Subp)
            then
               null;

            elsif Debug_Flag_J then
               null;

            elsif not Comes_From_Source (Prev) then
               null;

            elsif Is_Entity_Name (Prev)
              and then Ekind (Etype (Prev)) = E_Anonymous_Access_Type
            then
               null;

            elsif Nkind_In (Prev, N_Allocator, N_Attribute_Reference) then
               null;

            --  Suppress null checks when passing to access parameters of Java
            --  and CIL subprograms. (Should this be done for other foreign
            --  conventions as well ???)

            elsif Convention (Subp) = Convention_Java
              or else Convention (Subp) = Convention_CIL
            then
               null;

            else
               Install_Null_Excluding_Check (Prev);
            end if;
         end if;

         --  Perform appropriate validity checks on parameters that
         --  are entities.

         if Validity_Checks_On then
            if  (Ekind (Formal) = E_In_Parameter
                   and then Validity_Check_In_Params)
              or else
                (Ekind (Formal) = E_In_Out_Parameter
                   and then Validity_Check_In_Out_Params)
            then
               --  If the actual is an indexed component of a packed type (or
               --  is an indexed or selected component whose prefix recursively
               --  meets this condition), it has not been expanded yet. It will
               --  be copied in the validity code that follows, and has to be
               --  expanded appropriately, so reanalyze it.

               --  What we do is just to unset analyzed bits on prefixes till
               --  we reach something that does not have a prefix.

               declare
                  Nod : Node_Id;

               begin
                  Nod := Actual;
                  while Nkind_In (Nod, N_Indexed_Component,
                                       N_Selected_Component)
                  loop
                     Set_Analyzed (Nod, False);
                     Nod := Prefix (Nod);
                  end loop;
               end;

               Ensure_Valid (Actual);
            end if;
         end if;

         --  For IN OUT and OUT parameters, ensure that subscripts are valid
         --  since this is a left side reference. We only do this for calls
         --  from the source program since we assume that compiler generated
         --  calls explicitly generate any required checks. We also need it
         --  only if we are doing standard validity checks, since clearly it is
         --  not needed if validity checks are off, and in subscript validity
         --  checking mode, all indexed components are checked with a call
         --  directly from Expand_N_Indexed_Component.

         if Comes_From_Source (Call_Node)
           and then Ekind (Formal) /= E_In_Parameter
           and then Validity_Checks_On
           and then Validity_Check_Default
           and then not Validity_Check_Subscripts
         then
            Check_Valid_Lvalue_Subscripts (Actual);
         end if;

         --  Mark any scalar OUT parameter that is a simple variable as no
         --  longer known to be valid (unless the type is always valid). This
         --  reflects the fact that if an OUT parameter is never set in a
         --  procedure, then it can become invalid on the procedure return.

         if Ekind (Formal) = E_Out_Parameter
           and then Is_Entity_Name (Actual)
           and then Ekind (Entity (Actual)) = E_Variable
           and then not Is_Known_Valid (Etype (Actual))
         then
            Set_Is_Known_Valid (Entity (Actual), False);
         end if;

         --  For an OUT or IN OUT parameter, if the actual is an entity, then
         --  clear current values, since they can be clobbered. We are probably
         --  doing this in more places than we need to, but better safe than
         --  sorry when it comes to retaining bad current values!

         if Ekind (Formal) /= E_In_Parameter
           and then Is_Entity_Name (Actual)
           and then Present (Entity (Actual))
         then
            declare
               Ent : constant Entity_Id := Entity (Actual);
               Sav : Node_Id;

            begin
               --  For an OUT or IN OUT parameter that is an assignable entity,
               --  we do not want to clobber the Last_Assignment field, since
               --  if it is set, it was precisely because it is indeed an OUT
               --  or IN OUT parameter! We do reset the Is_Known_Valid flag
               --  since the subprogram could have returned in invalid value.

               if (Ekind (Formal) = E_Out_Parameter
                     or else
                   Ekind (Formal) = E_In_Out_Parameter)
                 and then Is_Assignable (Ent)
               then
                  Sav := Last_Assignment (Ent);
                  Kill_Current_Values (Ent);
                  Set_Last_Assignment (Ent, Sav);
                  Set_Is_Known_Valid (Ent, False);

                  --  For all other cases, just kill the current values

               else
                  Kill_Current_Values (Ent);
               end if;
            end;
         end if;

         --  If the formal is class wide and the actual is an aggregate, force
         --  evaluation so that the back end who does not know about class-wide
         --  type, does not generate a temporary of the wrong size.

         if not Is_Class_Wide_Type (Etype (Formal)) then
            null;

         elsif Nkind (Actual) = N_Aggregate
           or else (Nkind (Actual) = N_Qualified_Expression
                     and then Nkind (Expression (Actual)) = N_Aggregate)
         then
            Force_Evaluation (Actual);
         end if;

         --  In a remote call, if the formal is of a class-wide type, check
         --  that the actual meets the requirements described in E.4(18).

         if Remote and then Is_Class_Wide_Type (Etype (Formal)) then
            Insert_Action (Actual,
              Make_Transportable_Check (Loc,
                Duplicate_Subexpr_Move_Checks (Actual)));
         end if;

         --  This label is required when skipping extra actual generation for
         --  Unchecked_Union parameters.

         <<Skip_Extra_Actual_Generation>>

         Param_Count := Param_Count + 1;
         Next_Actual (Actual);
         Next_Formal (Formal);
      end loop;

      --  If we are expanding a rhs of an assignment we need to check if tag
      --  propagation is needed. You might expect this processing to be in
      --  Analyze_Assignment but has to be done earlier (bottom-up) because the
      --  assignment might be transformed to a declaration for an unconstrained
      --  value if the expression is classwide.

      if Nkind (Call_Node) = N_Function_Call
        and then Is_Tag_Indeterminate (Call_Node)
        and then Is_Entity_Name (Name (Call_Node))
      then
         declare
            Ass : Node_Id := Empty;

         begin
            if Nkind (Parent (Call_Node)) = N_Assignment_Statement then
               Ass := Parent (Call_Node);

            elsif Nkind (Parent (Call_Node)) = N_Qualified_Expression
              and then Nkind (Parent (Parent (Call_Node))) =
                                                  N_Assignment_Statement
            then
               Ass := Parent (Parent (Call_Node));

            elsif Nkind (Parent (Call_Node)) = N_Explicit_Dereference
              and then Nkind (Parent (Parent (Call_Node))) =
                                                  N_Assignment_Statement
            then
               Ass := Parent (Parent (Call_Node));
            end if;

            if Present (Ass)
              and then Is_Class_Wide_Type (Etype (Name (Ass)))
            then
               if Is_Access_Type (Etype (Call_Node)) then
                  if Designated_Type (Etype (Call_Node)) /=
                    Root_Type (Etype (Name (Ass)))
                  then
                     Error_Msg_NE
                       ("tag-indeterminate expression "
                         & " must have designated type& (RM 5.2 (6))",
                         Call_Node, Root_Type (Etype (Name (Ass))));
                  else
                     Propagate_Tag (Name (Ass), Call_Node);
                  end if;

               elsif Etype (Call_Node) /= Root_Type (Etype (Name (Ass))) then
                  Error_Msg_NE
                    ("tag-indeterminate expression must have type&"
                     & "(RM 5.2 (6))",
                     Call_Node, Root_Type (Etype (Name (Ass))));

               else
                  Propagate_Tag (Name (Ass), Call_Node);
               end if;

               --  The call will be rewritten as a dispatching call, and
               --  expanded as such.

               return;
            end if;
         end;
      end if;

      --  Ada 2005 (AI-251): If some formal is a class-wide interface, expand
      --  it to point to the correct secondary virtual table

      if Nkind_In (Call_Node, N_Function_Call, N_Procedure_Call_Statement)
        and then CW_Interface_Formals_Present
      then
         Expand_Interface_Actuals (Call_Node);
      end if;

      --  Deals with Dispatch_Call if we still have a call, before expanding
      --  extra actuals since this will be done on the re-analysis of the
      --  dispatching call. Note that we do not try to shorten the actual list
      --  for a dispatching call, it would not make sense to do so. Expansion
      --  of dispatching calls is suppressed when VM_Target, because the VM
      --  back-ends directly handle the generation of dispatching calls and
      --  would have to undo any expansion to an indirect call.

      if Nkind_In (Call_Node, N_Function_Call, N_Procedure_Call_Statement)
        and then Present (Controlling_Argument (Call_Node))
      then
         declare
            Call_Typ   : constant Entity_Id := Etype (Call_Node);
            Typ        : constant Entity_Id := Find_Dispatching_Type (Subp);
            Eq_Prim_Op : Entity_Id := Empty;
            New_Call   : Node_Id;
            Param      : Node_Id;
            Prev_Call  : Node_Id;

         begin
            if not Is_Limited_Type (Typ) then
               Eq_Prim_Op := Find_Prim_Op (Typ, Name_Op_Eq);
            end if;

            if Tagged_Type_Expansion then
               Expand_Dispatching_Call (Call_Node);

               --  The following return is worrisome. Is it really OK to skip
               --  all remaining processing in this procedure ???

               return;

            --  VM targets

            else
               Apply_Tag_Checks (Call_Node);

               --  If this is a dispatching "=", we must first compare the
               --  tags so we generate: x.tag = y.tag and then x = y

               if Subp = Eq_Prim_Op then

                  --  Mark the node as analyzed to avoid reanalizing this
                  --  dispatching call (which would cause a never-ending loop)

                  Prev_Call := Relocate_Node (Call_Node);
                  Set_Analyzed (Prev_Call);

                  Param := First_Actual (Call_Node);
                  New_Call :=
                    Make_And_Then (Loc,
                      Left_Opnd =>
                           Make_Op_Eq (Loc,
                             Left_Opnd =>
                               Make_Selected_Component (Loc,
                                 Prefix        => New_Value (Param),
                                 Selector_Name =>
                                   New_Reference_To (First_Tag_Component (Typ),
                                                     Loc)),

                             Right_Opnd =>
                               Make_Selected_Component (Loc,
                                 Prefix        =>
                                   Unchecked_Convert_To (Typ,
                                     New_Value (Next_Actual (Param))),
                                 Selector_Name =>
                                   New_Reference_To
                                     (First_Tag_Component (Typ), Loc))),
                      Right_Opnd => Prev_Call);

                  Rewrite (Call_Node, New_Call);

                  Analyze_And_Resolve
                    (Call_Node, Call_Typ, Suppress => All_Checks);
               end if;

               --  Expansion of a dispatching call results in an indirect call,
               --  which in turn causes current values to be killed (see
               --  Resolve_Call), so on VM targets we do the call here to
               --  ensure consistent warnings between VM and non-VM targets.

               Kill_Current_Values;
            end if;

            --  If this is a dispatching "=" then we must update the reference
            --  to the call node because we generated:
            --     x.tag = y.tag and then x = y

            if Subp = Eq_Prim_Op then
               Call_Node := Right_Opnd (Call_Node);
            end if;
         end;
      end if;

      --  Similarly, expand calls to RCI subprograms on which pragma
      --  All_Calls_Remote applies. The rewriting will be reanalyzed
      --  later. Do this only when the call comes from source since we
      --  do not want such a rewriting to occur in expanded code.

      if Is_All_Remote_Call (Call_Node) then
         Expand_All_Calls_Remote_Subprogram_Call (Call_Node);

      --  Similarly, do not add extra actuals for an entry call whose entity
      --  is a protected procedure, or for an internal protected subprogram
      --  call, because it will be rewritten as a protected subprogram call
      --  and reanalyzed (see Expand_Protected_Subprogram_Call).

      elsif Is_Protected_Type (Scope (Subp))
         and then (Ekind (Subp) = E_Procedure
                    or else Ekind (Subp) = E_Function)
      then
         null;

      --  During that loop we gathered the extra actuals (the ones that
      --  correspond to Extra_Formals), so now they can be appended.

      else
         while Is_Non_Empty_List (Extra_Actuals) loop
            Add_Actual_Parameter (Remove_Head (Extra_Actuals));
         end loop;
      end if;

      --  At this point we have all the actuals, so this is the point at which
      --  the various expansion activities for actuals is carried out.

      Expand_Actuals (Call_Node, Subp);

      --  If the subprogram is a renaming, or if it is inherited, replace it in
      --  the call with the name of the actual subprogram being called. If this
      --  is a dispatching call, the run-time decides what to call. The Alias
      --  attribute does not apply to entries.

      if Nkind (Call_Node) /= N_Entry_Call_Statement
        and then No (Controlling_Argument (Call_Node))
        and then Present (Parent_Subp)
      then
         if Present (Inherited_From_Formal (Subp)) then
            Parent_Subp := Inherited_From_Formal (Subp);
         else
            Parent_Subp := Ultimate_Alias (Parent_Subp);
         end if;

         --  The below setting of Entity is suspect, see F109-018 discussion???

         Set_Entity (Name (Call_Node), Parent_Subp);

         if Is_Abstract_Subprogram (Parent_Subp)
           and then not In_Instance
         then
            Error_Msg_NE
              ("cannot call abstract subprogram &!",
               Name (Call_Node), Parent_Subp);
         end if;

         --  Inspect all formals of derived subprogram Subp. Compare parameter
         --  types with the parent subprogram and check whether an actual may
         --  need a type conversion to the corresponding formal of the parent
         --  subprogram.

         --  Not clear whether intrinsic subprograms need such conversions. ???

         if not Is_Intrinsic_Subprogram (Parent_Subp)
           or else Is_Generic_Instance (Parent_Subp)
         then
            declare
               procedure Convert (Act : Node_Id; Typ : Entity_Id);
               --  Rewrite node Act as a type conversion of Act to Typ. Analyze
               --  and resolve the newly generated construct.

               -------------
               -- Convert --
               -------------

               procedure Convert (Act : Node_Id; Typ : Entity_Id) is
               begin
                  Rewrite (Act, OK_Convert_To (Typ, Relocate_Node (Act)));
                  Analyze (Act);
                  Resolve (Act, Typ);
               end Convert;

               --  Local variables

               Actual_Typ : Entity_Id;
               Formal_Typ : Entity_Id;
               Parent_Typ : Entity_Id;

            begin
               Actual := First_Actual (Call_Node);
               Formal := First_Formal (Subp);
               Parent_Formal := First_Formal (Parent_Subp);
               while Present (Formal) loop
                  Actual_Typ := Etype (Actual);
                  Formal_Typ := Etype (Formal);
                  Parent_Typ := Etype (Parent_Formal);

                  --  For an IN parameter of a scalar type, the parent formal
                  --  type and derived formal type differ or the parent formal
                  --  type and actual type do not match statically.

                  if Is_Scalar_Type (Formal_Typ)
                    and then Ekind (Formal) = E_In_Parameter
                    and then Formal_Typ /= Parent_Typ
                    and then
                      not Subtypes_Statically_Match (Parent_Typ, Actual_Typ)
                    and then not Raises_Constraint_Error (Actual)
                  then
                     Convert (Actual, Parent_Typ);
                     Enable_Range_Check (Actual);

                     --  If the actual has been marked as requiring a range
                     --  check, then generate it here.

                     if Do_Range_Check (Actual) then
                        Set_Do_Range_Check (Actual, False);
                        Generate_Range_Check
                          (Actual, Etype (Formal), CE_Range_Check_Failed);
                     end if;

                  --  For access types, the parent formal type and actual type
                  --  differ.

                  elsif Is_Access_Type (Formal_Typ)
                    and then Base_Type (Parent_Typ) /= Base_Type (Actual_Typ)
                  then
                     if Ekind (Formal) /= E_In_Parameter then
                        Convert (Actual, Parent_Typ);

                     elsif Ekind (Parent_Typ) = E_Anonymous_Access_Type
                       and then Designated_Type (Parent_Typ) /=
                                Designated_Type (Actual_Typ)
                       and then not Is_Controlling_Formal (Formal)
                     then
                        --  This unchecked conversion is not necessary unless
                        --  inlining is enabled, because in that case the type
                        --  mismatch may become visible in the body about to be
                        --  inlined.

                        Rewrite (Actual,
                          Unchecked_Convert_To (Parent_Typ,
                            Relocate_Node (Actual)));
                        Analyze (Actual);
                        Resolve (Actual, Parent_Typ);
                     end if;

                  --  For array and record types, the parent formal type and
                  --  derived formal type have different sizes or pragma Pack
                  --  status.

                  elsif ((Is_Array_Type (Formal_Typ)
                            and then Is_Array_Type (Parent_Typ))
                       or else
                         (Is_Record_Type (Formal_Typ)
                            and then Is_Record_Type (Parent_Typ)))
                    and then
                      (Esize (Formal_Typ) /= Esize (Parent_Typ)
                         or else Has_Pragma_Pack (Formal_Typ) /=
                                 Has_Pragma_Pack (Parent_Typ))
                  then
                     Convert (Actual, Parent_Typ);
                  end if;

                  Next_Actual (Actual);
                  Next_Formal (Formal);
                  Next_Formal (Parent_Formal);
               end loop;
            end;
         end if;

         Orig_Subp := Subp;
         Subp := Parent_Subp;
      end if;

      --  Check for violation of No_Abort_Statements

      if Is_RTE (Subp, RE_Abort_Task) then
         Check_Restriction (No_Abort_Statements, Call_Node);

      --  Check for violation of No_Dynamic_Attachment

      elsif RTU_Loaded (Ada_Interrupts)
        and then (Is_RTE (Subp, RE_Is_Reserved)      or else
                  Is_RTE (Subp, RE_Is_Attached)      or else
                  Is_RTE (Subp, RE_Current_Handler)  or else
                  Is_RTE (Subp, RE_Attach_Handler)   or else
                  Is_RTE (Subp, RE_Exchange_Handler) or else
                  Is_RTE (Subp, RE_Detach_Handler)   or else
                  Is_RTE (Subp, RE_Reference))
      then
         Check_Restriction (No_Dynamic_Attachment, Call_Node);
      end if;

      --  Deal with case where call is an explicit dereference

      if Nkind (Name (Call_Node)) = N_Explicit_Dereference then

      --  Handle case of access to protected subprogram type

         if Is_Access_Protected_Subprogram_Type
              (Base_Type (Etype (Prefix (Name (Call_Node)))))
         then
            --  If this is a call through an access to protected operation, the
            --  prefix has the form (object'address, operation'access). Rewrite
            --  as a for other protected calls: the object is the 1st parameter
            --  of the list of actuals.

            declare
               Call : Node_Id;
               Parm : List_Id;
               Nam  : Node_Id;
               Obj  : Node_Id;
               Ptr  : constant Node_Id := Prefix (Name (Call_Node));

               T : constant Entity_Id :=
                     Equivalent_Type (Base_Type (Etype (Ptr)));

               D_T : constant Entity_Id :=
                       Designated_Type (Base_Type (Etype (Ptr)));

            begin
               Obj :=
                 Make_Selected_Component (Loc,
                   Prefix        => Unchecked_Convert_To (T, Ptr),
                   Selector_Name =>
                     New_Occurrence_Of (First_Entity (T), Loc));

               Nam :=
                 Make_Selected_Component (Loc,
                   Prefix        => Unchecked_Convert_To (T, Ptr),
                   Selector_Name =>
                     New_Occurrence_Of (Next_Entity (First_Entity (T)), Loc));

               Nam :=
                 Make_Explicit_Dereference (Loc,
                   Prefix => Nam);

               if Present (Parameter_Associations (Call_Node))  then
                  Parm := Parameter_Associations (Call_Node);
               else
                  Parm := New_List;
               end if;

               Prepend (Obj, Parm);

               if Etype (D_T) = Standard_Void_Type then
                  Call :=
                    Make_Procedure_Call_Statement (Loc,
                      Name                   => Nam,
                      Parameter_Associations => Parm);
               else
                  Call :=
                    Make_Function_Call (Loc,
                      Name                   => Nam,
                      Parameter_Associations => Parm);
               end if;

               Set_First_Named_Actual (Call, First_Named_Actual (Call_Node));
               Set_Etype (Call, Etype (D_T));

               --  We do not re-analyze the call to avoid infinite recursion.
               --  We analyze separately the prefix and the object, and set
               --  the checks on the prefix that would otherwise be emitted
               --  when resolving a call.

               Rewrite (Call_Node, Call);
               Analyze (Nam);
               Apply_Access_Check (Nam);
               Analyze (Obj);
               return;
            end;
         end if;
      end if;

      --  If this is a call to an intrinsic subprogram, then perform the
      --  appropriate expansion to the corresponding tree node and we
      --  are all done (since after that the call is gone!)

      --  In the case where the intrinsic is to be processed by the back end,
      --  the call to Expand_Intrinsic_Call will do nothing, which is fine,
      --  since the idea in this case is to pass the call unchanged. If the
      --  intrinsic is an inherited unchecked conversion, and the derived type
      --  is the target type of the conversion, we must retain it as the return
      --  type of the expression. Otherwise the expansion below, which uses the
      --  parent operation, will yield the wrong type.

      if Is_Intrinsic_Subprogram (Subp) then
         Expand_Intrinsic_Call (Call_Node, Subp);

         if Nkind (Call_Node) = N_Unchecked_Type_Conversion
           and then Parent_Subp /= Orig_Subp
           and then Etype (Parent_Subp) /= Etype (Orig_Subp)
         then
            Set_Etype (Call_Node, Etype (Orig_Subp));
         end if;

         return;
      end if;

      if Ekind_In (Subp, E_Function, E_Procedure) then

         --  We perform two simple optimization on calls:

         --  a) replace calls to null procedures unconditionally;

         --  b) for To_Address, just do an unchecked conversion. Not only is
         --  this efficient, but it also avoids order of elaboration problems
         --  when address clauses are inlined (address expression elaborated
         --  at the wrong point).

         --  We perform these optimization regardless of whether we are in the
         --  main unit or in a unit in the context of the main unit, to ensure
         --  that tree generated is the same in both cases, for Inspector use.

         if Is_RTE (Subp, RE_To_Address) then
            Rewrite (Call_Node,
              Unchecked_Convert_To
                (RTE (RE_Address), Relocate_Node (First_Actual (Call_Node))));
            return;

         elsif Is_Null_Procedure (Subp)  then
            Rewrite (Call_Node, Make_Null_Statement (Loc));
            return;
         end if;

         if Is_Inlined (Subp) then

            Inlined_Subprogram : declare
               Bod         : Node_Id;
               Must_Inline : Boolean := False;
               Spec        : constant Node_Id := Unit_Declaration_Node (Subp);
               Scop        : constant Entity_Id := Scope (Subp);

               function In_Unfrozen_Instance return Boolean;
               --  If the subprogram comes from an instance in the same unit,
               --  and the instance is not yet frozen, inlining might trigger
               --  order-of-elaboration problems in gigi.

               --------------------------
               -- In_Unfrozen_Instance --
               --------------------------

               function In_Unfrozen_Instance return Boolean is
                  S : Entity_Id;

               begin
                  S := Scop;
                  while Present (S)
                    and then S /= Standard_Standard
                  loop
                     if Is_Generic_Instance (S)
                       and then Present (Freeze_Node (S))
                       and then not Analyzed (Freeze_Node (S))
                     then
                        return True;
                     end if;

                     S := Scope (S);
                  end loop;

                  return False;
               end In_Unfrozen_Instance;

            --  Start of processing for Inlined_Subprogram

            begin
               --  Verify that the body to inline has already been seen, and
               --  that if the body is in the current unit the inlining does
               --  not occur earlier. This avoids order-of-elaboration problems
               --  in the back end.

               --  This should be documented in sinfo/einfo ???

               if No (Spec)
                 or else Nkind (Spec) /= N_Subprogram_Declaration
                 or else No (Body_To_Inline (Spec))
               then
                  Must_Inline := False;

               --  If this an inherited function that returns a private type,
               --  do not inline if the full view is an unconstrained array,
               --  because such calls cannot be inlined.

               elsif Present (Orig_Subp)
                 and then Is_Array_Type (Etype (Orig_Subp))
                 and then not Is_Constrained (Etype (Orig_Subp))
               then
                  Must_Inline := False;

               elsif In_Unfrozen_Instance then
                  Must_Inline := False;

               else
                  Bod := Body_To_Inline (Spec);

                  if (In_Extended_Main_Code_Unit (Call_Node)
                        or else In_Extended_Main_Code_Unit (Parent (Call_Node))
                        or else Has_Pragma_Inline_Always (Subp))
                    and then (not In_Same_Extended_Unit (Sloc (Bod), Loc)
                               or else
                                 Earlier_In_Extended_Unit (Sloc (Bod), Loc))
                  then
                     Must_Inline := True;

                  --  If we are compiling a package body that is not the main
                  --  unit, it must be for inlining/instantiation purposes,
                  --  in which case we inline the call to insure that the same
                  --  temporaries are generated when compiling the body by
                  --  itself. Otherwise link errors can occur.

                  --  If the function being called is itself in the main unit,
                  --  we cannot inline, because there is a risk of double
                  --  elaboration and/or circularity: the inlining can make
                  --  visible a private entity in the body of the main unit,
                  --  that gigi will see before its sees its proper definition.

                  elsif not (In_Extended_Main_Code_Unit (Call_Node))
                    and then In_Package_Body
                  then
                     Must_Inline := not In_Extended_Main_Source_Unit (Subp);
                  end if;
               end if;

               if Must_Inline then
                  Expand_Inlined_Call (Call_Node, Subp, Orig_Subp);

               else
                  --  Let the back end handle it

                  Add_Inlined_Body (Subp);

                  if Front_End_Inlining
                    and then Nkind (Spec) = N_Subprogram_Declaration
                    and then (In_Extended_Main_Code_Unit (Call_Node))
                    and then No (Body_To_Inline (Spec))
                    and then not Has_Completion (Subp)
                    and then In_Same_Extended_Unit (Sloc (Spec), Loc)
                  then
                     Cannot_Inline
                      ("cannot inline& (body not seen yet)?", Call_Node, Subp);
                  end if;
               end if;
            end Inlined_Subprogram;
         end if;
      end if;

      --  Check for protected subprogram. This is either an intra-object call,
      --  or a protected function call. Protected procedure calls are rewritten
      --  as entry calls and handled accordingly.

      --  In Ada 2005, this may be an indirect call to an access parameter that
      --  is an access_to_subprogram. In that case the anonymous type has a
      --  scope that is a protected operation, but the call is a regular one.
      --  In either case do not expand call if subprogram is eliminated.

      Scop := Scope (Subp);

      if Nkind (Call_Node) /= N_Entry_Call_Statement
        and then Is_Protected_Type (Scop)
        and then Ekind (Subp) /= E_Subprogram_Type
        and then not Is_Eliminated (Subp)
      then
         --  If the call is an internal one, it is rewritten as a call to the
         --  corresponding unprotected subprogram.

         Expand_Protected_Subprogram_Call (Call_Node, Subp, Scop);
      end if;

      --  Functions returning controlled objects need special attention:
      --  if the return type is limited, the context is an initialization
      --  and different processing applies. If the call is to a protected
      --  function, the expansion above will call Expand_Call recursively.
      --  To prevent a double attachment, check that the current call is
      --  not a rewriting of a protected function call.

      if Needs_Finalization (Etype (Subp)) then
         if not Is_Immutably_Limited_Type (Etype (Subp))
           and then
             (No (First_Formal (Subp))
                or else
                  not Is_Concurrent_Record_Type (Etype (First_Formal (Subp))))
         then
            Expand_Ctrl_Function_Call (Call_Node);

         --  Build-in-place function calls which appear in anonymous contexts
         --  need a transient scope to ensure the proper finalization of the
         --  intermediate result after its use.

         elsif Is_Build_In_Place_Function_Call (Call_Node)
           and then Nkind_In (Parent (Call_Node), N_Attribute_Reference,
                                          N_Function_Call,
                                          N_Indexed_Component,
                                          N_Object_Renaming_Declaration,
                                          N_Procedure_Call_Statement,
                                          N_Selected_Component,
                                          N_Slice)
         then
            Establish_Transient_Scope (Call_Node, Sec_Stack => True);
         end if;
      end if;

      --  Test for First_Optional_Parameter, and if so, truncate parameter list
      --  if there are optional parameters at the trailing end.
      --  Note: we never delete procedures for call via a pointer.

      if (Ekind (Subp) = E_Procedure or else Ekind (Subp) = E_Function)
        and then Present (First_Optional_Parameter (Subp))
      then
         declare
            Last_Keep_Arg : Node_Id;

         begin
            --  Last_Keep_Arg will hold the last actual that should be kept.
            --  If it remains empty at the end, it means that all parameters
            --  are optional.

            Last_Keep_Arg := Empty;

            --  Find first optional parameter, must be present since we checked
            --  the validity of the parameter before setting it.

            Formal := First_Formal (Subp);
            Actual := First_Actual (Call_Node);
            while Formal /= First_Optional_Parameter (Subp) loop
               Last_Keep_Arg := Actual;
               Next_Formal (Formal);
               Next_Actual (Actual);
            end loop;

            --  We have Formal and Actual pointing to the first potentially
            --  droppable argument. We can drop all the trailing arguments
            --  whose actual matches the default. Note that we know that all
            --  remaining formals have defaults, because we checked that this
            --  requirement was met before setting First_Optional_Parameter.

            --  We use Fully_Conformant_Expressions to check for identity
            --  between formals and actuals, which may miss some cases, but
            --  on the other hand, this is only an optimization (if we fail
            --  to truncate a parameter it does not affect functionality).
            --  So if the default is 3 and the actual is 1+2, we consider
            --  them unequal, which hardly seems worrisome.

            while Present (Formal) loop
               if not Fully_Conformant_Expressions
                    (Actual, Default_Value (Formal))
               then
                  Last_Keep_Arg := Actual;
               end if;

               Next_Formal (Formal);
               Next_Actual (Actual);
            end loop;

            --  If no arguments, delete entire list, this is the easy case

            if No (Last_Keep_Arg) then
               Set_Parameter_Associations (Call_Node, No_List);
               Set_First_Named_Actual (Call_Node, Empty);

            --  Case where at the last retained argument is positional. This
            --  is also an easy case, since the retained arguments are already
            --  in the right form, and we don't need to worry about the order
            --  of arguments that get eliminated.

            elsif Is_List_Member (Last_Keep_Arg) then
               while Present (Next (Last_Keep_Arg)) loop
                  Discard_Node (Remove_Next (Last_Keep_Arg));
               end loop;

               Set_First_Named_Actual (Call_Node, Empty);

            --  This is the annoying case where the last retained argument
            --  is a named parameter. Since the original arguments are not
            --  in declaration order, we may have to delete some fairly
            --  random collection of arguments.

            else
               declare
                  Temp   : Node_Id;
                  Passoc : Node_Id;

               begin
                  --  First step, remove all the named parameters from the
                  --  list (they are still chained using First_Named_Actual
                  --  and Next_Named_Actual, so we have not lost them!)

                  Temp := First (Parameter_Associations (Call_Node));

                  --  Case of all parameters named, remove them all

                  if Nkind (Temp) = N_Parameter_Association then
                     --  Suppress warnings to avoid warning on possible
                     --  infinite loop (because Call_Node is not modified).

                     pragma Warnings (Off);
                     while Is_Non_Empty_List
                             (Parameter_Associations (Call_Node))
                     loop
                        Temp :=
                          Remove_Head (Parameter_Associations (Call_Node));
                     end loop;
                     pragma Warnings (On);

                  --  Case of mixed positional/named, remove named parameters

                  else
                     while Nkind (Next (Temp)) /= N_Parameter_Association loop
                        Next (Temp);
                     end loop;

                     while Present (Next (Temp)) loop
                        Remove (Next (Temp));
                     end loop;
                  end if;

                  --  Now we loop through the named parameters, till we get
                  --  to the last one to be retained, adding them to the list.
                  --  Note that the Next_Named_Actual list does not need to be
                  --  touched since we are only reordering them on the actual
                  --  parameter association list.

                  Passoc := Parent (First_Named_Actual (Call_Node));
                  loop
                     Temp := Relocate_Node (Passoc);
                     Append_To
                       (Parameter_Associations (Call_Node), Temp);
                     exit when
                       Last_Keep_Arg = Explicit_Actual_Parameter (Passoc);
                     Passoc := Parent (Next_Named_Actual (Passoc));
                  end loop;

                  Set_Next_Named_Actual (Temp, Empty);

                  loop
                     Temp := Next_Named_Actual (Passoc);
                     exit when No (Temp);
                     Set_Next_Named_Actual
                       (Passoc, Next_Named_Actual (Parent (Temp)));
                  end loop;
               end;

            end if;
         end;
      end if;
   end Expand_Call;

   --------------------------
   -- Expand_Inlined_Call --
   --------------------------

   procedure Expand_Inlined_Call
    (N         : Node_Id;
     Subp      : Entity_Id;
     Orig_Subp : Entity_Id)
   is
      Loc       : constant Source_Ptr := Sloc (N);
      Is_Predef : constant Boolean :=
                   Is_Predefined_File_Name
                     (Unit_File_Name (Get_Source_Unit (Subp)));
      Orig_Bod  : constant Node_Id :=
                    Body_To_Inline (Unit_Declaration_Node (Subp));

      Blk      : Node_Id;
      Bod      : Node_Id;
      Decl     : Node_Id;
      Decls    : constant List_Id := New_List;
      Exit_Lab : Entity_Id := Empty;
      F        : Entity_Id;
      A        : Node_Id;
      Lab_Decl : Node_Id;
      Lab_Id   : Node_Id;
      New_A    : Node_Id;
      Num_Ret  : Int := 0;
      Ret_Type : Entity_Id;
      Targ     : Node_Id;
      Targ1    : Node_Id;
      Temp     : Entity_Id;
      Temp_Typ : Entity_Id;

      Return_Object : Entity_Id := Empty;
      --  Entity in declaration in an extended_return_statement

      Is_Unc : constant Boolean :=
                    Is_Array_Type (Etype (Subp))
                      and then not Is_Constrained (Etype (Subp));
      --  If the type returned by the function is unconstrained and the call
      --  can be inlined, special processing is required.

      procedure Make_Exit_Label;
      --  Build declaration for exit label to be used in Return statements,
      --  sets Exit_Lab (the label node) and Lab_Decl (corresponding implicit
      --  declaration). Does nothing if Exit_Lab already set.

      function Process_Formals (N : Node_Id) return Traverse_Result;
      --  Replace occurrence of a formal with the corresponding actual, or the
      --  thunk generated for it.

      function Process_Sloc (Nod : Node_Id) return Traverse_Result;
      --  If the call being expanded is that of an internal subprogram, set the
      --  sloc of the generated block to that of the call itself, so that the
      --  expansion is skipped by the "next" command in gdb.
      --  Same processing for a subprogram in a predefined file, e.g.
      --  Ada.Tags. If Debug_Generated_Code is true, suppress this change to
      --  simplify our own development.

      procedure Rewrite_Function_Call (N : Node_Id; Blk : Node_Id);
      --  If the function body is a single expression, replace call with
      --  expression, else insert block appropriately.

      procedure Rewrite_Procedure_Call (N : Node_Id; Blk : Node_Id);
      --  If procedure body has no local variables, inline body without
      --  creating block, otherwise rewrite call with block.

      function Formal_Is_Used_Once (Formal : Entity_Id) return Boolean;
      --  Determine whether a formal parameter is used only once in Orig_Bod

      ---------------------
      -- Make_Exit_Label --
      ---------------------

      procedure Make_Exit_Label is
         Lab_Ent : Entity_Id;
      begin
         if No (Exit_Lab) then
            Lab_Ent := Make_Temporary (Loc, 'L');
            Lab_Id  := New_Reference_To (Lab_Ent, Loc);
            Exit_Lab := Make_Label (Loc, Lab_Id);
            Lab_Decl :=
              Make_Implicit_Label_Declaration (Loc,
                Defining_Identifier  => Lab_Ent,
                Label_Construct      => Exit_Lab);
         end if;
      end Make_Exit_Label;

      ---------------------
      -- Process_Formals --
      ---------------------

      function Process_Formals (N : Node_Id) return Traverse_Result is
         A   : Entity_Id;
         E   : Entity_Id;
         Ret : Node_Id;

      begin
         if Is_Entity_Name (N)
           and then Present (Entity (N))
         then
            E := Entity (N);

            if Is_Formal (E)
              and then Scope (E) = Subp
            then
               A := Renamed_Object (E);

               --  Rewrite the occurrence of the formal into an occurrence of
               --  the actual. Also establish visibility on the proper view of
               --  the actual's subtype for the body's context (if the actual's
               --  subtype is private at the call point but its full view is
               --  visible to the body, then the inlined tree here must be
               --  analyzed with the full view).

               if Is_Entity_Name (A) then
                  Rewrite (N, New_Occurrence_Of (Entity (A), Loc));
                  Check_Private_View (N);

               elsif Nkind (A) = N_Defining_Identifier then
                  Rewrite (N, New_Occurrence_Of (A, Loc));
                  Check_Private_View (N);

               --  Numeric literal

               else
                  Rewrite (N, New_Copy (A));
               end if;
            end if;
            return Skip;

         elsif Is_Entity_Name (N)
           and then Present (Return_Object)
           and then Chars (N) = Chars (Return_Object)
         then
            --  Occurrence within an extended return statement. The return
            --  object is local to the body been inlined, and thus the generic
            --  copy is not analyzed yet, so we match by name, and replace it
            --  with target of call.

            if Nkind (Targ) = N_Defining_Identifier then
               Rewrite (N, New_Occurrence_Of (Targ, Loc));
            else
               Rewrite (N, New_Copy_Tree (Targ));
            end if;

            return Skip;

         elsif Nkind (N) = N_Simple_Return_Statement then
            if No (Expression (N)) then
               Make_Exit_Label;
               Rewrite (N,
                 Make_Goto_Statement (Loc, Name => New_Copy (Lab_Id)));

            else
               if Nkind (Parent (N)) = N_Handled_Sequence_Of_Statements
                 and then Nkind (Parent (Parent (N))) = N_Subprogram_Body
               then
                  --  Function body is a single expression. No need for
                  --  exit label.

                  null;

               else
                  Num_Ret := Num_Ret + 1;
                  Make_Exit_Label;
               end if;

               --  Because of the presence of private types, the views of the
               --  expression and the context may be different, so place an
               --  unchecked conversion to the context type to avoid spurious
               --  errors, e.g. when the expression is a numeric literal and
               --  the context is private. If the expression is an aggregate,
               --  use a qualified expression, because an aggregate is not a
               --  legal argument of a conversion.

               if Nkind_In (Expression (N), N_Aggregate, N_Null) then
                  Ret :=
                    Make_Qualified_Expression (Sloc (N),
                       Subtype_Mark => New_Occurrence_Of (Ret_Type, Sloc (N)),
                       Expression => Relocate_Node (Expression (N)));
               else
                  Ret :=
                    Unchecked_Convert_To
                      (Ret_Type, Relocate_Node (Expression (N)));
               end if;

               if Nkind (Targ) = N_Defining_Identifier then
                  Rewrite (N,
                    Make_Assignment_Statement (Loc,
                      Name => New_Occurrence_Of (Targ, Loc),
                      Expression => Ret));
               else
                  Rewrite (N,
                    Make_Assignment_Statement (Loc,
                      Name => New_Copy (Targ),
                      Expression => Ret));
               end if;

               Set_Assignment_OK (Name (N));

               if Present (Exit_Lab) then
                  Insert_After (N,
                    Make_Goto_Statement (Loc,
                      Name => New_Copy (Lab_Id)));
               end if;
            end if;

            return OK;

         elsif Nkind (N) = N_Extended_Return_Statement then

            --  An extended return becomes a block whose first statement is
            --  the assignment of the initial expression of the return object
            --  to the target of the call itself.

            declare
               Return_Decl : constant Entity_Id :=
                               First (Return_Object_Declarations (N));
               Assign      : Node_Id;

            begin
               Return_Object := Defining_Identifier (Return_Decl);

               if Present (Expression (Return_Decl)) then
                  if Nkind (Targ) = N_Defining_Identifier then
                     Assign :=
                       Make_Assignment_Statement (Loc,
                         Name => New_Occurrence_Of (Targ, Loc),
                         Expression => Expression (Return_Decl));
                  else
                     Assign :=
                       Make_Assignment_Statement (Loc,
                         Name => New_Copy (Targ),
                         Expression => Expression (Return_Decl));
                  end if;

                  Set_Assignment_OK (Name (Assign));
                  Prepend (Assign,
                    Statements (Handled_Statement_Sequence (N)));
               end if;

               Rewrite (N,
                 Make_Block_Statement (Loc,
                    Handled_Statement_Sequence =>
                      Handled_Statement_Sequence (N)));

               return OK;
            end;

         --  Remove pragma Unreferenced since it may refer to formals that
         --  are not visible in the inlined body, and in any case we will
         --  not be posting warnings on the inlined body so it is unneeded.

         elsif Nkind (N) = N_Pragma
           and then Pragma_Name (N) = Name_Unreferenced
         then
            Rewrite (N, Make_Null_Statement (Sloc (N)));
            return OK;

         else
            return OK;
         end if;
      end Process_Formals;

      procedure Replace_Formals is new Traverse_Proc (Process_Formals);

      ------------------
      -- Process_Sloc --
      ------------------

      function Process_Sloc (Nod : Node_Id) return Traverse_Result is
      begin
         if not Debug_Generated_Code then
            Set_Sloc (Nod, Sloc (N));
            Set_Comes_From_Source (Nod, False);
         end if;

         return OK;
      end Process_Sloc;

      procedure Reset_Slocs is new Traverse_Proc (Process_Sloc);

      ---------------------------
      -- Rewrite_Function_Call --
      ---------------------------

      procedure Rewrite_Function_Call (N : Node_Id; Blk : Node_Id) is
         HSS : constant Node_Id := Handled_Statement_Sequence (Blk);
         Fst : constant Node_Id := First (Statements (HSS));

      begin
         --  Optimize simple case: function body is a single return statement,
         --  which has been expanded into an assignment.

         if Is_Empty_List (Declarations (Blk))
           and then Nkind (Fst) = N_Assignment_Statement
           and then No (Next (Fst))
         then

            --  The function call may have been rewritten as the temporary
            --  that holds the result of the call, in which case remove the
            --  now useless declaration.

            if Nkind (N) = N_Identifier
              and then Nkind (Parent (Entity (N))) = N_Object_Declaration
            then
               Rewrite (Parent (Entity (N)), Make_Null_Statement (Loc));
            end if;

            Rewrite (N, Expression (Fst));

         elsif Nkind (N) = N_Identifier
           and then Nkind (Parent (Entity (N))) = N_Object_Declaration
         then
            --  The block assigns the result of the call to the temporary

            Insert_After (Parent (Entity (N)), Blk);

         elsif Nkind (Parent (N)) = N_Assignment_Statement
           and then
            (Is_Entity_Name (Name (Parent (N)))
               or else
                  (Nkind (Name (Parent (N))) = N_Explicit_Dereference
                    and then Is_Entity_Name (Prefix (Name (Parent (N))))))
         then
            --  Replace assignment with the block

            declare
               Original_Assignment : constant Node_Id := Parent (N);

            begin
               --  Preserve the original assignment node to keep the complete
               --  assignment subtree consistent enough for Analyze_Assignment
               --  to proceed (specifically, the original Lhs node must still
               --  have an assignment statement as its parent).

               --  We cannot rely on Original_Node to go back from the block
               --  node to the assignment node, because the assignment might
               --  already be a rewrite substitution.

               Discard_Node (Relocate_Node (Original_Assignment));
               Rewrite (Original_Assignment, Blk);
            end;

         elsif Nkind (Parent (N)) = N_Object_Declaration then
            Set_Expression (Parent (N), Empty);
            Insert_After (Parent (N), Blk);

         elsif Is_Unc then
            Insert_Before (Parent (N), Blk);
         end if;
      end Rewrite_Function_Call;

      ----------------------------
      -- Rewrite_Procedure_Call --
      ----------------------------

      procedure Rewrite_Procedure_Call (N : Node_Id; Blk : Node_Id) is
         HSS  : constant Node_Id := Handled_Statement_Sequence (Blk);
      begin
         --  If there is a transient scope for N, this will be the scope of the
         --  actions for N, and the statements in Blk need to be within this
         --  scope. For example, they need to have visibility on the constant
         --  declarations created for the formals.

         --  If N needs no transient scope, and if there are no declarations in
         --  the inlined body, we can do a little optimization and insert the
         --  statements for the body directly after N, and rewrite N to a
         --  null statement, instead of rewriting N into a full-blown block
         --  statement.

         if not Scope_Is_Transient
           and then Is_Empty_List (Declarations (Blk))
         then
            Insert_List_After (N, Statements (HSS));
            Rewrite (N, Make_Null_Statement (Loc));
         else
            Rewrite (N, Blk);
         end if;
      end Rewrite_Procedure_Call;

      -------------------------
      -- Formal_Is_Used_Once --
      -------------------------

      function Formal_Is_Used_Once (Formal : Entity_Id) return Boolean is
         Use_Counter : Int := 0;

         function Count_Uses (N : Node_Id) return Traverse_Result;
         --  Traverse the tree and count the uses of the formal parameter.
         --  In this case, for optimization purposes, we do not need to
         --  continue the traversal once more than one use is encountered.

         ----------------
         -- Count_Uses --
         ----------------

         function Count_Uses (N : Node_Id) return Traverse_Result is
         begin
            --  The original node is an identifier

            if Nkind (N) = N_Identifier
              and then Present (Entity (N))

               --  Original node's entity points to the one in the copied body

              and then Nkind (Entity (N)) = N_Identifier
              and then Present (Entity (Entity (N)))

               --  The entity of the copied node is the formal parameter

              and then Entity (Entity (N)) = Formal
            then
               Use_Counter := Use_Counter + 1;

               if Use_Counter > 1 then

                  --  Denote more than one use and abandon the traversal

                  Use_Counter := 2;
                  return Abandon;

               end if;
            end if;

            return OK;
         end Count_Uses;

         procedure Count_Formal_Uses is new Traverse_Proc (Count_Uses);

      --  Start of processing for Formal_Is_Used_Once

      begin
         Count_Formal_Uses (Orig_Bod);
         return Use_Counter = 1;
      end Formal_Is_Used_Once;

   --  Start of processing for Expand_Inlined_Call

   begin

      --  Check for an illegal attempt to inline a recursive procedure. If the
      --  subprogram has parameters this is detected when trying to supply a
      --  binding for parameters that already have one. For parameterless
      --  subprograms this must be done explicitly.

      if In_Open_Scopes (Subp) then
         Error_Msg_N ("call to recursive subprogram cannot be inlined?", N);
         Set_Is_Inlined (Subp, False);
         return;
      end if;

      if Nkind (Orig_Bod) = N_Defining_Identifier
        or else Nkind (Orig_Bod) = N_Defining_Operator_Symbol
      then
         --  Subprogram is renaming_as_body. Calls occurring after the renaming
         --  can be replaced with calls to the renamed entity directly, because
         --  the subprograms are subtype conformant. If the renamed subprogram
         --  is an inherited operation, we must redo the expansion because
         --  implicit conversions may be needed. Similarly, if the renamed
         --  entity is inlined, expand the call for further optimizations.

         Set_Name (N, New_Occurrence_Of (Orig_Bod, Loc));

         if Present (Alias (Orig_Bod)) or else Is_Inlined (Orig_Bod) then
            Expand_Call (N);
         end if;

         return;
      end if;

      --  Use generic machinery to copy body of inlined subprogram, as if it
      --  were an instantiation, resetting source locations appropriately, so
      --  that nested inlined calls appear in the main unit.

      Save_Env (Subp, Empty);
      Set_Copied_Sloc_For_Inlined_Body (N, Defining_Entity (Orig_Bod));

      Bod := Copy_Generic_Node (Orig_Bod, Empty, Instantiating => True);
      Blk :=
        Make_Block_Statement (Loc,
          Declarations => Declarations (Bod),
          Handled_Statement_Sequence => Handled_Statement_Sequence (Bod));

      if No (Declarations (Bod)) then
         Set_Declarations (Blk, New_List);
      end if;

      --  For the unconstrained case, capture the name of the local
      --  variable that holds the result. This must be the first declaration
      --  in the block, because its bounds cannot depend on local variables.
      --  Otherwise there is no way to declare the result outside of the
      --  block. Needless to say, in general the bounds will depend on the
      --  actuals in the call.

      if Is_Unc then
         Targ1 := Defining_Identifier (First (Declarations (Blk)));
      end if;

      --  If this is a derived function, establish the proper return type

      if Present (Orig_Subp)
        and then Orig_Subp /= Subp
      then
         Ret_Type := Etype (Orig_Subp);
      else
         Ret_Type := Etype (Subp);
      end if;

      --  Create temporaries for the actuals that are expressions, or that
      --  are scalars and require copying to preserve semantics.

      F := First_Formal (Subp);
      A := First_Actual (N);
      while Present (F) loop
         if Present (Renamed_Object (F)) then
            Error_Msg_N ("cannot inline call to recursive subprogram", N);
            return;
         end if;

         --  If the argument may be a controlling argument in a call within
         --  the inlined body, we must preserve its classwide nature to insure
         --  that dynamic dispatching take place subsequently. If the formal
         --  has a constraint it must be preserved to retain the semantics of
         --  the body.

         if Is_Class_Wide_Type (Etype (F))
           or else (Is_Access_Type (Etype (F))
                      and then
                    Is_Class_Wide_Type (Designated_Type (Etype (F))))
         then
            Temp_Typ := Etype (F);

         elsif Base_Type (Etype (F)) = Base_Type (Etype (A))
           and then Etype (F) /= Base_Type (Etype (F))
         then
            Temp_Typ := Etype (F);

         else
            Temp_Typ := Etype (A);
         end if;

         --  If the actual is a simple name or a literal, no need to
         --  create a temporary, object can be used directly.

         --  If the actual is a literal and the formal has its address taken,
         --  we cannot pass the literal itself as an argument, so its value
         --  must be captured in a temporary.

         if (Is_Entity_Name (A)
              and then
               (not Is_Scalar_Type (Etype (A))
                 or else Ekind (Entity (A)) = E_Enumeration_Literal))

         --  When the actual is an identifier and the corresponding formal
         --  is used only once in the original body, the formal can be
         --  substituted directly with the actual parameter.

           or else (Nkind (A) = N_Identifier
             and then Formal_Is_Used_Once (F))

           or else
             (Nkind_In (A, N_Real_Literal,
                            N_Integer_Literal,
                            N_Character_Literal)
                and then not Address_Taken (F))
         then
            if Etype (F) /= Etype (A) then
               Set_Renamed_Object
                (F, Unchecked_Convert_To (Etype (F), Relocate_Node (A)));
            else
               Set_Renamed_Object (F, A);
            end if;

         else
            Temp := Make_Temporary (Loc, 'C');

            --  If the actual for an in/in-out parameter is a view conversion,
            --  make it into an unchecked conversion, given that an untagged
            --  type conversion is not a proper object for a renaming.

            --  In-out conversions that involve real conversions have already
            --  been transformed in Expand_Actuals.

            if Nkind (A) = N_Type_Conversion
              and then Ekind (F) /= E_In_Parameter
            then
               New_A :=
                 Make_Unchecked_Type_Conversion (Loc,
                   Subtype_Mark => New_Occurrence_Of (Etype (F), Loc),
                   Expression   => Relocate_Node (Expression (A)));

            elsif Etype (F) /= Etype (A) then
               New_A := Unchecked_Convert_To (Etype (F), Relocate_Node (A));
               Temp_Typ := Etype (F);

            else
               New_A := Relocate_Node (A);
            end if;

            Set_Sloc (New_A, Sloc (N));

            --  If the actual has a by-reference type, it cannot be copied, so
            --  its value is captured in a renaming declaration. Otherwise
            --  declare a local constant initialized with the actual.

            --  We also use a renaming declaration for expressions of an array
            --  type that is not bit-packed, both for efficiency reasons and to
            --  respect the semantics of the call: in most cases the original
            --  call will pass the parameter by reference, and thus the inlined
            --  code will have the same semantics.

            if Ekind (F) = E_In_Parameter
              and then not Is_Limited_Type (Etype (A))
              and then not Is_Tagged_Type  (Etype (A))
              and then
               (not Is_Array_Type (Etype (A))
                 or else not Is_Object_Reference (A)
                 or else Is_Bit_Packed_Array (Etype (A)))
            then
               Decl :=
                 Make_Object_Declaration (Loc,
                   Defining_Identifier => Temp,
                   Constant_Present => True,
                   Object_Definition => New_Occurrence_Of (Temp_Typ, Loc),
                   Expression => New_A);
            else
               Decl :=
                 Make_Object_Renaming_Declaration (Loc,
                   Defining_Identifier => Temp,
                   Subtype_Mark        => New_Occurrence_Of (Temp_Typ, Loc),
                   Name                => New_A);
            end if;

            Append (Decl, Decls);
            Set_Renamed_Object (F, Temp);
         end if;

         Next_Formal (F);
         Next_Actual (A);
      end loop;

      --  Establish target of function call. If context is not assignment or
      --  declaration, create a temporary as a target. The declaration for
      --  the temporary may be subsequently optimized away if the body is a
      --  single expression, or if the left-hand side of the assignment is
      --  simple enough, i.e. an entity or an explicit dereference of one.

      if Ekind (Subp) = E_Function then
         if Nkind (Parent (N)) = N_Assignment_Statement
           and then Is_Entity_Name (Name (Parent (N)))
         then
            Targ := Name (Parent (N));

         elsif Nkind (Parent (N)) = N_Assignment_Statement
           and then Nkind (Name (Parent (N))) = N_Explicit_Dereference
           and then Is_Entity_Name (Prefix (Name (Parent (N))))
         then
            Targ := Name (Parent (N));

         elsif Nkind (Parent (N)) = N_Object_Declaration
           and then Is_Limited_Type (Etype (Subp))
         then
            Targ := Defining_Identifier (Parent (N));

         else
            --  Replace call with temporary and create its declaration

            Temp := Make_Temporary (Loc, 'C');
            Set_Is_Internal (Temp);

            --  For the unconstrained case, the generated temporary has the
            --  same constrained declaration as the result variable. It may
            --  eventually be possible to remove that temporary and use the
            --  result variable directly.

            if Is_Unc then
               Decl :=
                 Make_Object_Declaration (Loc,
                   Defining_Identifier => Temp,
                   Object_Definition =>
                     New_Copy_Tree (Object_Definition (Parent (Targ1))));

               Replace_Formals (Decl);

            else
               Decl :=
                 Make_Object_Declaration (Loc,
                   Defining_Identifier => Temp,
                   Object_Definition =>
                     New_Occurrence_Of (Ret_Type, Loc));

               Set_Etype (Temp, Ret_Type);
            end if;

            Set_No_Initialization (Decl);
            Append (Decl, Decls);
            Rewrite (N, New_Occurrence_Of (Temp, Loc));
            Targ := Temp;
         end if;
      end if;

      Insert_Actions (N, Decls);

      --  Traverse the tree and replace formals with actuals or their thunks.
      --  Attach block to tree before analysis and rewriting.

      Replace_Formals (Blk);
      Set_Parent (Blk, N);

      if not Comes_From_Source (Subp)
        or else Is_Predef
      then
         Reset_Slocs (Blk);
      end if;

      if Present (Exit_Lab) then

         --  If the body was a single expression, the single return statement
         --  and the corresponding label are useless.

         if Num_Ret = 1
           and then
             Nkind (Last (Statements (Handled_Statement_Sequence (Blk)))) =
               N_Goto_Statement
         then
            Remove (Last (Statements (Handled_Statement_Sequence (Blk))));
         else
            Append (Lab_Decl, (Declarations (Blk)));
            Append (Exit_Lab, Statements (Handled_Statement_Sequence (Blk)));
         end if;
      end if;

      --  Analyze Blk with In_Inlined_Body set, to avoid spurious errors on
      --  conflicting private views that Gigi would ignore. If this is a
      --  predefined unit, analyze with checks off, as is done in the non-
      --  inlined run-time units.

      declare
         I_Flag : constant Boolean := In_Inlined_Body;

      begin
         In_Inlined_Body := True;

         if Is_Predef then
            declare
               Style : constant Boolean := Style_Check;
            begin
               Style_Check := False;
               Analyze (Blk, Suppress => All_Checks);
               Style_Check := Style;
            end;

         else
            Analyze (Blk);
         end if;

         In_Inlined_Body := I_Flag;
      end;

      if Ekind (Subp) = E_Procedure then
         Rewrite_Procedure_Call (N, Blk);
      else
         Rewrite_Function_Call (N, Blk);

         --  For the unconstrained case, the replacement of the call has been
         --  made prior to the complete analysis of the generated declarations.
         --  Propagate the proper type now.

         if Is_Unc then
            if Nkind (N) = N_Identifier then
               Set_Etype (N, Etype (Entity (N)));
            else
               Set_Etype (N, Etype (Targ1));
            end if;
         end if;
      end if;

      Restore_Env;

      --  Cleanup mapping between formals and actuals for other expansions

      F := First_Formal (Subp);
      while Present (F) loop
         Set_Renamed_Object (F, Empty);
         Next_Formal (F);
      end loop;
   end Expand_Inlined_Call;

   ----------------------------------------
   -- Expand_N_Extended_Return_Statement --
   ----------------------------------------

   --  If there is a Handled_Statement_Sequence, we rewrite this:

   --     return Result : T := <expression> do
   --        <handled_seq_of_stms>
   --     end return;

   --  to be:

   --     declare
   --        Result : T := <expression>;
   --     begin
   --        <handled_seq_of_stms>
   --        return Result;
   --     end;

   --  Otherwise (no Handled_Statement_Sequence), we rewrite this:

   --     return Result : T := <expression>;

   --  to be:

   --     return <expression>;

   --  unless it's build-in-place or there's no <expression>, in which case
   --  we generate:

   --     declare
   --        Result : T := <expression>;
   --     begin
   --        return Result;
   --     end;

   --  Note that this case could have been written by the user as an extended
   --  return statement, or could have been transformed to this from a simple
   --  return statement.

   --  That is, we need to have a reified return object if there are statements
   --  (which might refer to it) or if we're doing build-in-place (so we can
   --  set its address to the final resting place or if there is no expression
   --  (in which case default initial values might need to be set).

   procedure Expand_N_Extended_Return_Statement (N : Node_Id) is
      Loc : constant Source_Ptr := Sloc (N);

      Return_Object_Entity : constant Entity_Id :=
                               First_Entity (Return_Statement_Entity (N));
      Return_Object_Decl   : constant Node_Id :=
                               Parent (Return_Object_Entity);
      Parent_Function      : constant Entity_Id :=
                               Return_Applies_To (Return_Statement_Entity (N));
      Parent_Function_Typ  : constant Entity_Id := Etype (Parent_Function);
      Is_Build_In_Place    : constant Boolean :=
                               Is_Build_In_Place_Function (Parent_Function);

      Return_Stm      : Node_Id;
      Statements      : List_Id;
      Handled_Stm_Seq : Node_Id;
      Result          : Node_Id;
      Exp             : Node_Id;

      function Has_Controlled_Parts (Typ : Entity_Id) return Boolean;
      --  Determine whether type Typ is controlled or contains a controlled
      --  subcomponent.

      function Move_Activation_Chain return Node_Id;
      --  Construct a call to System.Tasking.Stages.Move_Activation_Chain
      --  with parameters:
      --    From         current activation chain
      --    To           activation chain passed in by the caller
      --    New_Master   master passed in by the caller

      function Move_Final_List return Node_Id;
      --  Construct call to System.Finalization_Implementation.Move_Final_List
      --  with parameters:
      --
      --    From         finalization list of the return statement
      --    To           finalization list passed in by the caller

      --------------------------
      -- Has_Controlled_Parts --
      --------------------------

      function Has_Controlled_Parts (Typ : Entity_Id) return Boolean is
      begin
         return
           Is_Controlled (Typ)
             or else Has_Controlled_Component (Typ);
      end Has_Controlled_Parts;

      ---------------------------
      -- Move_Activation_Chain --
      ---------------------------

      function Move_Activation_Chain return Node_Id is
         Activation_Chain_Formal : constant Entity_Id :=
                                     Build_In_Place_Formal
                                       (Parent_Function, BIP_Activation_Chain);
         To                      : constant Node_Id :=
                                     New_Reference_To
                                       (Activation_Chain_Formal, Loc);
         Master_Formal           : constant Entity_Id :=
                                     Build_In_Place_Formal
                                       (Parent_Function, BIP_Master);
         New_Master              : constant Node_Id :=
                                     New_Reference_To (Master_Formal, Loc);

         Chain_Entity : Entity_Id;
         From         : Node_Id;

      begin
         Chain_Entity := First_Entity (Return_Statement_Entity (N));
         while Chars (Chain_Entity) /= Name_uChain loop
            Chain_Entity := Next_Entity (Chain_Entity);
         end loop;

         From :=
           Make_Attribute_Reference (Loc,
             Prefix         => New_Reference_To (Chain_Entity, Loc),
             Attribute_Name => Name_Unrestricted_Access);
         --  ??? Not clear why "Make_Identifier (Loc, Name_uChain)" doesn't
         --  work, instead of "New_Reference_To (Chain_Entity, Loc)" above.

         return
           Make_Procedure_Call_Statement (Loc,
             Name => New_Reference_To (RTE (RE_Move_Activation_Chain), Loc),
             Parameter_Associations => New_List (From, To, New_Master));
      end Move_Activation_Chain;

      ---------------------
      -- Move_Final_List --
      ---------------------

      function Move_Final_List return Node_Id is
         Flist : constant Entity_Id  :=
                   Finalization_Chain_Entity (Return_Statement_Entity (N));

         From : constant Node_Id := New_Reference_To (Flist, Loc);

         Caller_Final_List : constant Entity_Id :=
                               Build_In_Place_Formal
                                 (Parent_Function, BIP_Final_List);

         To : constant Node_Id := New_Reference_To (Caller_Final_List, Loc);

      begin
         --  Catch cases where a finalization chain entity has not been
         --  associated with the return statement entity.

         pragma Assert (Present (Flist));

         --  Build required call

         return
           Make_If_Statement (Loc,
             Condition =>
               Make_Op_Ne (Loc,
                 Left_Opnd  => New_Copy (From),
                 Right_Opnd => New_Node (N_Null, Loc)),
             Then_Statements =>
               New_List (
                 Make_Procedure_Call_Statement (Loc,
                   Name => New_Reference_To (RTE (RE_Move_Final_List), Loc),
                   Parameter_Associations => New_List (From, To))));
      end Move_Final_List;

   --  Start of processing for Expand_N_Extended_Return_Statement

   begin
      if Nkind (Return_Object_Decl) = N_Object_Declaration then
         Exp := Expression (Return_Object_Decl);
      else
         Exp := Empty;
      end if;

      Handled_Stm_Seq := Handled_Statement_Sequence (N);

      --  Build a simple_return_statement that returns the return object when
      --  there is a statement sequence, or no expression, or the result will
      --  be built in place. Note however that we currently do this for all
      --  composite cases, even though nonlimited composite results are not yet
      --  built in place (though we plan to do so eventually).

      if Present (Handled_Stm_Seq)
        or else Is_Composite_Type (Etype (Parent_Function))
        or else No (Exp)
      then
         if No (Handled_Stm_Seq) then
            Statements := New_List;

         --  If the extended return has a handled statement sequence, then wrap
         --  it in a block and use the block as the first statement.

         else
            Statements :=
              New_List (Make_Block_Statement (Loc,
                          Declarations => New_List,
                          Handled_Statement_Sequence => Handled_Stm_Seq));
         end if;

         --  If control gets past the above Statements, we have successfully
         --  completed the return statement. If the result type has controlled
         --  parts and the return is for a build-in-place function, then we
         --  call Move_Final_List to transfer responsibility for finalization
         --  of the return object to the caller. An alternative would be to
         --  declare a Success flag in the function, initialize it to False,
         --  and set it to True here. Then move the Move_Final_List call into
         --  the cleanup code, and check Success. If Success then make a call
         --  to Move_Final_List else do finalization. Then we can remove the
         --  abort-deferral and the nulling-out of the From parameter from
         --  Move_Final_List. Note that the current method is not quite correct
         --  in the rather obscure case of a select-then-abort statement whose
         --  abortable part contains the return statement.

         --  Check the type of the function to determine whether to move the
         --  finalization list. A special case arises when processing a simple
         --  return statement which has been rewritten as an extended return.
         --  In that case check the type of the returned object or the original
         --  expression.

         if Is_Build_In_Place
           and then
               (Has_Controlled_Parts (Parent_Function_Typ)
                 or else (Is_Class_Wide_Type (Parent_Function_Typ)
                           and then
                        Has_Controlled_Parts (Root_Type (Parent_Function_Typ)))
                 or else Has_Controlled_Parts (Etype (Return_Object_Entity))
                 or else (Present (Exp)
                           and then Has_Controlled_Parts (Etype (Exp))))
         then
            Append_To (Statements, Move_Final_List);
         end if;

         --  Similarly to the above Move_Final_List, if the result type
         --  contains tasks, we call Move_Activation_Chain. Later, the cleanup
         --  code will call Complete_Master, which will terminate any
         --  unactivated tasks belonging to the return statement master. But
         --  Move_Activation_Chain updates their master to be that of the
         --  caller, so they will not be terminated unless the return statement
         --  completes unsuccessfully due to exception, abort, goto, or exit.
         --  As a formality, we test whether the function requires the result
         --  to be built in place, though that's necessarily true for the case
         --  of result types with task parts.

         if Is_Build_In_Place and Has_Task (Etype (Parent_Function)) then
            Append_To (Statements, Move_Activation_Chain);
         end if;

         --  Build a simple_return_statement that returns the return object

         Return_Stm :=
           Make_Simple_Return_Statement (Loc,
             Expression => New_Occurrence_Of (Return_Object_Entity, Loc));
         Append_To (Statements, Return_Stm);

         Handled_Stm_Seq :=
           Make_Handled_Sequence_Of_Statements (Loc, Statements);
      end if;

      --  Case where we build a block

      if Present (Handled_Stm_Seq) then
         Result :=
           Make_Block_Statement (Loc,
             Declarations => Return_Object_Declarations (N),
             Handled_Statement_Sequence => Handled_Stm_Seq);

         --  We set the entity of the new block statement to be that of the
         --  return statement. This is necessary so that various fields, such
         --  as Finalization_Chain_Entity carry over from the return statement
         --  to the block. Note that this block is unusual, in that its entity
         --  is an E_Return_Statement rather than an E_Block.

         Set_Identifier
           (Result, New_Occurrence_Of (Return_Statement_Entity (N), Loc));

         --  If the object decl was already rewritten as a renaming, then
         --  we don't want to do the object allocation and transformation of
         --  of the return object declaration to a renaming. This case occurs
         --  when the return object is initialized by a call to another
         --  build-in-place function, and that function is responsible for the
         --  allocation of the return object.

         if Is_Build_In_Place
           and then
             Nkind (Return_Object_Decl) = N_Object_Renaming_Declaration
         then
            pragma Assert (Nkind (Original_Node (Return_Object_Decl)) =
                            N_Object_Declaration
              and then Is_Build_In_Place_Function_Call
                         (Expression (Original_Node (Return_Object_Decl))));

            Set_By_Ref (Return_Stm);  -- Return build-in-place results by ref

         elsif Is_Build_In_Place then

            --  Locate the implicit access parameter associated with the
            --  caller-supplied return object and convert the return
            --  statement's return object declaration to a renaming of a
            --  dereference of the access parameter. If the return object's
            --  declaration includes an expression that has not already been
            --  expanded as separate assignments, then add an assignment
            --  statement to ensure the return object gets initialized.

            --  declare
            --     Result : T [:= <expression>];
            --  begin
            --     ...

            --  is converted to

            --  declare
            --     Result : T renames FuncRA.all;
            --     [Result := <expression;]
            --  begin
            --     ...

            declare
               Return_Obj_Id    : constant Entity_Id :=
                                    Defining_Identifier (Return_Object_Decl);
               Return_Obj_Typ   : constant Entity_Id := Etype (Return_Obj_Id);
               Return_Obj_Expr  : constant Node_Id :=
                                    Expression (Return_Object_Decl);
               Result_Subt      : constant Entity_Id :=
                                    Etype (Parent_Function);
               Constr_Result    : constant Boolean :=
                                    Is_Constrained (Result_Subt);
               Obj_Alloc_Formal : Entity_Id;
               Object_Access    : Entity_Id;
               Obj_Acc_Deref    : Node_Id;
               Init_Assignment  : Node_Id := Empty;

            begin
               --  Build-in-place results must be returned by reference

               Set_By_Ref (Return_Stm);

               --  Retrieve the implicit access parameter passed by the caller

               Object_Access :=
                 Build_In_Place_Formal (Parent_Function, BIP_Object_Access);

               --  If the return object's declaration includes an expression
               --  and the declaration isn't marked as No_Initialization, then
               --  we need to generate an assignment to the object and insert
               --  it after the declaration before rewriting it as a renaming
               --  (otherwise we'll lose the initialization). The case where
               --  the result type is an interface (or class-wide interface)
               --  is also excluded because the context of the function call
               --  must be unconstrained, so the initialization will always
               --  be done as part of an allocator evaluation (storage pool
               --  or secondary stack), never to a constrained target object
               --  passed in by the caller. Besides the assignment being
               --  unneeded in this case, it avoids problems with trying to
               --  generate a dispatching assignment when the return expression
               --  is a nonlimited descendant of a limited interface (the
               --  interface has no assignment operation).

               if Present (Return_Obj_Expr)
                 and then not No_Initialization (Return_Object_Decl)
                 and then not Is_Interface (Return_Obj_Typ)
               then
                  Init_Assignment :=
                    Make_Assignment_Statement (Loc,
                      Name       => New_Reference_To (Return_Obj_Id, Loc),
                      Expression => Relocate_Node (Return_Obj_Expr));
                  Set_Etype (Name (Init_Assignment), Etype (Return_Obj_Id));
                  Set_Assignment_OK (Name (Init_Assignment));
                  Set_No_Ctrl_Actions (Init_Assignment);

                  Set_Parent (Name (Init_Assignment), Init_Assignment);
                  Set_Parent (Expression (Init_Assignment), Init_Assignment);

                  Set_Expression (Return_Object_Decl, Empty);

                  if Is_Class_Wide_Type (Etype (Return_Obj_Id))
                    and then not Is_Class_Wide_Type
                                   (Etype (Expression (Init_Assignment)))
                  then
                     Rewrite (Expression (Init_Assignment),
                       Make_Type_Conversion (Loc,
                         Subtype_Mark =>
                           New_Occurrence_Of
                             (Etype (Return_Obj_Id), Loc),
                         Expression =>
                           Relocate_Node (Expression (Init_Assignment))));
                  end if;

                  --  In the case of functions where the calling context can
                  --  determine the form of allocation needed, initialization
                  --  is done with each part of the if statement that handles
                  --  the different forms of allocation (this is true for
                  --  unconstrained and tagged result subtypes).

                  if Constr_Result
                    and then not Is_Tagged_Type (Underlying_Type (Result_Subt))
                  then
                     Insert_After (Return_Object_Decl, Init_Assignment);
                  end if;
               end if;

               --  When the function's subtype is unconstrained, a run-time
               --  test is needed to determine the form of allocation to use
               --  for the return object. The function has an implicit formal
               --  parameter indicating this. If the BIP_Alloc_Form formal has
               --  the value one, then the caller has passed access to an
               --  existing object for use as the return object. If the value
               --  is two, then the return object must be allocated on the
               --  secondary stack. Otherwise, the object must be allocated in
               --  a storage pool (currently only supported for the global
               --  heap, user-defined storage pools TBD ???). We generate an
               --  if statement to test the implicit allocation formal and
               --  initialize a local access value appropriately, creating
               --  allocators in the secondary stack and global heap cases.
               --  The special formal also exists and must be tested when the
               --  function has a tagged result, even when the result subtype
               --  is constrained, because in general such functions can be
               --  called in dispatching contexts and must be handled similarly
               --  to functions with a class-wide result.

               if not Constr_Result
                 or else Is_Tagged_Type (Underlying_Type (Result_Subt))
               then
                  Obj_Alloc_Formal :=
                    Build_In_Place_Formal (Parent_Function, BIP_Alloc_Form);

                  declare
                     Ref_Type       : Entity_Id;
                     Ptr_Type_Decl  : Node_Id;
                     Alloc_Obj_Id   : Entity_Id;
                     Alloc_Obj_Decl : Node_Id;
                     Alloc_If_Stmt  : Node_Id;
                     SS_Allocator   : Node_Id;
                     Heap_Allocator : Node_Id;

                  begin
                     --  Reuse the itype created for the function's implicit
                     --  access formal. This avoids the need to create a new
                     --  access type here, plus it allows assigning the access
                     --  formal directly without applying a conversion.

                     --  Ref_Type := Etype (Object_Access);

                     --  Create an access type designating the function's
                     --  result subtype.

                     Ref_Type := Make_Temporary (Loc, 'A');

                     Ptr_Type_Decl :=
                       Make_Full_Type_Declaration (Loc,
                         Defining_Identifier => Ref_Type,
                         Type_Definition =>
                           Make_Access_To_Object_Definition (Loc,
                             All_Present => True,
                             Subtype_Indication =>
                               New_Reference_To (Return_Obj_Typ, Loc)));

                     Insert_Before (Return_Object_Decl, Ptr_Type_Decl);

                     --  Create an access object that will be initialized to an
                     --  access value denoting the return object, either coming
                     --  from an implicit access value passed in by the caller
                     --  or from the result of an allocator.

                     Alloc_Obj_Id := Make_Temporary (Loc, 'R');
                     Set_Etype (Alloc_Obj_Id, Ref_Type);

                     Alloc_Obj_Decl :=
                       Make_Object_Declaration (Loc,
                         Defining_Identifier => Alloc_Obj_Id,
                         Object_Definition   => New_Reference_To
                                                  (Ref_Type, Loc));

                     Insert_Before (Return_Object_Decl, Alloc_Obj_Decl);

                     --  Create allocators for both the secondary stack and
                     --  global heap. If there's an initialization expression,
                     --  then create these as initialized allocators.

                     if Present (Return_Obj_Expr)
                       and then not No_Initialization (Return_Object_Decl)
                     then
                        --  Always use the type of the expression for the
                        --  qualified expression, rather than the result type.
                        --  In general we cannot always use the result type
                        --  for the allocator, because the expression might be
                        --  of a specific type, such as in the case of an
                        --  aggregate or even a nonlimited object when the
                        --  result type is a limited class-wide interface type.

                        Heap_Allocator :=
                          Make_Allocator (Loc,
                            Expression =>
                              Make_Qualified_Expression (Loc,
                                Subtype_Mark =>
                                  New_Reference_To
                                    (Etype (Return_Obj_Expr), Loc),
                                Expression =>
                                  New_Copy_Tree (Return_Obj_Expr)));

                     else
                        --  If the function returns a class-wide type we cannot
                        --  use the return type for the allocator. Instead we
                        --  use the type of the expression, which must be an
                        --  aggregate of a definite type.

                        if Is_Class_Wide_Type (Return_Obj_Typ) then
                           Heap_Allocator :=
                             Make_Allocator (Loc,
                               Expression =>
                                 New_Reference_To
                                   (Etype (Return_Obj_Expr), Loc));
                        else
                           Heap_Allocator :=
                             Make_Allocator (Loc,
                               Expression =>
                                 New_Reference_To (Return_Obj_Typ, Loc));
                        end if;

                        --  If the object requires default initialization then
                        --  that will happen later following the elaboration of
                        --  the object renaming. If we don't turn it off here
                        --  then the object will be default initialized twice.

                        Set_No_Initialization (Heap_Allocator);
                     end if;

                     --  If the No_Allocators restriction is active, then only
                     --  an allocator for secondary stack allocation is needed.
                     --  It's OK for such allocators to have Comes_From_Source
                     --  set to False, because gigi knows not to flag them as
                     --  being a violation of No_Implicit_Heap_Allocations.

                     if Restriction_Active (No_Allocators) then
                        SS_Allocator   := Heap_Allocator;
                        Heap_Allocator := Make_Null (Loc);

                     --  Otherwise the heap allocator may be needed, so we make
                     --  another allocator for secondary stack allocation.

                     else
                        SS_Allocator := New_Copy_Tree (Heap_Allocator);

                        --  The heap allocator is marked Comes_From_Source
                        --  since it corresponds to an explicit user-written
                        --  allocator (that is, it will only be executed on
                        --  behalf of callers that call the function as
                        --  initialization for such an allocator). This
                        --  prevents errors when No_Implicit_Heap_Allocations
                        --  is in force.

                        Set_Comes_From_Source (Heap_Allocator, True);
                     end if;

                     --  The allocator is returned on the secondary stack. We
                     --  don't do this on VM targets, since the SS is not used.

                     if VM_Target = No_VM then
                        Set_Storage_Pool (SS_Allocator, RTE (RE_SS_Pool));
                        Set_Procedure_To_Call
                          (SS_Allocator, RTE (RE_SS_Allocate));

                        --  The allocator is returned on the secondary stack,
                        --  so indicate that the function return, as well as
                        --  the block that encloses the allocator, must not
                        --  release it. The flags must be set now because the
                        --  decision to use the secondary stack is done very
                        --  late in the course of expanding the return
                        --  statement, past the point where these flags are
                        --  normally set.

                        Set_Sec_Stack_Needed_For_Return (Parent_Function);
                        Set_Sec_Stack_Needed_For_Return
                          (Return_Statement_Entity (N));
                        Set_Uses_Sec_Stack (Parent_Function);
                        Set_Uses_Sec_Stack (Return_Statement_Entity (N));
                     end if;

                     --  Create an if statement to test the BIP_Alloc_Form
                     --  formal and initialize the access object to either the
                     --  BIP_Object_Access formal (BIP_Alloc_Form = 0), the
                     --  result of allocating the object in the secondary stack
                     --  (BIP_Alloc_Form = 1), or else an allocator to create
                     --  the return object in the heap (BIP_Alloc_Form = 2).

                     --  ??? An unchecked type conversion must be made in the
                     --  case of assigning the access object formal to the
                     --  local access object, because a normal conversion would
                     --  be illegal in some cases (such as converting access-
                     --  to-unconstrained to access-to-constrained), but the
                     --  the unchecked conversion will presumably fail to work
                     --  right in just such cases. It's not clear at all how to
                     --  handle this. ???

                     Alloc_If_Stmt :=
                       Make_If_Statement (Loc,
                         Condition       =>
                           Make_Op_Eq (Loc,
                             Left_Opnd =>
                               New_Reference_To (Obj_Alloc_Formal, Loc),
                             Right_Opnd =>
                               Make_Integer_Literal (Loc,
                                 UI_From_Int (BIP_Allocation_Form'Pos
                                                (Caller_Allocation)))),
                         Then_Statements =>
                           New_List (Make_Assignment_Statement (Loc,
                                       Name       =>
                                         New_Reference_To
                                           (Alloc_Obj_Id, Loc),
                                       Expression =>
                                         Make_Unchecked_Type_Conversion (Loc,
                                           Subtype_Mark =>
                                             New_Reference_To (Ref_Type, Loc),
                                           Expression =>
                                             New_Reference_To
                                               (Object_Access, Loc)))),
                         Elsif_Parts     =>
                           New_List (Make_Elsif_Part (Loc,
                                       Condition       =>
                                         Make_Op_Eq (Loc,
                                           Left_Opnd =>
                                             New_Reference_To
                                               (Obj_Alloc_Formal, Loc),
                                           Right_Opnd =>
                                             Make_Integer_Literal (Loc,
                                               UI_From_Int (
                                                 BIP_Allocation_Form'Pos
                                                    (Secondary_Stack)))),
                                       Then_Statements =>
                                          New_List
                                            (Make_Assignment_Statement (Loc,
                                               Name       =>
                                                 New_Reference_To
                                                   (Alloc_Obj_Id, Loc),
                                               Expression =>
                                                 SS_Allocator)))),
                         Else_Statements =>
                           New_List (Make_Assignment_Statement (Loc,
                                        Name       =>
                                          New_Reference_To
                                            (Alloc_Obj_Id, Loc),
                                        Expression =>
                                          Heap_Allocator)));

                     --  If a separate initialization assignment was created
                     --  earlier, append that following the assignment of the
                     --  implicit access formal to the access object, to ensure
                     --  that the return object is initialized in that case.
                     --  In this situation, the target of the assignment must
                     --  be rewritten to denote a dereference of the access to
                     --  the return object passed in by the caller.

                     if Present (Init_Assignment) then
                        Rewrite (Name (Init_Assignment),
                          Make_Explicit_Dereference (Loc,
                            Prefix => New_Reference_To (Alloc_Obj_Id, Loc)));
                        Set_Etype
                          (Name (Init_Assignment), Etype (Return_Obj_Id));

                        Append_To
                          (Then_Statements (Alloc_If_Stmt),
                           Init_Assignment);
                     end if;

                     Insert_Before (Return_Object_Decl, Alloc_If_Stmt);

                     --  Remember the local access object for use in the
                     --  dereference of the renaming created below.

                     Object_Access := Alloc_Obj_Id;
                  end;
               end if;

               --  Replace the return object declaration with a renaming of a
               --  dereference of the access value designating the return
               --  object.

               Obj_Acc_Deref :=
                 Make_Explicit_Dereference (Loc,
                   Prefix => New_Reference_To (Object_Access, Loc));

               Rewrite (Return_Object_Decl,
                 Make_Object_Renaming_Declaration (Loc,
                   Defining_Identifier => Return_Obj_Id,
                   Access_Definition   => Empty,
                   Subtype_Mark        => New_Occurrence_Of
                                            (Return_Obj_Typ, Loc),
                   Name                => Obj_Acc_Deref));

               Set_Renamed_Object (Return_Obj_Id, Obj_Acc_Deref);
            end;
         end if;

      --  Case where we do not build a block

      else
         --  We're about to drop Return_Object_Declarations on the floor, so
         --  we need to insert it, in case it got expanded into useful code.
         --  Remove side effects from expression, which may be duplicated in
         --  subsequent checks (see Expand_Simple_Function_Return).

         Insert_List_Before (N, Return_Object_Declarations (N));
         Remove_Side_Effects (Exp);

         --  Build simple_return_statement that returns the expression directly

         Return_Stm := Make_Simple_Return_Statement (Loc, Expression => Exp);

         Result := Return_Stm;
      end if;

      --  Set the flag to prevent infinite recursion

      Set_Comes_From_Extended_Return_Statement (Return_Stm);

      Rewrite (N, Result);
      Analyze (N);
   end Expand_N_Extended_Return_Statement;

   ----------------------------
   -- Expand_N_Function_Call --
   ----------------------------

   procedure Expand_N_Function_Call (N : Node_Id) is
   begin
      Expand_Call (N);

      --  If the return value of a foreign compiled function is VAX Float, then
      --  expand the return (adjusts the location of the return value on
      --  Alpha/VMS, no-op everywhere else).
      --  Comes_From_Source intercepts recursive expansion.

      if Vax_Float (Etype (N))
        and then Nkind (N) = N_Function_Call
        and then Present (Name (N))
        and then Present (Entity (Name (N)))
        and then Has_Foreign_Convention (Entity (Name (N)))
        and then Comes_From_Source (Parent (N))
      then
         Expand_Vax_Foreign_Return (N);
      end if;
   end Expand_N_Function_Call;

   ---------------------------------------
   -- Expand_N_Procedure_Call_Statement --
   ---------------------------------------

   procedure Expand_N_Procedure_Call_Statement (N : Node_Id) is
   begin
      Expand_Call (N);
   end Expand_N_Procedure_Call_Statement;

   --------------------------------------
   -- Expand_N_Simple_Return_Statement --
   --------------------------------------

   procedure Expand_N_Simple_Return_Statement (N : Node_Id) is
   begin
      --  Defend against previous errors (i.e. the return statement calls a
      --  function that is not available in configurable runtime).

      if Present (Expression (N))
        and then Nkind (Expression (N)) = N_Empty
      then
         return;
      end if;

      --  Distinguish the function and non-function cases:

      case Ekind (Return_Applies_To (Return_Statement_Entity (N))) is

         when E_Function          |
              E_Generic_Function  =>
            Expand_Simple_Function_Return (N);

         when E_Procedure         |
              E_Generic_Procedure |
              E_Entry             |
              E_Entry_Family      |
              E_Return_Statement =>
            Expand_Non_Function_Return (N);

         when others =>
            raise Program_Error;
      end case;

   exception
      when RE_Not_Available =>
         return;
   end Expand_N_Simple_Return_Statement;

   ------------------------------
   -- Expand_N_Subprogram_Body --
   ------------------------------

   --  Add poll call if ATC polling is enabled, unless the body will be inlined
   --  by the back-end.

   --  Add dummy push/pop label nodes at start and end to clear any local
   --  exception indications if local-exception-to-goto optimization is active.

   --  Add return statement if last statement in body is not a return statement
   --  (this makes things easier on Gigi which does not want to have to handle
   --  a missing return).

   --  Add call to Activate_Tasks if body is a task activator

   --  Deal with possible detection of infinite recursion

   --  Eliminate body completely if convention stubbed

   --  Encode entity names within body, since we will not need to reference
   --  these entities any longer in the front end.

   --  Initialize scalar out parameters if Initialize/Normalize_Scalars

   --  Reset Pure indication if any parameter has root type System.Address
   --  or has any parameters of limited types, where limited means that the
   --  run-time view is limited (i.e. the full type is limited).

   --  Wrap thread body

   procedure Expand_N_Subprogram_Body (N : Node_Id) is
      Loc      : constant Source_Ptr := Sloc (N);
      H        : constant Node_Id    := Handled_Statement_Sequence (N);
      Body_Id  : Entity_Id;
      Except_H : Node_Id;
      L        : List_Id;
      Spec_Id  : Entity_Id;

      procedure Add_Return (S : List_Id);
      --  Append a return statement to the statement sequence S if the last
      --  statement is not already a return or a goto statement. Note that
      --  the latter test is not critical, it does not matter if we add a few
      --  extra returns, since they get eliminated anyway later on.

      ----------------
      -- Add_Return --
      ----------------

      procedure Add_Return (S : List_Id) is
         Last_Stm : Node_Id;
         Loc      : Source_Ptr;

      begin
         --  Get last statement, ignoring any Pop_xxx_Label nodes, which are
         --  not relevant in this context since they are not executable.

         Last_Stm := Last (S);
         while Nkind (Last_Stm) in N_Pop_xxx_Label loop
            Prev (Last_Stm);
         end loop;

         --  Now insert return unless last statement is a transfer

         if not Is_Transfer (Last_Stm) then

            --  The source location for the return is the end label of the
            --  procedure if present. Otherwise use the sloc of the last
            --  statement in the list. If the list comes from a generated
            --  exception handler and we are not debugging generated code,
            --  all the statements within the handler are made invisible
            --  to the debugger.

            if Nkind (Parent (S)) = N_Exception_Handler
              and then not Comes_From_Source (Parent (S))
            then
               Loc := Sloc (Last_Stm);

            elsif Present (End_Label (H)) then
               Loc := Sloc (End_Label (H));

            else
               Loc := Sloc (Last_Stm);
            end if;

            declare
               Rtn : constant Node_Id := Make_Simple_Return_Statement (Loc);

            begin
               --  Append return statement, and set analyzed manually. We can't
               --  call Analyze on this return since the scope is wrong.

               --  Note: it almost works to push the scope and then do the
               --  Analyze call, but something goes wrong in some weird cases
               --  and it is not worth worrying about ???

               Append_To (S, Rtn);
               Set_Analyzed (Rtn);

               --  Call _Postconditions procedure if appropriate. We need to
               --  do this explicitly because we did not analyze the generated
               --  return statement above, so the call did not get inserted.

               if Ekind (Spec_Id) = E_Procedure
                 and then Has_Postconditions (Spec_Id)
               then
                  pragma Assert (Present (Postcondition_Proc (Spec_Id)));
                  Insert_Action (Rtn,
                    Make_Procedure_Call_Statement (Loc,
                      Name =>
                        New_Reference_To (Postcondition_Proc (Spec_Id), Loc)));
               end if;
            end;
         end if;
      end Add_Return;

   --  Start of processing for Expand_N_Subprogram_Body

   begin
      --  Set L to either the list of declarations if present, or to the list
      --  of statements if no declarations are present. This is used to insert
      --  new stuff at the start.

      if Is_Non_Empty_List (Declarations (N)) then
         L := Declarations (N);
      else
         L := Statements (H);
      end if;

      --  If local-exception-to-goto optimization active, insert dummy push
      --  statements at start, and dummy pop statements at end.

      if (Debug_Flag_Dot_G
           or else Restriction_Active (No_Exception_Propagation))
        and then Is_Non_Empty_List (L)
      then
         declare
            FS  : constant Node_Id    := First (L);
            FL  : constant Source_Ptr := Sloc (FS);
            LS  : Node_Id;
            LL  : Source_Ptr;

         begin
            --  LS points to either last statement, if statements are present
            --  or to the last declaration if there are no statements present.
            --  It is the node after which the pop's are generated.

            if Is_Non_Empty_List (Statements (H)) then
               LS := Last (Statements (H));
            else
               LS := Last (L);
            end if;

            LL := Sloc (LS);

            Insert_List_Before_And_Analyze (FS, New_List (
              Make_Push_Constraint_Error_Label (FL),
              Make_Push_Program_Error_Label    (FL),
              Make_Push_Storage_Error_Label    (FL)));

            Insert_List_After_And_Analyze (LS, New_List (
              Make_Pop_Constraint_Error_Label  (LL),
              Make_Pop_Program_Error_Label     (LL),
              Make_Pop_Storage_Error_Label     (LL)));
         end;
      end if;

      --  Find entity for subprogram

      Body_Id := Defining_Entity (N);

      if Present (Corresponding_Spec (N)) then
         Spec_Id := Corresponding_Spec (N);
      else
         Spec_Id := Body_Id;
      end if;

      --  Need poll on entry to subprogram if polling enabled. We only do this
      --  for non-empty subprograms, since it does not seem necessary to poll
      --  for a dummy null subprogram.

      if Is_Non_Empty_List (L) then

         --  Do not add a polling call if the subprogram is to be inlined by
         --  the back-end, to avoid repeated calls with multiple inlinings.

         if Is_Inlined (Spec_Id)
           and then Front_End_Inlining
           and then Optimization_Level > 1
         then
            null;
         else
            Generate_Poll_Call (First (L));
         end if;
      end if;

      --  If this is a Pure function which has any parameters whose root type
      --  is System.Address, reset the Pure indication, since it will likely
      --  cause incorrect code to be generated as the parameter is probably
      --  a pointer, and the fact that the same pointer is passed does not mean
      --  that the same value is being referenced.

      --  Note that if the programmer gave an explicit Pure_Function pragma,
      --  then we believe the programmer, and leave the subprogram Pure.

      --  This code should probably be at the freeze point, so that it happens
      --  even on a -gnatc (or more importantly -gnatt) compile, so that the
      --  semantic tree has Is_Pure set properly ???

      if Is_Pure (Spec_Id)
        and then Is_Subprogram (Spec_Id)
        and then not Has_Pragma_Pure_Function (Spec_Id)
      then
         declare
            F : Entity_Id;

         begin
            F := First_Formal (Spec_Id);
            while Present (F) loop
               if Is_Descendent_Of_Address (Etype (F))

                 --  Note that this test is being made in the body of the
                 --  subprogram, not the spec, so we are testing the full
                 --  type for being limited here, as required.

                 or else Is_Limited_Type (Etype (F))
               then
                  Set_Is_Pure (Spec_Id, False);

                  if Spec_Id /= Body_Id then
                     Set_Is_Pure (Body_Id, False);
                  end if;

                  exit;
               end if;

               Next_Formal (F);
            end loop;
         end;
      end if;

      --  Initialize any scalar OUT args if Initialize/Normalize_Scalars

      if Init_Or_Norm_Scalars and then Is_Subprogram (Spec_Id) then
         declare
            F : Entity_Id;

         begin
            --  Loop through formals

            F := First_Formal (Spec_Id);
            while Present (F) loop
               if Is_Scalar_Type (Etype (F))
                 and then Ekind (F) = E_Out_Parameter
               then
                  Check_Restriction (No_Default_Initialization, F);

                  --  Insert the initialization. We turn off validity checks
                  --  for this assignment, since we do not want any check on
                  --  the initial value itself (which may well be invalid).

                  Insert_Before_And_Analyze (First (L),
                    Make_Assignment_Statement (Loc,
                      Name       => New_Occurrence_Of (F, Loc),
                      Expression => Get_Simple_Init_Val (Etype (F), N)),
                    Suppress => Validity_Check);
               end if;

               Next_Formal (F);
            end loop;
         end;
      end if;

      --  Clear out statement list for stubbed procedure

      if Present (Corresponding_Spec (N)) then
         Set_Elaboration_Flag (N, Spec_Id);

         if Convention (Spec_Id) = Convention_Stubbed
           or else Is_Eliminated (Spec_Id)
         then
            Set_Declarations (N, Empty_List);
            Set_Handled_Statement_Sequence (N,
              Make_Handled_Sequence_Of_Statements (Loc,
                Statements => New_List (
                  Make_Null_Statement (Loc))));
            return;
         end if;
      end if;

      --  Create a set of discriminals for the next protected subprogram body

      if Is_List_Member (N)
        and then Present (Parent (List_Containing (N)))
        and then Nkind (Parent (List_Containing (N))) = N_Protected_Body
        and then Present (Next_Protected_Operation (N))
      then
         Set_Discriminals (Parent (Base_Type (Scope (Spec_Id))));
      end if;

      --  Returns_By_Ref flag is normally set when the subprogram is frozen but
      --  subprograms with no specs are not frozen.

      declare
         Typ  : constant Entity_Id := Etype (Spec_Id);
         Utyp : constant Entity_Id := Underlying_Type (Typ);

      begin
         if not Acts_As_Spec (N)
           and then Nkind (Parent (Parent (Spec_Id))) /=
             N_Subprogram_Body_Stub
         then
            null;

         elsif Is_Immutably_Limited_Type (Typ) then
            Set_Returns_By_Ref (Spec_Id);

         elsif Present (Utyp) and then CW_Or_Has_Controlled_Part (Utyp) then
            Set_Returns_By_Ref (Spec_Id);
         end if;
      end;

      --  For a procedure, we add a return for all possible syntactic ends of
      --  the subprogram.

      if Ekind_In (Spec_Id, E_Procedure, E_Generic_Procedure) then
         Add_Return (Statements (H));

         if Present (Exception_Handlers (H)) then
            Except_H := First_Non_Pragma (Exception_Handlers (H));
            while Present (Except_H) loop
               Add_Return (Statements (Except_H));
               Next_Non_Pragma (Except_H);
            end loop;
         end if;

      --  For a function, we must deal with the case where there is at least
      --  one missing return. What we do is to wrap the entire body of the
      --  function in a block:

      --    begin
      --      ...
      --    end;

      --  becomes

      --    begin
      --       begin
      --          ...
      --       end;

      --       raise Program_Error;
      --    end;

      --  This approach is necessary because the raise must be signalled to the
      --  caller, not handled by any local handler (RM 6.4(11)).

      --  Note: we do not need to analyze the constructed sequence here, since
      --  it has no handler, and an attempt to analyze the handled statement
      --  sequence twice is risky in various ways (e.g. the issue of expanding
      --  cleanup actions twice).

      elsif Has_Missing_Return (Spec_Id) then
         declare
            Hloc : constant Source_Ptr := Sloc (H);
            Blok : constant Node_Id    :=
                     Make_Block_Statement (Hloc,
                       Handled_Statement_Sequence => H);
            Rais : constant Node_Id    :=
                     Make_Raise_Program_Error (Hloc,
                       Reason => PE_Missing_Return);

         begin
            Set_Handled_Statement_Sequence (N,
              Make_Handled_Sequence_Of_Statements (Hloc,
                Statements => New_List (Blok, Rais)));

            Push_Scope (Spec_Id);
            Analyze (Blok);
            Analyze (Rais);
            Pop_Scope;
         end;
      end if;

      --  If subprogram contains a parameterless recursive call, then we may
      --  have an infinite recursion, so see if we can generate code to check
      --  for this possibility if storage checks are not suppressed.

      if Ekind (Spec_Id) = E_Procedure
        and then Has_Recursive_Call (Spec_Id)
        and then not Storage_Checks_Suppressed (Spec_Id)
      then
         Detect_Infinite_Recursion (N, Spec_Id);
      end if;

      --  Set to encode entity names in package body before gigi is called

      Qualify_Entity_Names (N);
   end Expand_N_Subprogram_Body;

   -----------------------------------
   -- Expand_N_Subprogram_Body_Stub --
   -----------------------------------

   procedure Expand_N_Subprogram_Body_Stub (N : Node_Id) is
   begin
      if Present (Corresponding_Body (N)) then
         Expand_N_Subprogram_Body (
           Unit_Declaration_Node (Corresponding_Body (N)));
      end if;
   end Expand_N_Subprogram_Body_Stub;

   -------------------------------------
   -- Expand_N_Subprogram_Declaration --
   -------------------------------------

   --  If the declaration appears within a protected body, it is a private
   --  operation of the protected type. We must create the corresponding
   --  protected subprogram an associated formals. For a normal protected
   --  operation, this is done when expanding the protected type declaration.

   --  If the declaration is for a null procedure, emit null body

   procedure Expand_N_Subprogram_Declaration (N : Node_Id) is
      Loc       : constant Source_Ptr := Sloc (N);
      Subp      : constant Entity_Id  := Defining_Entity (N);
      Scop      : constant Entity_Id  := Scope (Subp);
      Prot_Decl : Node_Id;
      Prot_Bod  : Node_Id;
      Prot_Id   : Entity_Id;

   begin
      --  Deal with case of protected subprogram. Do not generate protected
      --  operation if operation is flagged as eliminated.

      if Is_List_Member (N)
        and then Present (Parent (List_Containing (N)))
        and then Nkind (Parent (List_Containing (N))) = N_Protected_Body
        and then Is_Protected_Type (Scop)
      then
         if No (Protected_Body_Subprogram (Subp))
           and then not Is_Eliminated (Subp)
         then
            Prot_Decl :=
              Make_Subprogram_Declaration (Loc,
                Specification =>
                  Build_Protected_Sub_Specification
                    (N, Scop, Unprotected_Mode));

            --  The protected subprogram is declared outside of the protected
            --  body. Given that the body has frozen all entities so far, we
            --  analyze the subprogram and perform freezing actions explicitly.
            --  including the generation of an explicit freeze node, to ensure
            --  that gigi has the proper order of elaboration.
            --  If the body is a subunit, the insertion point is before the
            --  stub in the parent.

            Prot_Bod := Parent (List_Containing (N));

            if Nkind (Parent (Prot_Bod)) = N_Subunit then
               Prot_Bod := Corresponding_Stub (Parent (Prot_Bod));
            end if;

            Insert_Before (Prot_Bod, Prot_Decl);
            Prot_Id := Defining_Unit_Name (Specification (Prot_Decl));
            Set_Has_Delayed_Freeze (Prot_Id);

            Push_Scope (Scope (Scop));
            Analyze (Prot_Decl);
            Freeze_Before (N, Prot_Id);
            Set_Protected_Body_Subprogram (Subp, Prot_Id);

            --  Create protected operation as well. Even though the operation
            --  is only accessible within the body, it is possible to make it
            --  available outside of the protected object by using 'Access to
            --  provide a callback, so build protected version in all cases.

            Prot_Decl :=
              Make_Subprogram_Declaration (Loc,
                Specification =>
                  Build_Protected_Sub_Specification (N, Scop, Protected_Mode));
            Insert_Before (Prot_Bod, Prot_Decl);
            Analyze (Prot_Decl);

            Pop_Scope;
         end if;

      --  Ada 2005 (AI-348): Generate body for a null procedure.
      --  In most cases this is superfluous because calls to it
      --  will be automatically inlined, but we definitely need
      --  the body if preconditions for the procedure are present.

      elsif Nkind (Specification (N)) = N_Procedure_Specification
        and then Null_Present (Specification (N))
      then
         declare
            Bod : constant Node_Id := Body_To_Inline (N);

         begin
            Set_Has_Completion (Subp, False);
            Append_Freeze_Action (Subp, Bod);

            --  The body now contains raise statements, so calls to it will
            --  not be inlined.

            Set_Is_Inlined (Subp, False);
         end;
      end if;
   end Expand_N_Subprogram_Declaration;

   --------------------------------
   -- Expand_Non_Function_Return --
   --------------------------------

   procedure Expand_Non_Function_Return (N : Node_Id) is
      pragma Assert (No (Expression (N)));

      Loc         : constant Source_Ptr := Sloc (N);
      Scope_Id    : Entity_Id :=
                      Return_Applies_To (Return_Statement_Entity (N));
      Kind        : constant Entity_Kind := Ekind (Scope_Id);
      Call        : Node_Id;
      Acc_Stat    : Node_Id;
      Goto_Stat   : Node_Id;
      Lab_Node    : Node_Id;

   begin
      --  Call _Postconditions procedure if procedure with active
      --  postconditions. Here, we use the Postcondition_Proc attribute, which
      --  is needed for implicitly-generated returns. Functions never
      --  have implicitly-generated returns, and there's no room for
      --  Postcondition_Proc in E_Function, so we look up the identifier
      --  Name_uPostconditions for function returns (see
      --  Expand_Simple_Function_Return).

      if Ekind (Scope_Id) = E_Procedure
        and then Has_Postconditions (Scope_Id)
      then
         pragma Assert (Present (Postcondition_Proc (Scope_Id)));
         Insert_Action (N,
           Make_Procedure_Call_Statement (Loc,
             Name => New_Reference_To (Postcondition_Proc (Scope_Id), Loc)));
      end if;

      --  If it is a return from a procedure do no extra steps

      if Kind = E_Procedure or else Kind = E_Generic_Procedure then
         return;

      --  If it is a nested return within an extended one, replace it with a
      --  return of the previously declared return object.

      elsif Kind = E_Return_Statement then
         Rewrite (N,
           Make_Simple_Return_Statement (Loc,
             Expression =>
               New_Occurrence_Of (First_Entity (Scope_Id), Loc)));
         Set_Comes_From_Extended_Return_Statement (N);
         Set_Return_Statement_Entity (N, Scope_Id);
         Expand_Simple_Function_Return (N);
         return;
      end if;

      pragma Assert (Is_Entry (Scope_Id));

      --  Look at the enclosing block to see whether the return is from an
      --  accept statement or an entry body.

      for J in reverse 0 .. Scope_Stack.Last loop
         Scope_Id := Scope_Stack.Table (J).Entity;
         exit when Is_Concurrent_Type (Scope_Id);
      end loop;

      --  If it is a return from accept statement it is expanded as call to
      --  RTS Complete_Rendezvous and a goto to the end of the accept body.

      --  (cf : Expand_N_Accept_Statement, Expand_N_Selective_Accept,
      --  Expand_N_Accept_Alternative in exp_ch9.adb)

      if Is_Task_Type (Scope_Id) then

         Call :=
           Make_Procedure_Call_Statement (Loc,
             Name => New_Reference_To (RTE (RE_Complete_Rendezvous), Loc));
         Insert_Before (N, Call);
         --  why not insert actions here???
         Analyze (Call);

         Acc_Stat := Parent (N);
         while Nkind (Acc_Stat) /= N_Accept_Statement loop
            Acc_Stat := Parent (Acc_Stat);
         end loop;

         Lab_Node := Last (Statements
           (Handled_Statement_Sequence (Acc_Stat)));

         Goto_Stat := Make_Goto_Statement (Loc,
           Name => New_Occurrence_Of
             (Entity (Identifier (Lab_Node)), Loc));

         Set_Analyzed (Goto_Stat);

         Rewrite (N, Goto_Stat);
         Analyze (N);

      --  If it is a return from an entry body, put a Complete_Entry_Body call
      --  in front of the return.

      elsif Is_Protected_Type (Scope_Id) then
         Call :=
           Make_Procedure_Call_Statement (Loc,
             Name =>
               New_Reference_To (RTE (RE_Complete_Entry_Body), Loc),
             Parameter_Associations => New_List (
               Make_Attribute_Reference (Loc,
                 Prefix =>
                   New_Reference_To
                     (Find_Protection_Object (Current_Scope), Loc),
                 Attribute_Name =>
                   Name_Unchecked_Access)));

         Insert_Before (N, Call);
         Analyze (Call);
      end if;
   end Expand_Non_Function_Return;

   ---------------------------------------
   -- Expand_Protected_Object_Reference --
   ---------------------------------------

   function Expand_Protected_Object_Reference
     (N    : Node_Id;
      Scop : Entity_Id) return Node_Id
   is
      Loc   : constant Source_Ptr := Sloc (N);
      Corr  : Entity_Id;
      Rec   : Node_Id;
      Param : Entity_Id;
      Proc  : Entity_Id;

   begin
      Rec := Make_Identifier (Loc, Name_uObject);
      Set_Etype (Rec, Corresponding_Record_Type (Scop));

      --  Find enclosing protected operation, and retrieve its first parameter,
      --  which denotes the enclosing protected object. If the enclosing
      --  operation is an entry, we are immediately within the protected body,
      --  and we can retrieve the object from the service entries procedure. A
      --  barrier function has the same signature as an entry. A barrier
      --  function is compiled within the protected object, but unlike
      --  protected operations its never needs locks, so that its protected
      --  body subprogram points to itself.

      Proc := Current_Scope;
      while Present (Proc)
        and then Scope (Proc) /= Scop
      loop
         Proc := Scope (Proc);
      end loop;

      Corr := Protected_Body_Subprogram (Proc);

      if No (Corr) then

         --  Previous error left expansion incomplete.
         --  Nothing to do on this call.

         return Empty;
      end if;

      Param :=
        Defining_Identifier
          (First (Parameter_Specifications (Parent (Corr))));

      if Is_Subprogram (Proc)
        and then Proc /= Corr
      then
         --  Protected function or procedure

         Set_Entity (Rec, Param);

         --  Rec is a reference to an entity which will not be in scope when
         --  the call is reanalyzed, and needs no further analysis.

         Set_Analyzed (Rec);

      else
         --  Entry or barrier function for entry body. The first parameter of
         --  the entry body procedure is pointer to the object. We create a
         --  local variable of the proper type, duplicating what is done to
         --  define _object later on.

         declare
            Decls   : List_Id;
            Obj_Ptr : constant Entity_Id :=  Make_Temporary (Loc, 'T');

         begin
            Decls := New_List (
              Make_Full_Type_Declaration (Loc,
                Defining_Identifier => Obj_Ptr,
                  Type_Definition =>
                     Make_Access_To_Object_Definition (Loc,
                       Subtype_Indication =>
                         New_Reference_To
                           (Corresponding_Record_Type (Scop), Loc))));

            Insert_Actions (N, Decls);
            Freeze_Before (N, Obj_Ptr);

            Rec :=
              Make_Explicit_Dereference (Loc,
                Unchecked_Convert_To (Obj_Ptr,
                  New_Occurrence_Of (Param, Loc)));

            --  Analyze new actual. Other actuals in calls are already analyzed
            --  and the list of actuals is not reanalyzed after rewriting.

            Set_Parent (Rec, N);
            Analyze (Rec);
         end;
      end if;

      return Rec;
   end Expand_Protected_Object_Reference;

   --------------------------------------
   -- Expand_Protected_Subprogram_Call --
   --------------------------------------

   procedure Expand_Protected_Subprogram_Call
     (N    : Node_Id;
      Subp : Entity_Id;
      Scop : Entity_Id)
   is
      Rec   : Node_Id;

   begin
      --  If the protected object is not an enclosing scope, this is
      --  an inter-object function call. Inter-object procedure
      --  calls are expanded by Exp_Ch9.Build_Simple_Entry_Call.
      --  The call is intra-object only if the subprogram being
      --  called is in the protected body being compiled, and if the
      --  protected object in the call is statically the enclosing type.
      --  The object may be an component of some other data structure,
      --  in which case this must be handled as an inter-object call.

      if not In_Open_Scopes (Scop)
        or else not Is_Entity_Name (Name (N))
      then
         if Nkind (Name (N)) = N_Selected_Component then
            Rec := Prefix (Name (N));

         else
            pragma Assert (Nkind (Name (N)) = N_Indexed_Component);
            Rec := Prefix (Prefix (Name (N)));
         end if;

         Build_Protected_Subprogram_Call (N,
           Name => New_Occurrence_Of (Subp, Sloc (N)),
           Rec =>  Convert_Concurrent (Rec, Etype (Rec)),
           External => True);

      else
         Rec := Expand_Protected_Object_Reference (N, Scop);

         if No (Rec) then
            return;
         end if;

         Build_Protected_Subprogram_Call (N,
           Name     => Name (N),
           Rec      => Rec,
           External => False);

      end if;

      --  If it is a function call it can appear in elaboration code and
      --  the called entity must be frozen here.

      if Ekind (Subp) = E_Function then
         Freeze_Expression (Name (N));
      end if;

      --  Analyze and resolve the new call. The actuals have already been
      --  resolved, but expansion of a function call will add extra actuals
      --  if needed. Analysis of a procedure call already includes resolution.

      Analyze (N);

      if Ekind (Subp) = E_Function then
         Resolve (N, Etype (Subp));
      end if;
   end Expand_Protected_Subprogram_Call;

   -----------------------------------
   -- Expand_Simple_Function_Return --
   -----------------------------------

   --  The "simple" comes from the syntax rule simple_return_statement.
   --  The semantics are not at all simple!

   procedure Expand_Simple_Function_Return (N : Node_Id) is
      Loc : constant Source_Ptr := Sloc (N);

      Scope_Id : constant Entity_Id :=
                   Return_Applies_To (Return_Statement_Entity (N));
      --  The function we are returning from

      R_Type : constant Entity_Id := Etype (Scope_Id);
      --  The result type of the function

      Utyp : constant Entity_Id := Underlying_Type (R_Type);

      Exp : constant Node_Id := Expression (N);
      pragma Assert (Present (Exp));

      Exptyp : constant Entity_Id := Etype (Exp);
      --  The type of the expression (not necessarily the same as R_Type)

      Subtype_Ind : Node_Id;
      --  If the result type of the function is class-wide and the
      --  expression has a specific type, then we use the expression's
      --  type as the type of the return object. In cases where the
      --  expression is an aggregate that is built in place, this avoids
      --  the need for an expensive conversion of the return object to
      --  the specific type on assignments to the individual components.

   begin
      if Is_Class_Wide_Type (R_Type)
        and then not Is_Class_Wide_Type (Etype (Exp))
      then
         Subtype_Ind := New_Occurrence_Of (Etype (Exp), Loc);
      else
         Subtype_Ind := New_Occurrence_Of (R_Type, Loc);
      end if;

      --  For the case of a simple return that does not come from an extended
      --  return, in the case of Ada 2005 where we are returning a limited
      --  type, we rewrite "return <expression>;" to be:

      --    return _anon_ : <return_subtype> := <expression>

      --  The expansion produced by Expand_N_Extended_Return_Statement will
      --  contain simple return statements (for example, a block containing
      --  simple return of the return object), which brings us back here with
      --  Comes_From_Extended_Return_Statement set. The reason for the barrier
      --  checking for a simple return that does not come from an extended
      --  return is to avoid this infinite recursion.

      --  The reason for this design is that for Ada 2005 limited returns, we
      --  need to reify the return object, so we can build it "in place", and
      --  we need a block statement to hang finalization and tasking stuff.

      --  ??? In order to avoid disruption, we avoid translating to extended
      --  return except in the cases where we really need to (Ada 2005 for
      --  inherently limited). We might prefer to do this translation in all
      --  cases (except perhaps for the case of Ada 95 inherently limited),
      --  in order to fully exercise the Expand_N_Extended_Return_Statement
      --  code. This would also allow us to do the build-in-place optimization
      --  for efficiency even in cases where it is semantically not required.

      --  As before, we check the type of the return expression rather than the
      --  return type of the function, because the latter may be a limited
      --  class-wide interface type, which is not a limited type, even though
      --  the type of the expression may be.

      if not Comes_From_Extended_Return_Statement (N)
        and then Is_Immutably_Limited_Type (Etype (Expression (N)))
        and then Ada_Version >= Ada_2005
        and then not Debug_Flag_Dot_L
      then
         declare
            Return_Object_Entity : constant Entity_Id :=
                                     Make_Temporary (Loc, 'R', Exp);
            Obj_Decl : constant Node_Id :=
                         Make_Object_Declaration (Loc,
                           Defining_Identifier => Return_Object_Entity,
                           Object_Definition   => Subtype_Ind,
                           Expression          => Exp);

            Ext : constant Node_Id := Make_Extended_Return_Statement (Loc,
                    Return_Object_Declarations => New_List (Obj_Decl));
            --  Do not perform this high-level optimization if the result type
            --  is an interface because the "this" pointer must be displaced.

         begin
            Rewrite (N, Ext);
            Analyze (N);
            return;
         end;
      end if;

      --  Here we have a simple return statement that is part of the expansion
      --  of an extended return statement (either written by the user, or
      --  generated by the above code).

      --  Always normalize C/Fortran boolean result. This is not always needed,
      --  but it seems a good idea to minimize the passing around of non-
      --  normalized values, and in any case this handles the processing of
      --  barrier functions for protected types, which turn the condition into
      --  a return statement.

      if Is_Boolean_Type (Exptyp)
        and then Nonzero_Is_True (Exptyp)
      then
         Adjust_Condition (Exp);
         Adjust_Result_Type (Exp, Exptyp);
      end if;

      --  Do validity check if enabled for returns

      if Validity_Checks_On
        and then Validity_Check_Returns
      then
         Ensure_Valid (Exp);
      end if;

      --  Check the result expression of a scalar function against the subtype
      --  of the function by inserting a conversion. This conversion must
      --  eventually be performed for other classes of types, but for now it's
      --  only done for scalars.
      --  ???

      if Is_Scalar_Type (Exptyp) then
         Rewrite (Exp, Convert_To (R_Type, Exp));

         --  The expression is resolved to ensure that the conversion gets
         --  expanded to generate a possible constraint check.

         Analyze_And_Resolve (Exp, R_Type);
      end if;

      --  Deal with returning variable length objects and controlled types

      --  Nothing to do if we are returning by reference, or this is not a
      --  type that requires special processing (indicated by the fact that
      --  it requires a cleanup scope for the secondary stack case).

      if Is_Immutably_Limited_Type (Exptyp)
        or else Is_Limited_Interface (Exptyp)
      then
         null;

      elsif not Requires_Transient_Scope (R_Type) then

         --  Mutable records with no variable length components are not
         --  returned on the sec-stack, so we need to make sure that the
         --  backend will only copy back the size of the actual value, and not
         --  the maximum size. We create an actual subtype for this purpose.

         declare
            Ubt  : constant Entity_Id := Underlying_Type (Base_Type (Exptyp));
            Decl : Node_Id;
            Ent  : Entity_Id;
         begin
            if Has_Discriminants (Ubt)
              and then not Is_Constrained (Ubt)
              and then not Has_Unchecked_Union (Ubt)
            then
               Decl := Build_Actual_Subtype (Ubt, Exp);
               Ent := Defining_Identifier (Decl);
               Insert_Action (Exp, Decl);
               Rewrite (Exp, Unchecked_Convert_To (Ent, Exp));
               Analyze_And_Resolve (Exp);
            end if;
         end;

      --  Here if secondary stack is used

      else
         --  Make sure that no surrounding block will reclaim the secondary
         --  stack on which we are going to put the result. Not only may this
         --  introduce secondary stack leaks but worse, if the reclamation is
         --  done too early, then the result we are returning may get
         --  clobbered.

         declare
            S : Entity_Id;
         begin
            S := Current_Scope;
            while Ekind (S) = E_Block or else Ekind (S) = E_Loop loop
               Set_Sec_Stack_Needed_For_Return (S, True);
               S := Enclosing_Dynamic_Scope (S);
            end loop;
         end;

         --  Optimize the case where the result is a function call. In this
         --  case either the result is already on the secondary stack, or is
         --  already being returned with the stack pointer depressed and no
         --  further processing is required except to set the By_Ref flag to
         --  ensure that gigi does not attempt an extra unnecessary copy.
         --  (actually not just unnecessary but harmfully wrong in the case
         --  of a controlled type, where gigi does not know how to do a copy).
         --  To make up for a gcc 2.8.1 deficiency (???), we perform
         --  the copy for array types if the constrained status of the
         --  target type is different from that of the expression.

         if Requires_Transient_Scope (Exptyp)
           and then
              (not Is_Array_Type (Exptyp)
                or else Is_Constrained (Exptyp) = Is_Constrained (R_Type)
                or else CW_Or_Has_Controlled_Part (Utyp))
           and then Nkind (Exp) = N_Function_Call
         then
            Set_By_Ref (N);

            --  Remove side effects from the expression now so that other parts
            --  of the expander do not have to reanalyze this node without this
            --  optimization

            Rewrite (Exp, Duplicate_Subexpr_No_Checks (Exp));

         --  For controlled types, do the allocation on the secondary stack
         --  manually in order to call adjust at the right time:

         --    type Anon1 is access R_Type;
         --    for Anon1'Storage_pool use ss_pool;
         --    Anon2 : anon1 := new R_Type'(expr);
         --    return Anon2.all;

         --  We do the same for classwide types that are not potentially
         --  controlled (by the virtue of restriction No_Finalization) because
         --  gigi is not able to properly allocate class-wide types.

         elsif CW_Or_Has_Controlled_Part (Utyp) then
            declare
               Loc        : constant Source_Ptr := Sloc (N);
               Acc_Typ    : constant Entity_Id := Make_Temporary (Loc, 'A');
               Alloc_Node : Node_Id;
               Temp       : Entity_Id;

            begin
               Set_Ekind (Acc_Typ, E_Access_Type);

               Set_Associated_Storage_Pool (Acc_Typ, RTE (RE_SS_Pool));

               --  This is an allocator for the secondary stack, and it's fine
               --  to have Comes_From_Source set False on it, as gigi knows not
               --  to flag it as a violation of No_Implicit_Heap_Allocations.

               Alloc_Node :=
                 Make_Allocator (Loc,
                   Expression =>
                     Make_Qualified_Expression (Loc,
                       Subtype_Mark => New_Reference_To (Etype (Exp), Loc),
                       Expression   => Relocate_Node (Exp)));

               --  We do not want discriminant checks on the declaration,
               --  given that it gets its value from the allocator.

               Set_No_Initialization (Alloc_Node);

               Temp := Make_Temporary (Loc, 'R', Alloc_Node);

               Insert_List_Before_And_Analyze (N, New_List (
                 Make_Full_Type_Declaration (Loc,
                   Defining_Identifier => Acc_Typ,
                   Type_Definition     =>
                     Make_Access_To_Object_Definition (Loc,
                       Subtype_Indication => Subtype_Ind)),

                 Make_Object_Declaration (Loc,
                   Defining_Identifier => Temp,
                   Object_Definition   => New_Reference_To (Acc_Typ, Loc),
                   Expression          => Alloc_Node)));

               Rewrite (Exp,
                 Make_Explicit_Dereference (Loc,
                 Prefix => New_Reference_To (Temp, Loc)));

               Analyze_And_Resolve (Exp, R_Type);
            end;

         --  Otherwise use the gigi mechanism to allocate result on the
         --  secondary stack.

         else
            Check_Restriction (No_Secondary_Stack, N);
            Set_Storage_Pool (N, RTE (RE_SS_Pool));

            --  If we are generating code for the VM do not use
            --  SS_Allocate since everything is heap-allocated anyway.

            if VM_Target = No_VM then
               Set_Procedure_To_Call (N, RTE (RE_SS_Allocate));
            end if;
         end if;
      end if;

      --  Implement the rules of 6.5(8-10), which require a tag check in the
      --  case of a limited tagged return type, and tag reassignment for
      --  nonlimited tagged results. These actions are needed when the return
      --  type is a specific tagged type and the result expression is a
      --  conversion or a formal parameter, because in that case the tag of the
      --  expression might differ from the tag of the specific result type.

      if Is_Tagged_Type (Utyp)
        and then not Is_Class_Wide_Type (Utyp)
        and then (Nkind_In (Exp, N_Type_Conversion,
                                 N_Unchecked_Type_Conversion)
                    or else (Is_Entity_Name (Exp)
                               and then Ekind (Entity (Exp)) in Formal_Kind))
      then
         --  When the return type is limited, perform a check that the
         --  tag of the result is the same as the tag of the return type.

         if Is_Limited_Type (R_Type) then
            Insert_Action (Exp,
              Make_Raise_Constraint_Error (Loc,
                Condition =>
                  Make_Op_Ne (Loc,
                    Left_Opnd =>
                      Make_Selected_Component (Loc,
                        Prefix        => Duplicate_Subexpr (Exp),
                        Selector_Name => Make_Identifier (Loc, Name_uTag)),
                    Right_Opnd =>
                      Make_Attribute_Reference (Loc,
                        Prefix => New_Occurrence_Of (Base_Type (Utyp), Loc),
                        Attribute_Name => Name_Tag)),
                Reason => CE_Tag_Check_Failed));

         --  If the result type is a specific nonlimited tagged type, then we
         --  have to ensure that the tag of the result is that of the result
         --  type. This is handled by making a copy of the expression in the
         --  case where it might have a different tag, namely when the
         --  expression is a conversion or a formal parameter. We create a new
         --  object of the result type and initialize it from the expression,
         --  which will implicitly force the tag to be set appropriately.

         else
            declare
               ExpR       : constant Node_Id   := Relocate_Node (Exp);
               Result_Id  : constant Entity_Id :=
                              Make_Temporary (Loc, 'R', ExpR);
               Result_Exp : constant Node_Id   :=
                              New_Reference_To (Result_Id, Loc);
               Result_Obj : constant Node_Id   :=
                              Make_Object_Declaration (Loc,
                                Defining_Identifier => Result_Id,
                                Object_Definition   =>
                                  New_Reference_To (R_Type, Loc),
                                Constant_Present    => True,
                                Expression          => ExpR);

            begin
               Set_Assignment_OK (Result_Obj);
               Insert_Action (Exp, Result_Obj);

               Rewrite (Exp, Result_Exp);
               Analyze_And_Resolve (Exp, R_Type);
            end;
         end if;

      --  Ada 2005 (AI-344): If the result type is class-wide, then insert
      --  a check that the level of the return expression's underlying type
      --  is not deeper than the level of the master enclosing the function.
      --  Always generate the check when the type of the return expression
      --  is class-wide, when it's a type conversion, or when it's a formal
      --  parameter. Otherwise, suppress the check in the case where the
      --  return expression has a specific type whose level is known not to
      --  be statically deeper than the function's result type.

      --  Note: accessibility check is skipped in the VM case, since there
      --  does not seem to be any practical way to implement this check.

      elsif Ada_Version >= Ada_2005
        and then Tagged_Type_Expansion
        and then Is_Class_Wide_Type (R_Type)
        and then not Scope_Suppress (Accessibility_Check)
        and then
          (Is_Class_Wide_Type (Etype (Exp))
            or else Nkind_In (Exp, N_Type_Conversion,
                                   N_Unchecked_Type_Conversion)
            or else (Is_Entity_Name (Exp)
                       and then Ekind (Entity (Exp)) in Formal_Kind)
            or else Scope_Depth (Enclosing_Dynamic_Scope (Etype (Exp))) >
                      Scope_Depth (Enclosing_Dynamic_Scope (Scope_Id)))
      then
         declare
            Tag_Node : Node_Id;

         begin
            --  Ada 2005 (AI-251): In class-wide interface objects we displace
            --  "this" to reference the base of the object --- required to get
            --  access to the TSD of the object.

            if Is_Class_Wide_Type (Etype (Exp))
              and then Is_Interface (Etype (Exp))
              and then Nkind (Exp) = N_Explicit_Dereference
            then
               Tag_Node :=
                 Make_Explicit_Dereference (Loc,
                   Unchecked_Convert_To (RTE (RE_Tag_Ptr),
                     Make_Function_Call (Loc,
                       Name => New_Reference_To (RTE (RE_Base_Address), Loc),
                       Parameter_Associations => New_List (
                         Unchecked_Convert_To (RTE (RE_Address),
                           Duplicate_Subexpr (Prefix (Exp)))))));
            else
               Tag_Node :=
                 Make_Attribute_Reference (Loc,
                   Prefix => Duplicate_Subexpr (Exp),
                   Attribute_Name => Name_Tag);
            end if;

            Insert_Action (Exp,
              Make_Raise_Program_Error (Loc,
                Condition =>
                  Make_Op_Gt (Loc,
                    Left_Opnd =>
                      Build_Get_Access_Level (Loc, Tag_Node),
                    Right_Opnd =>
                      Make_Integer_Literal (Loc,
                        Scope_Depth (Enclosing_Dynamic_Scope (Scope_Id)))),
                Reason => PE_Accessibility_Check_Failed));
         end;

      --  AI05-0073: If function has a controlling access result, check that
      --  the tag of the return value, if it is not null, matches designated
      --  type of return type.
      --  The return expression is referenced twice in the code below, so
      --  it must be made free of side effects. Given that different compilers
      --  may evaluate these parameters in different order, both occurrences
      --  perform a copy.

      elsif Ekind (R_Type) = E_Anonymous_Access_Type
        and then Has_Controlling_Result (Scope_Id)
      then
         Insert_Action (N,
           Make_Raise_Constraint_Error (Loc,
             Condition =>
               Make_And_Then (Loc,
                 Left_Opnd  =>
                   Make_Op_Ne (Loc,
                     Left_Opnd  => Duplicate_Subexpr (Exp),
                     Right_Opnd => Make_Null (Loc)),
                 Right_Opnd => Make_Op_Ne (Loc,
                   Left_Opnd  =>
                     Make_Selected_Component (Loc,
                       Prefix        => Duplicate_Subexpr (Exp),
                       Selector_Name => Make_Identifier (Loc, Name_uTag)),
                   Right_Opnd =>
                     Make_Attribute_Reference (Loc,
                       Prefix         =>
                         New_Occurrence_Of (Designated_Type (R_Type), Loc),
                       Attribute_Name => Name_Tag))),
             Reason    => CE_Tag_Check_Failed),
             Suppress  => All_Checks);
      end if;

      --  If we are returning an object that may not be bit-aligned, then copy
      --  the value into a temporary first. This copy may need to expand to a
      --  loop of component operations.

      if Is_Possibly_Unaligned_Slice (Exp)
        or else Is_Possibly_Unaligned_Object (Exp)
      then
         declare
            ExpR : constant Node_Id   := Relocate_Node (Exp);
            Tnn  : constant Entity_Id := Make_Temporary (Loc, 'T', ExpR);
         begin
            Insert_Action (Exp,
              Make_Object_Declaration (Loc,
                Defining_Identifier => Tnn,
                Constant_Present    => True,
                Object_Definition   => New_Occurrence_Of (R_Type, Loc),
                Expression          => ExpR),
              Suppress            => All_Checks);
            Rewrite (Exp, New_Occurrence_Of (Tnn, Loc));
         end;
      end if;

      --  Generate call to postcondition checks if they are present

      if Ekind (Scope_Id) = E_Function
        and then Has_Postconditions (Scope_Id)
      then
         --  We are going to reference the returned value twice in this case,
         --  once in the call to _Postconditions, and once in the actual return
         --  statement, but we can't have side effects happening twice, and in
         --  any case for efficiency we don't want to do the computation twice.

         --  If the returned expression is an entity name, we don't need to
         --  worry since it is efficient and safe to reference it twice, that's
         --  also true for literals other than string literals, and for the
         --  case of X.all where X is an entity name.

         if Is_Entity_Name (Exp)
           or else Nkind_In (Exp, N_Character_Literal,
                                  N_Integer_Literal,
                                  N_Real_Literal)
           or else (Nkind (Exp) = N_Explicit_Dereference
                      and then Is_Entity_Name (Prefix (Exp)))
         then
            null;

         --  Otherwise we are going to need a temporary to capture the value

         else
            declare
               ExpR : constant Node_Id   := Relocate_Node (Exp);
               Tnn  : constant Entity_Id := Make_Temporary (Loc, 'T', ExpR);

            begin
               --  For a complex expression of an elementary type, capture
               --  value in the temporary and use it as the reference.

               if Is_Elementary_Type (R_Type) then
                  Insert_Action (Exp,
                    Make_Object_Declaration (Loc,
                      Defining_Identifier => Tnn,
                      Constant_Present    => True,
                      Object_Definition   => New_Occurrence_Of (R_Type, Loc),
                      Expression          => ExpR),
                    Suppress => All_Checks);

                  Rewrite (Exp, New_Occurrence_Of (Tnn, Loc));

               --  If we have something we can rename, generate a renaming of
               --  the object and replace the expression with a reference

               elsif Is_Object_Reference (Exp) then
                  Insert_Action (Exp,
                    Make_Object_Renaming_Declaration (Loc,
                      Defining_Identifier => Tnn,
                      Subtype_Mark        => New_Occurrence_Of (R_Type, Loc),
                      Name                => ExpR),
                    Suppress => All_Checks);

                  Rewrite (Exp, New_Occurrence_Of (Tnn, Loc));

               --  Otherwise we have something like a string literal or an
               --  aggregate. We could copy the value, but that would be
               --  inefficient. Instead we make a reference to the value and
               --  capture this reference with a renaming, the expression is
               --  then replaced by a dereference of this renaming.

               else
                  --  For now, copy the value, since the code below does not
                  --  seem to work correctly ???

                  Insert_Action (Exp,
                    Make_Object_Declaration (Loc,
                      Defining_Identifier => Tnn,
                      Constant_Present    => True,
                      Object_Definition   => New_Occurrence_Of (R_Type, Loc),
                      Expression          => Relocate_Node (Exp)),
                    Suppress => All_Checks);

                  Rewrite (Exp, New_Occurrence_Of (Tnn, Loc));

                  --  Insert_Action (Exp,
                  --    Make_Object_Renaming_Declaration (Loc,
                  --      Defining_Identifier => Tnn,
                  --      Access_Definition =>
                  --        Make_Access_Definition (Loc,
                  --          All_Present  => True,
                  --          Subtype_Mark => New_Occurrence_Of (R_Type, Loc)),
                  --      Name =>
                  --        Make_Reference (Loc,
                  --          Prefix => Relocate_Node (Exp))),
                  --    Suppress => All_Checks);

                  --  Rewrite (Exp,
                  --    Make_Explicit_Dereference (Loc,
                  --      Prefix => New_Occurrence_Of (Tnn, Loc)));
               end if;
            end;
         end if;

         --  Generate call to _postconditions

         Insert_Action (Exp,
           Make_Procedure_Call_Statement (Loc,
             Name => Make_Identifier (Loc, Name_uPostconditions),
             Parameter_Associations => New_List (Duplicate_Subexpr (Exp))));
      end if;

      --  Ada 2005 (AI-251): If this return statement corresponds with an
      --  simple return statement associated with an extended return statement
      --  and the type of the returned object is an interface then generate an
      --  implicit conversion to force displacement of the "this" pointer.

      if Ada_Version >= Ada_2005
        and then Comes_From_Extended_Return_Statement (N)
        and then Nkind (Expression (N)) = N_Identifier
        and then Is_Interface (Utyp)
        and then Utyp /= Underlying_Type (Exptyp)
      then
         Rewrite (Exp, Convert_To (Utyp, Relocate_Node (Exp)));
         Analyze_And_Resolve (Exp);
      end if;
   end Expand_Simple_Function_Return;

   --------------------------------
   -- Is_Build_In_Place_Function --
   --------------------------------

   function Is_Build_In_Place_Function (E : Entity_Id) return Boolean is
   begin
      --  This function is called from Expand_Subtype_From_Expr during
      --  semantic analysis, even when expansion is off. In those cases
      --  the build_in_place expansion will not take place.

      if not Expander_Active then
         return False;
      end if;

      --  For now we test whether E denotes a function or access-to-function
      --  type whose result subtype is inherently limited. Later this test may
      --  be revised to allow composite nonlimited types. Functions with a
      --  foreign convention or whose result type has a foreign convention
      --  never qualify.

      if Ekind_In (E, E_Function, E_Generic_Function)
        or else (Ekind (E) = E_Subprogram_Type
                  and then Etype (E) /= Standard_Void_Type)
      then
         --  Note: If you have Convention (C) on an inherently limited type,
         --  you're on your own. That is, the C code will have to be carefully
         --  written to know about the Ada conventions.

         if Has_Foreign_Convention (E)
           or else Has_Foreign_Convention (Etype (E))
         then
            return False;

         --  In Ada 2005 all functions with an inherently limited return type
         --  must be handled using a build-in-place profile, including the case
         --  of a function with a limited interface result, where the function
         --  may return objects of nonlimited descendants.

         else
            return Is_Immutably_Limited_Type (Etype (E))
              and then Ada_Version >= Ada_2005
              and then not Debug_Flag_Dot_L;
         end if;

      else
         return False;
      end if;
   end Is_Build_In_Place_Function;

   -------------------------------------
   -- Is_Build_In_Place_Function_Call --
   -------------------------------------

   function Is_Build_In_Place_Function_Call (N : Node_Id) return Boolean is
      Exp_Node    : Node_Id := N;
      Function_Id : Entity_Id;

   begin
      --  Step past qualification or unchecked conversion (the latter can occur
      --  in cases of calls to 'Input).

      if Nkind_In
           (Exp_Node, N_Qualified_Expression, N_Unchecked_Type_Conversion)
      then
         Exp_Node := Expression (N);
      end if;

      if Nkind (Exp_Node) /= N_Function_Call then
         return False;

      else
         if Is_Entity_Name (Name (Exp_Node)) then
            Function_Id := Entity (Name (Exp_Node));

         elsif Nkind (Name (Exp_Node)) = N_Explicit_Dereference then
            Function_Id := Etype (Name (Exp_Node));
         end if;

         return Is_Build_In_Place_Function (Function_Id);
      end if;
   end Is_Build_In_Place_Function_Call;

   -----------------------
   -- Freeze_Subprogram --
   -----------------------

   procedure Freeze_Subprogram (N : Node_Id) is
      Loc : constant Source_Ptr := Sloc (N);

      procedure Register_Predefined_DT_Entry (Prim : Entity_Id);
      --  (Ada 2005): Register a predefined primitive in all the secondary
      --  dispatch tables of its primitive type.

      ----------------------------------
      -- Register_Predefined_DT_Entry --
      ----------------------------------

      procedure Register_Predefined_DT_Entry (Prim : Entity_Id) is
         Iface_DT_Ptr : Elmt_Id;
         Tagged_Typ   : Entity_Id;
         Thunk_Id     : Entity_Id;
         Thunk_Code   : Node_Id;

      begin
         Tagged_Typ := Find_Dispatching_Type (Prim);

         if No (Access_Disp_Table (Tagged_Typ))
           or else not Has_Interfaces (Tagged_Typ)
           or else not RTE_Available (RE_Interface_Tag)
           or else Restriction_Active (No_Dispatching_Calls)
         then
            return;
         end if;

         --  Skip the first two access-to-dispatch-table pointers since they
         --  leads to the primary dispatch table (predefined DT and user
         --  defined DT). We are only concerned with the secondary dispatch
         --  table pointers. Note that the access-to- dispatch-table pointer
         --  corresponds to the first implemented interface retrieved below.

         Iface_DT_Ptr :=
           Next_Elmt (Next_Elmt (First_Elmt (Access_Disp_Table (Tagged_Typ))));

         while Present (Iface_DT_Ptr)
            and then Ekind (Node (Iface_DT_Ptr)) = E_Constant
         loop
            pragma Assert (Has_Thunks (Node (Iface_DT_Ptr)));
            Expand_Interface_Thunk (Prim, Thunk_Id, Thunk_Code);

            if Present (Thunk_Code) then
               Insert_Actions_After (N, New_List (
                 Thunk_Code,

                 Build_Set_Predefined_Prim_Op_Address (Loc,
                   Tag_Node =>
                     New_Reference_To (Node (Next_Elmt (Iface_DT_Ptr)), Loc),
                   Position => DT_Position (Prim),
                   Address_Node =>
                     Unchecked_Convert_To (RTE (RE_Prim_Ptr),
                       Make_Attribute_Reference (Loc,
                         Prefix         => New_Reference_To (Thunk_Id, Loc),
                         Attribute_Name => Name_Unrestricted_Access))),

                 Build_Set_Predefined_Prim_Op_Address (Loc,
                   Tag_Node =>
                     New_Reference_To
                      (Node (Next_Elmt (Next_Elmt (Next_Elmt (Iface_DT_Ptr)))),
                       Loc),
                   Position => DT_Position (Prim),
                   Address_Node =>
                     Unchecked_Convert_To (RTE (RE_Prim_Ptr),
                       Make_Attribute_Reference (Loc,
                         Prefix         => New_Reference_To (Prim, Loc),
                         Attribute_Name => Name_Unrestricted_Access)))));
            end if;

            --  Skip the tag of the predefined primitives dispatch table

            Next_Elmt (Iface_DT_Ptr);
            pragma Assert (Has_Thunks (Node (Iface_DT_Ptr)));

            --  Skip the tag of the no-thunks dispatch table

            Next_Elmt (Iface_DT_Ptr);
            pragma Assert (not Has_Thunks (Node (Iface_DT_Ptr)));

            --  Skip the tag of the predefined primitives no-thunks dispatch
            --  table

            Next_Elmt (Iface_DT_Ptr);
            pragma Assert (not Has_Thunks (Node (Iface_DT_Ptr)));

            Next_Elmt (Iface_DT_Ptr);
         end loop;
      end Register_Predefined_DT_Entry;

      --  Local variables

      Subp : constant Entity_Id := Entity (N);

   --  Start of processing for Freeze_Subprogram

   begin
      --  We suppress the initialization of the dispatch table entry when
      --  VM_Target because the dispatching mechanism is handled internally
      --  by the VM.

      if Is_Dispatching_Operation (Subp)
        and then not Is_Abstract_Subprogram (Subp)
        and then Present (DTC_Entity (Subp))
        and then Present (Scope (DTC_Entity (Subp)))
        and then Tagged_Type_Expansion
        and then not Restriction_Active (No_Dispatching_Calls)
        and then RTE_Available (RE_Tag)
      then
         declare
            Typ : constant Entity_Id := Scope (DTC_Entity (Subp));

         begin
            --  Handle private overridden primitives

            if not Is_CPP_Class (Typ) then
               Check_Overriding_Operation (Subp);
            end if;

            --  We assume that imported CPP primitives correspond with objects
            --  whose constructor is in the CPP side; therefore we don't need
            --  to generate code to register them in the dispatch table.

            if Is_CPP_Class (Typ) then
               null;

            --  Handle CPP primitives found in derivations of CPP_Class types.
            --  These primitives must have been inherited from some parent, and
            --  there is no need to register them in the dispatch table because
            --  Build_Inherit_Prims takes care of the initialization of these
            --  slots.

            elsif Is_Imported (Subp)
                    and then (Convention (Subp) = Convention_CPP
                                or else Convention (Subp) = Convention_C)
            then
               null;

            --  Generate code to register the primitive in non statically
            --  allocated dispatch tables

            elsif not Building_Static_DT (Scope (DTC_Entity (Subp))) then

               --  When a primitive is frozen, enter its name in its dispatch
               --  table slot.

               if not Is_Interface (Typ)
                 or else Present (Interface_Alias (Subp))
               then
                  if Is_Predefined_Dispatching_Operation (Subp) then
                     Register_Predefined_DT_Entry (Subp);
                  end if;

                  Insert_Actions_After (N,
                    Register_Primitive (Loc, Prim => Subp));
               end if;
            end if;
         end;
      end if;

      --  Mark functions that return by reference. Note that it cannot be part
      --  of the normal semantic analysis of the spec since the underlying
      --  returned type may not be known yet (for private types).

      declare
         Typ  : constant Entity_Id := Etype (Subp);
         Utyp : constant Entity_Id := Underlying_Type (Typ);
      begin
         if Is_Immutably_Limited_Type (Typ) then
            Set_Returns_By_Ref (Subp);
         elsif Present (Utyp) and then CW_Or_Has_Controlled_Part (Utyp) then
            Set_Returns_By_Ref (Subp);
         end if;
      end;
   end Freeze_Subprogram;

   -----------------------
   -- Is_Null_Procedure --
   -----------------------

   function Is_Null_Procedure (Subp : Entity_Id) return Boolean is
      Decl : constant Node_Id := Unit_Declaration_Node (Subp);

   begin
      if Ekind (Subp) /= E_Procedure then
         return False;

      --  Check if this is a declared null procedure

      elsif Nkind (Decl) = N_Subprogram_Declaration then
         if not Null_Present (Specification (Decl)) then
            return False;

         elsif No (Body_To_Inline (Decl)) then
            return False;

         --  Check if the body contains only a null statement, followed by
         --  the return statement added during expansion.

         else
            declare
               Orig_Bod : constant Node_Id := Body_To_Inline (Decl);

               Stat  : Node_Id;
               Stat2 : Node_Id;

            begin
               if Nkind (Orig_Bod) /= N_Subprogram_Body then
                  return False;
               else
                  --  We must skip SCIL nodes because they are currently
                  --  implemented as special N_Null_Statement nodes.

                  Stat :=
                     First_Non_SCIL_Node
                       (Statements (Handled_Statement_Sequence (Orig_Bod)));
                  Stat2 := Next_Non_SCIL_Node (Stat);

                  return
                     Is_Empty_List (Declarations (Orig_Bod))
                       and then Nkind (Stat) = N_Null_Statement
                       and then
                        (No (Stat2)
                          or else
                            (Nkind (Stat2) = N_Simple_Return_Statement
                              and then No (Next (Stat2))));
               end if;
            end;
         end if;

      else
         return False;
      end if;
   end Is_Null_Procedure;

   -------------------------------------------
   -- Make_Build_In_Place_Call_In_Allocator --
   -------------------------------------------

   procedure Make_Build_In_Place_Call_In_Allocator
     (Allocator     : Node_Id;
      Function_Call : Node_Id)
   is
      Loc               : Source_Ptr;
      Func_Call         : Node_Id := Function_Call;
      Function_Id       : Entity_Id;
      Result_Subt       : Entity_Id;
      Acc_Type          : constant Entity_Id := Etype (Allocator);
      New_Allocator     : Node_Id;
      Return_Obj_Access : Entity_Id;

   begin
      --  Step past qualification or unchecked conversion (the latter can occur
      --  in cases of calls to 'Input).

      if Nkind_In (Func_Call,
                   N_Qualified_Expression,
                   N_Unchecked_Type_Conversion)
      then
         Func_Call := Expression (Func_Call);
      end if;

      --  If the call has already been processed to add build-in-place actuals
      --  then return. This should not normally occur in an allocator context,
      --  but we add the protection as a defensive measure.

      if Is_Expanded_Build_In_Place_Call (Func_Call) then
         return;
      end if;

      --  Mark the call as processed as a build-in-place call

      Set_Is_Expanded_Build_In_Place_Call (Func_Call);

      Loc := Sloc (Function_Call);

      if Is_Entity_Name (Name (Func_Call)) then
         Function_Id := Entity (Name (Func_Call));

      elsif Nkind (Name (Func_Call)) = N_Explicit_Dereference then
         Function_Id := Etype (Name (Func_Call));

      else
         raise Program_Error;
      end if;

      Result_Subt := Etype (Function_Id);

      --  When the result subtype is constrained, the return object must be
      --  allocated on the caller side, and access to it is passed to the
      --  function.

      --  Here and in related routines, we must examine the full view of the
      --  type, because the view at the point of call may differ from that
      --  that in the function body, and the expansion mechanism depends on
      --  the characteristics of the full view.

      if Is_Constrained (Underlying_Type (Result_Subt)) then

         --  Replace the initialized allocator of form "new T'(Func (...))"
         --  with an uninitialized allocator of form "new T", where T is the
         --  result subtype of the called function. The call to the function
         --  is handled separately further below.

         New_Allocator :=
           Make_Allocator (Loc,
             Expression => New_Reference_To (Result_Subt, Loc));
         Set_No_Initialization (New_Allocator);

         --  Copy attributes to new allocator. Note that the new allocator
         --  logically comes from source if the original one did, so copy the
         --  relevant flag. This ensures proper treatment of the restriction
         --  No_Implicit_Heap_Allocations in this case.

         Set_Storage_Pool      (New_Allocator, Storage_Pool      (Allocator));
         Set_Procedure_To_Call (New_Allocator, Procedure_To_Call (Allocator));
         Set_Comes_From_Source (New_Allocator, Comes_From_Source (Allocator));

         Rewrite (Allocator, New_Allocator);

         --  Create a new access object and initialize it to the result of the
         --  new uninitialized allocator. Note: we do not use Allocator as the
         --  Related_Node of Return_Obj_Access in call to Make_Temporary below
         --  as this would create a sort of infinite "recursion".

         Return_Obj_Access := Make_Temporary (Loc, 'R');
         Set_Etype (Return_Obj_Access, Acc_Type);

         Insert_Action (Allocator,
           Make_Object_Declaration (Loc,
             Defining_Identifier => Return_Obj_Access,
             Object_Definition   => New_Reference_To (Acc_Type, Loc),
             Expression          => Relocate_Node (Allocator)));

         --  When the function has a controlling result, an allocation-form
         --  parameter must be passed indicating that the caller is allocating
         --  the result object. This is needed because such a function can be
         --  called as a dispatching operation and must be treated similarly
         --  to functions with unconstrained result subtypes.

         Add_Alloc_Form_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Alloc_Form => Caller_Allocation);

         Add_Final_List_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Acc_Type);

         Add_Task_Actuals_To_Build_In_Place_Call
           (Func_Call, Function_Id, Master_Actual => Master_Id (Acc_Type));

         --  Add an implicit actual to the function call that provides access
         --  to the allocated object. An unchecked conversion to the (specific)
         --  result subtype of the function is inserted to handle cases where
         --  the access type of the allocator has a class-wide designated type.

         Add_Access_Actual_To_Build_In_Place_Call
           (Func_Call,
            Function_Id,
            Make_Unchecked_Type_Conversion (Loc,
              Subtype_Mark => New_Reference_To (Result_Subt, Loc),
              Expression   =>
                Make_Explicit_Dereference (Loc,
                  Prefix => New_Reference_To (Return_Obj_Access, Loc))));

      --  When the result subtype is unconstrained, the function itself must
      --  perform the allocation of the return object, so we pass parameters
      --  indicating that. We don't yet handle the case where the allocation
      --  must be done in a user-defined storage pool, which will require
      --  passing another actual or two to provide allocation/deallocation
      --  operations. ???

      else

         --  Pass an allocation parameter indicating that the function should
         --  allocate its result on the heap.

         Add_Alloc_Form_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Alloc_Form => Global_Heap);

         Add_Final_List_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Acc_Type);

         Add_Task_Actuals_To_Build_In_Place_Call
           (Func_Call, Function_Id, Master_Actual => Master_Id (Acc_Type));

         --  The caller does not provide the return object in this case, so we
         --  have to pass null for the object access actual.

         Add_Access_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Return_Object => Empty);
      end if;

      --  Finally, replace the allocator node with a reference to the result
      --  of the function call itself (which will effectively be an access
      --  to the object created by the allocator).

      Rewrite (Allocator, Make_Reference (Loc, Relocate_Node (Function_Call)));
      Analyze_And_Resolve (Allocator, Acc_Type);
   end Make_Build_In_Place_Call_In_Allocator;

   ---------------------------------------------------
   -- Make_Build_In_Place_Call_In_Anonymous_Context --
   ---------------------------------------------------

   procedure Make_Build_In_Place_Call_In_Anonymous_Context
     (Function_Call : Node_Id)
   is
      Loc             : Source_Ptr;
      Func_Call       : Node_Id := Function_Call;
      Function_Id     : Entity_Id;
      Result_Subt     : Entity_Id;
      Return_Obj_Id   : Entity_Id;
      Return_Obj_Decl : Entity_Id;

   begin
      --  Step past qualification or unchecked conversion (the latter can occur
      --  in cases of calls to 'Input).

      if Nkind_In (Func_Call, N_Qualified_Expression,
                              N_Unchecked_Type_Conversion)
      then
         Func_Call := Expression (Func_Call);
      end if;

      --  If the call has already been processed to add build-in-place actuals
      --  then return. One place this can occur is for calls to build-in-place
      --  functions that occur within a call to a protected operation, where
      --  due to rewriting and expansion of the protected call there can be
      --  more than one call to Expand_Actuals for the same set of actuals.

      if Is_Expanded_Build_In_Place_Call (Func_Call) then
         return;
      end if;

      --  Mark the call as processed as a build-in-place call

      Set_Is_Expanded_Build_In_Place_Call (Func_Call);

      Loc := Sloc (Function_Call);

      if Is_Entity_Name (Name (Func_Call)) then
         Function_Id := Entity (Name (Func_Call));

      elsif Nkind (Name (Func_Call)) = N_Explicit_Dereference then
         Function_Id := Etype (Name (Func_Call));

      else
         raise Program_Error;
      end if;

      Result_Subt := Etype (Function_Id);

      --  When the result subtype is constrained, an object of the subtype is
      --  declared and an access value designating it is passed as an actual.

      if Is_Constrained (Underlying_Type (Result_Subt)) then

         --  Create a temporary object to hold the function result

         Return_Obj_Id := Make_Temporary (Loc, 'R');
         Set_Etype (Return_Obj_Id, Result_Subt);

         Return_Obj_Decl :=
           Make_Object_Declaration (Loc,
             Defining_Identifier => Return_Obj_Id,
             Aliased_Present     => True,
             Object_Definition   => New_Reference_To (Result_Subt, Loc));

         Set_No_Initialization (Return_Obj_Decl);

         Insert_Action (Func_Call, Return_Obj_Decl);

         --  When the function has a controlling result, an allocation-form
         --  parameter must be passed indicating that the caller is allocating
         --  the result object. This is needed because such a function can be
         --  called as a dispatching operation and must be treated similarly
         --  to functions with unconstrained result subtypes.

         Add_Alloc_Form_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Alloc_Form => Caller_Allocation);

         Add_Final_List_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Acc_Type => Empty);

         Add_Task_Actuals_To_Build_In_Place_Call
           (Func_Call, Function_Id, Make_Identifier (Loc, Name_uMaster));

         --  Add an implicit actual to the function call that provides access
         --  to the caller's return object.

         Add_Access_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, New_Reference_To (Return_Obj_Id, Loc));

      --  When the result subtype is unconstrained, the function must allocate
      --  the return object in the secondary stack, so appropriate implicit
      --  parameters are added to the call to indicate that. A transient
      --  scope is established to ensure eventual cleanup of the result.

      else
         --  Pass an allocation parameter indicating that the function should
         --  allocate its result on the secondary stack.

         Add_Alloc_Form_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Alloc_Form => Secondary_Stack);

         Add_Final_List_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Acc_Type => Empty);

         Add_Task_Actuals_To_Build_In_Place_Call
           (Func_Call, Function_Id, Make_Identifier (Loc, Name_uMaster));

         --  Pass a null value to the function since no return object is
         --  available on the caller side.

         Add_Access_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Empty);
      end if;
   end Make_Build_In_Place_Call_In_Anonymous_Context;

   --------------------------------------------
   -- Make_Build_In_Place_Call_In_Assignment --
   --------------------------------------------

   procedure Make_Build_In_Place_Call_In_Assignment
     (Assign        : Node_Id;
      Function_Call : Node_Id)
   is
      Lhs          : constant Node_Id := Name (Assign);
      Func_Call    : Node_Id := Function_Call;
      Func_Id      : Entity_Id;
      Loc          : Source_Ptr;
      Obj_Decl     : Node_Id;
      Obj_Id       : Entity_Id;
      Ptr_Typ      : Entity_Id;
      Ptr_Typ_Decl : Node_Id;
      Result_Subt  : Entity_Id;
      Target       : Node_Id;

   begin
      --  Step past qualification or unchecked conversion (the latter can occur
      --  in cases of calls to 'Input).

      if Nkind_In (Func_Call, N_Qualified_Expression,
                              N_Unchecked_Type_Conversion)
      then
         Func_Call := Expression (Func_Call);
      end if;

      --  If the call has already been processed to add build-in-place actuals
      --  then return. This should not normally occur in an assignment context,
      --  but we add the protection as a defensive measure.

      if Is_Expanded_Build_In_Place_Call (Func_Call) then
         return;
      end if;

      --  Mark the call as processed as a build-in-place call

      Set_Is_Expanded_Build_In_Place_Call (Func_Call);

      Loc := Sloc (Function_Call);

      if Is_Entity_Name (Name (Func_Call)) then
         Func_Id := Entity (Name (Func_Call));

      elsif Nkind (Name (Func_Call)) = N_Explicit_Dereference then
         Func_Id := Etype (Name (Func_Call));

      else
         raise Program_Error;
      end if;

      Result_Subt := Etype (Func_Id);

      --  When the result subtype is unconstrained, an additional actual must
      --  be passed to indicate that the caller is providing the return object.
      --  This parameter must also be passed when the called function has a
      --  controlling result, because dispatching calls to the function needs
      --  to be treated effectively the same as calls to class-wide functions.

      Add_Alloc_Form_Actual_To_Build_In_Place_Call
        (Func_Call, Func_Id, Alloc_Form => Caller_Allocation);

      --  If Lhs is a selected component, then pass it along so that its prefix
      --  object will be used as the source of the finalization list.

      if Nkind (Lhs) = N_Selected_Component then
         Add_Final_List_Actual_To_Build_In_Place_Call
           (Func_Call, Func_Id, Acc_Type => Empty, Sel_Comp => Lhs);
      else
         Add_Final_List_Actual_To_Build_In_Place_Call
           (Func_Call, Func_Id, Acc_Type => Empty);
      end if;

      Add_Task_Actuals_To_Build_In_Place_Call
        (Func_Call, Func_Id, Make_Identifier (Loc, Name_uMaster));

      --  Add an implicit actual to the function call that provides access to
      --  the caller's return object.

      Add_Access_Actual_To_Build_In_Place_Call
        (Func_Call,
         Func_Id,
         Make_Unchecked_Type_Conversion (Loc,
           Subtype_Mark => New_Reference_To (Result_Subt, Loc),
           Expression   => Relocate_Node (Lhs)));

      --  Create an access type designating the function's result subtype

      Ptr_Typ := Make_Temporary (Loc, 'A');

      Ptr_Typ_Decl :=
        Make_Full_Type_Declaration (Loc,
          Defining_Identifier => Ptr_Typ,
          Type_Definition =>
            Make_Access_To_Object_Definition (Loc,
              All_Present => True,
              Subtype_Indication =>
                New_Reference_To (Result_Subt, Loc)));
      Insert_After_And_Analyze (Assign, Ptr_Typ_Decl);

      --  Finally, create an access object initialized to a reference to the
      --  function call.

      Obj_Id := Make_Temporary (Loc, 'R');
      Set_Etype (Obj_Id, Ptr_Typ);

      Obj_Decl :=
        Make_Object_Declaration (Loc,
          Defining_Identifier => Obj_Id,
          Object_Definition =>
            New_Reference_To (Ptr_Typ, Loc),
          Expression =>
            Make_Reference (Loc,
              Prefix => Relocate_Node (Func_Call)));
      Insert_After_And_Analyze (Ptr_Typ_Decl, Obj_Decl);

      Rewrite (Assign, Make_Null_Statement (Loc));

      --  Retrieve the target of the assignment

      if Nkind (Lhs) = N_Selected_Component then
         Target := Selector_Name (Lhs);
      elsif Nkind (Lhs) = N_Type_Conversion then
         Target := Expression (Lhs);
      else
         Target := Lhs;
      end if;

      --  If we are assigning to a return object or this is an expression of
      --  an extension aggregate, the target should either be an identifier
      --  or a simple expression. All other cases imply a different scenario.

      if Nkind (Target) in N_Has_Entity then
         Target := Entity (Target);
      else
         return;
      end if;

      --  When the target of the assignment is a return object of an enclosing
      --  build-in-place function and also requires finalization, the list
      --  generated for the assignment must be moved to that of the enclosing
      --  function.

      --    function Enclosing_BIP_Function return Ctrl_Typ is
      --    begin
      --       return (Ctrl_Parent_Part => BIP_Function with ...);
      --    end Enclosing_BIP_Function;

      if Is_Return_Object (Target)
        and then Needs_Finalization (Etype (Target))
        and then Needs_Finalization (Result_Subt)
      then
         declare
            Obj_List  : constant Node_Id := Find_Final_List (Obj_Id);
            Encl_List : Node_Id;
            Encl_Scop : Entity_Id;

         begin
            Encl_Scop := Scope (Target);

            --  Locate the scope of the extended return statement

            while Present (Encl_Scop)
              and then Ekind (Encl_Scop) /= E_Return_Statement
            loop
               Encl_Scop := Scope (Encl_Scop);
            end loop;

            --  A return object should always be enclosed by a return statement
            --  scope at some level.

            pragma Assert (Present (Encl_Scop));

            Encl_List :=
              Make_Attribute_Reference (Loc,
                Prefix =>
                  New_Reference_To (
                    Finalization_Chain_Entity (Encl_Scop), Loc),
                Attribute_Name => Name_Unrestricted_Access);

            --  Generate a call to move final list

            Insert_After_And_Analyze (Obj_Decl,
              Make_Procedure_Call_Statement (Loc,
                Name =>
                  New_Reference_To (RTE (RE_Move_Final_List), Loc),
                Parameter_Associations => New_List (Obj_List, Encl_List)));
         end;
      end if;
   end Make_Build_In_Place_Call_In_Assignment;

   ----------------------------------------------------
   -- Make_Build_In_Place_Call_In_Object_Declaration --
   ----------------------------------------------------

   procedure Make_Build_In_Place_Call_In_Object_Declaration
     (Object_Decl   : Node_Id;
      Function_Call : Node_Id)
   is
      Loc             : Source_Ptr;
      Obj_Def_Id      : constant Entity_Id :=
                          Defining_Identifier (Object_Decl);

      Func_Call       : Node_Id := Function_Call;
      Function_Id     : Entity_Id;
      Result_Subt     : Entity_Id;
      Caller_Object   : Node_Id;
      Call_Deref      : Node_Id;
      Ref_Type        : Entity_Id;
      Ptr_Typ_Decl    : Node_Id;
      Def_Id          : Entity_Id;
      New_Expr        : Node_Id;
      Enclosing_Func  : Entity_Id;
      Pass_Caller_Acc : Boolean := False;

   begin
      --  Step past qualification or unchecked conversion (the latter can occur
      --  in cases of calls to 'Input).

      if Nkind_In (Func_Call, N_Qualified_Expression,
                              N_Unchecked_Type_Conversion)
      then
         Func_Call := Expression (Func_Call);
      end if;

      --  If the call has already been processed to add build-in-place actuals
      --  then return. This should not normally occur in an object declaration,
      --  but we add the protection as a defensive measure.

      if Is_Expanded_Build_In_Place_Call (Func_Call) then
         return;
      end if;

      --  Mark the call as processed as a build-in-place call

      Set_Is_Expanded_Build_In_Place_Call (Func_Call);

      Loc := Sloc (Function_Call);

      if Is_Entity_Name (Name (Func_Call)) then
         Function_Id := Entity (Name (Func_Call));

      elsif Nkind (Name (Func_Call)) = N_Explicit_Dereference then
         Function_Id := Etype (Name (Func_Call));

      else
         raise Program_Error;
      end if;

      Result_Subt := Etype (Function_Id);

      --  In the constrained case, add an implicit actual to the function call
      --  that provides access to the declared object. An unchecked conversion
      --  to the (specific) result type of the function is inserted to handle
      --  the case where the object is declared with a class-wide type.

      if Is_Constrained (Underlying_Type (Result_Subt)) then
         Caller_Object :=
            Make_Unchecked_Type_Conversion (Loc,
              Subtype_Mark => New_Reference_To (Result_Subt, Loc),
              Expression   => New_Reference_To (Obj_Def_Id, Loc));

         --  When the function has a controlling result, an allocation-form
         --  parameter must be passed indicating that the caller is allocating
         --  the result object. This is needed because such a function can be
         --  called as a dispatching operation and must be treated similarly
         --  to functions with unconstrained result subtypes.

         Add_Alloc_Form_Actual_To_Build_In_Place_Call
           (Func_Call, Function_Id, Alloc_Form => Caller_Allocation);

      --  If the function's result subtype is unconstrained and the object is
      --  a return object of an enclosing build-in-place function, then the
      --  implicit build-in-place parameters of the enclosing function must be
      --  passed along to the called function. (Unfortunately, this won't cover
      --  the case of extension aggregates where the ancestor part is a build-
      --  in-place unconstrained function call that should be passed along the
      --  caller's parameters. Currently those get mishandled by reassigning
      --  the result of the call to the aggregate return object, when the call
      --  result should really be directly built in place in the aggregate and
      --  not built in a temporary. ???)

      elsif Is_Return_Object (Defining_Identifier (Object_Decl)) then
         Pass_Caller_Acc := True;

         Enclosing_Func := Enclosing_Subprogram (Obj_Def_Id);

         --  If the enclosing function has a constrained result type, then
         --  caller allocation will be used.

         if Is_Constrained (Etype (Enclosing_Func)) then
            Add_Alloc_Form_Actual_To_Build_In_Place_Call
              (Func_Call, Function_Id, Alloc_Form => Caller_Allocation);

         --  Otherwise, when the enclosing function has an unconstrained result
         --  type, the BIP_Alloc_Form formal of the enclosing function must be
         --  passed along to the callee.

         else
            Add_Alloc_Form_Actual_To_Build_In_Place_Call
              (Func_Call,
               Function_Id,
               Alloc_Form_Exp =>
                 New_Reference_To
                   (Build_In_Place_Formal (Enclosing_Func, BIP_Alloc_Form),
                    Loc));
         end if;

         --  Retrieve the BIPacc formal from the enclosing function and convert
         --  it to the access type of the callee's BIP_Object_Access formal.

         Caller_Object :=
            Make_Unchecked_Type_Conversion (Loc,
              Subtype_Mark =>
                New_Reference_To
                  (Etype
                     (Build_In_Place_Formal (Function_Id, BIP_Object_Access)),
                   Loc),
              Expression   =>
                New_Reference_To
                  (Build_In_Place_Formal (Enclosing_Func, BIP_Object_Access),
                   Loc));

      --  In other unconstrained cases, pass an indication to do the allocation
      --  on the secondary stack and set Caller_Object to Empty so that a null
      --  value will be passed for the caller's object address. A transient
      --  scope is established to ensure eventual cleanup of the result.

      else
         Add_Alloc_Form_Actual_To_Build_In_Place_Call
           (Func_Call,
            Function_Id,
            Alloc_Form => Secondary_Stack);
         Caller_Object := Empty;

         Establish_Transient_Scope (Object_Decl, Sec_Stack => True);
      end if;

      Add_Final_List_Actual_To_Build_In_Place_Call
        (Func_Call, Function_Id, Acc_Type => Empty);

      if Nkind (Parent (Object_Decl)) = N_Extended_Return_Statement
        and then Has_Task (Result_Subt)
      then
         Enclosing_Func := Enclosing_Subprogram (Obj_Def_Id);

         --  Here we're passing along the master that was passed in to this
         --  function.

         Add_Task_Actuals_To_Build_In_Place_Call
           (Func_Call, Function_Id,
            Master_Actual =>
              New_Reference_To
                (Build_In_Place_Formal (Enclosing_Func, BIP_Master), Loc));

      else
         Add_Task_Actuals_To_Build_In_Place_Call
           (Func_Call, Function_Id, Make_Identifier (Loc, Name_uMaster));
      end if;

      Add_Access_Actual_To_Build_In_Place_Call
        (Func_Call, Function_Id, Caller_Object, Is_Access => Pass_Caller_Acc);

      --  Create an access type designating the function's result subtype. We
      --  use the type of the original expression because it may be a call to
      --  an inherited operation, which the expansion has replaced with the
      --  parent operation that yields the parent type.

      Ref_Type := Make_Temporary (Loc, 'A');

      Ptr_Typ_Decl :=
        Make_Full_Type_Declaration (Loc,
          Defining_Identifier => Ref_Type,
          Type_Definition =>
            Make_Access_To_Object_Definition (Loc,
              All_Present => True,
              Subtype_Indication =>
                New_Reference_To (Etype (Function_Call), Loc)));

      --  The access type and its accompanying object must be inserted after
      --  the object declaration in the constrained case, so that the function
      --  call can be passed access to the object. In the unconstrained case,
      --  the access type and object must be inserted before the object, since
      --  the object declaration is rewritten to be a renaming of a dereference
      --  of the access object.

      if Is_Constrained (Underlying_Type (Result_Subt)) then
         Insert_After_And_Analyze (Object_Decl, Ptr_Typ_Decl);
      else
         Insert_Action (Object_Decl, Ptr_Typ_Decl);
      end if;

      --  Finally, create an access object initialized to a reference to the
      --  function call.

      New_Expr :=
        Make_Reference (Loc,
          Prefix => Relocate_Node (Func_Call));

      Def_Id := Make_Temporary (Loc, 'R', New_Expr);
      Set_Etype (Def_Id, Ref_Type);

      Insert_After_And_Analyze (Ptr_Typ_Decl,
        Make_Object_Declaration (Loc,
          Defining_Identifier => Def_Id,
          Object_Definition   => New_Reference_To (Ref_Type, Loc),
          Expression          => New_Expr));

      if Is_Constrained (Underlying_Type (Result_Subt)) then
         Set_Expression (Object_Decl, Empty);
         Set_No_Initialization (Object_Decl);

      --  In case of an unconstrained result subtype, rewrite the object
      --  declaration as an object renaming where the renamed object is a
      --  dereference of <function_Call>'reference:
      --
      --      Obj : Subt renames <function_call>'Ref.all;

      else
         Call_Deref :=
           Make_Explicit_Dereference (Loc,
             Prefix => New_Reference_To (Def_Id, Loc));

         Loc := Sloc (Object_Decl);
         Rewrite (Object_Decl,
           Make_Object_Renaming_Declaration (Loc,
             Defining_Identifier => Make_Temporary (Loc, 'D'),
             Access_Definition   => Empty,
             Subtype_Mark        => New_Occurrence_Of (Result_Subt, Loc),
             Name                => Call_Deref));

         Set_Renamed_Object (Defining_Identifier (Object_Decl), Call_Deref);

         Analyze (Object_Decl);

         --  Replace the internal identifier of the renaming declaration's
         --  entity with identifier of the original object entity. We also have
         --  to exchange the entities containing their defining identifiers to
         --  ensure the correct replacement of the object declaration by the
         --  object renaming declaration to avoid homograph conflicts (since
         --  the object declaration's defining identifier was already entered
         --  in current scope). The Next_Entity links of the two entities also
         --  have to be swapped since the entities are part of the return
         --  scope's entity list and the list structure would otherwise be
         --  corrupted. Finally, the homonym chain must be preserved as well.

         declare
            Renaming_Def_Id  : constant Entity_Id :=
                                 Defining_Identifier (Object_Decl);
            Next_Entity_Temp : constant Entity_Id :=
                                 Next_Entity (Renaming_Def_Id);
         begin
            Set_Chars (Renaming_Def_Id, Chars (Obj_Def_Id));

            --  Swap next entity links in preparation for exchanging entities

            Set_Next_Entity (Renaming_Def_Id, Next_Entity (Obj_Def_Id));
            Set_Next_Entity (Obj_Def_Id, Next_Entity_Temp);
            Set_Homonym     (Renaming_Def_Id, Homonym (Obj_Def_Id));

            Exchange_Entities (Renaming_Def_Id, Obj_Def_Id);

            --  Preserve source indication of original declaration, so that
            --  xref information is properly generated for the right entity.

            Preserve_Comes_From_Source
              (Object_Decl, Original_Node (Object_Decl));
            Set_Comes_From_Source (Obj_Def_Id, True);
            Set_Comes_From_Source (Renaming_Def_Id, False);
         end;
      end if;

      --  If the object entity has a class-wide Etype, then we need to change
      --  it to the result subtype of the function call, because otherwise the
      --  object will be class-wide without an explicit initialization and
      --  won't be allocated properly by the back end. It seems unclean to make
      --  such a revision to the type at this point, and we should try to
      --  improve this treatment when build-in-place functions with class-wide
      --  results are implemented. ???

      if Is_Class_Wide_Type (Etype (Defining_Identifier (Object_Decl))) then
         Set_Etype (Defining_Identifier (Object_Decl), Result_Subt);
      end if;
   end Make_Build_In_Place_Call_In_Object_Declaration;

   --------------------------
   -- Needs_BIP_Final_List --
   --------------------------

   function Needs_BIP_Final_List (E : Entity_Id) return Boolean is
      pragma Assert (Is_Build_In_Place_Function (E));
      Result_Subt : constant Entity_Id := Underlying_Type (Etype (E));

   begin
      --  We need the BIP_Final_List if the result type needs finalization. We
      --  also need it for tagged types, even if not class-wide, because some
      --  type extension might need finalization, and all overriding functions
      --  must have the same calling conventions. However, if there is a
      --  pragma Restrictions (No_Finalization), we never need this parameter.

      return (Needs_Finalization (Result_Subt)
               or else Is_Tagged_Type (Underlying_Type (Result_Subt)))
        and then not Restriction_Active (No_Finalization);
   end Needs_BIP_Final_List;

end Exp_Ch6;
