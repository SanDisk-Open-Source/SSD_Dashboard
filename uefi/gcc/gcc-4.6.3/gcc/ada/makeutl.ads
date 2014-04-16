------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                              M A K E U T L                               --
--                                                                          --
--                                 S p e c                                  --
--                                                                          --
--          Copyright (C) 2004-2010, Free Software Foundation, Inc.         --
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

with ALI;
with Namet; use Namet;
with Opt;
with Osint;
with Prj;   use Prj;
with Prj.Tree;
with Types; use Types;

with GNAT.OS_Lib; use GNAT.OS_Lib;

package Makeutl is

   type Fail_Proc is access procedure (S : String);
   Do_Fail : Fail_Proc := Osint.Fail'Access;
   --  Failing procedure called from procedure Test_If_Relative_Path below. May
   --  be redirected.

   Project_Tree : constant Project_Tree_Ref := new Project_Tree_Data;
   --  The project tree

   Source_Info_Option : constant String := "--source-info=";
   --  Switch to indicate the source info file

   Subdirs_Option : constant String := "--subdirs=";
   --  Switch used to indicate that the real directories (object, exec,
   --  library, ...) are subdirectories of those in the project file.

   Unchecked_Shared_Lib_Imports : constant String :=
                                    "--unchecked-shared-lib-imports";
   --  Command line switch to allow shared library projects to import projects
   --  that are not shared library projects.

   Single_Compile_Per_Obj_Dir_Switch : constant String :=
                                         "--single-compile-per-obj-dir";
   --  Switch to forbid simultaneous compilations for the same object directory
   --  when project files are used.

   Create_Map_File_Switch : constant String := "--create-map-file";
   --  Switch to create a map file when an executable is linked

   procedure Add
     (Option : String_Access;
      To     : in out String_List_Access;
      Last   : in out Natural);
   procedure Add
     (Option : String;
      To     : in out String_List_Access;
      Last   : in out Natural);
   --  Add a string to a list of strings

   function Create_Binder_Mapping_File return Path_Name_Type;
   --  Create a binder mapping file and returns its path name

   function Create_Name (Name : String) return File_Name_Type;
   function Create_Name (Name : String) return Name_Id;
   function Create_Name (Name : String) return Path_Name_Type;
   --  Get an id for a name

   function Base_Name_Index_For
     (Main            : String;
      Main_Index      : Int;
      Index_Separator : Character) return File_Name_Type;
   --  Returns the base name of Main, without the extension, followed by the
   --  Index_Separator followed by the Main_Index if it is non-zero.

   function Executable_Prefix_Path return String;
   --  Return the absolute path parent directory of the directory where the
   --  current executable resides, if its directory is named "bin", otherwise
   --  return an empty string. When a directory is returned, it is guaranteed
   --  to end with a directory separator.

   procedure Inform (N : Name_Id := No_Name; Msg : String);
   procedure Inform (N : File_Name_Type; Msg : String);
   --  Prints out the program name followed by a colon, N and S

   function File_Not_A_Source_Of
     (Uname : Name_Id;
      Sfile : File_Name_Type) return Boolean;
   --  Check that file name Sfile is one of the source of unit Uname. Returns
   --  True if the unit is in one of the project file, but the file name is not
   --  one of its source. Returns False otherwise.

   function Check_Source_Info_In_ALI
     (The_ALI : ALI.ALI_Id;
      Tree    : Project_Tree_Ref) return Boolean;
   --  Check whether all file references in ALI are still valid (i.e. the
   --  source files are still associated with the same units). Return True
   --  if everything is still valid.

   function Is_External_Assignment
     (Tree : Prj.Tree.Project_Node_Tree_Ref;
      Argv : String) return Boolean;
   --  Verify that an external assignment switch is syntactically correct
   --
   --  Correct forms are:
   --
   --      -Xname=value
   --      -X"name=other value"
   --
   --  Assumptions: 'First = 1, Argv (1 .. 2) = "-X"
   --
   --  When this function returns True, the external assignment has been
   --  entered by a call to Prj.Ext.Add, so that in a project file, External
   --  ("name") will return "value".

   procedure Verbose_Msg
     (N1                : Name_Id;
      S1                : String;
      N2                : Name_Id := No_Name;
      S2                : String  := "";
      Prefix            : String  := "  -> ";
      Minimum_Verbosity : Opt.Verbosity_Level_Type := Opt.Low);
   procedure Verbose_Msg
     (N1                : File_Name_Type;
      S1                : String;
      N2                : File_Name_Type := No_File;
      S2                : String  := "";
      Prefix            : String  := "  -> ";
      Minimum_Verbosity : Opt.Verbosity_Level_Type := Opt.Low);
   --  If the verbose flag (Verbose_Mode) is set and the verbosity level is at
   --  least equal to Minimum_Verbosity, then print Prefix to standard output
   --  followed by N1 and S1. If N2 /= No_Name then N2 is printed after S1. S2
   --  is printed last. Both N1 and N2 are printed in quotation marks. The two
   --  forms differ only in taking Name_Id or File_name_Type arguments.

   function Linker_Options_Switches
     (Project  : Project_Id;
      In_Tree  : Project_Tree_Ref) return String_List;
   --  Collect the options specified in the Linker'Linker_Options attributes
   --  of project Project, in project tree In_Tree, and in the projects that
   --  it imports directly or indirectly, and returns the result.

   --  Package Mains is used to store the mains specified on the command line
   --  and to retrieve them when a project file is used, to verify that the
   --  files exist and that they belong to a project file.

   function Unit_Index_Of (ALI_File : File_Name_Type) return Int;
   --  Find the index of a unit in a source file. Return zero if the file is
   --  not a multi-unit source file.

   procedure Test_If_Relative_Path
     (Switch               : in out String_Access;
      Parent               : String;
      Including_L_Switch   : Boolean := True;
      Including_Non_Switch : Boolean := True;
      Including_RTS        : Boolean := False);
   --  Test if Switch is a relative search path switch. If it is, fail if
   --  Parent is the empty string, otherwise prepend the path with Parent.
   --  This subprogram is only called when using project files. For gnatbind
   --  switches, Including_L_Switch is False, because the argument of the -L
   --  switch is not a path. If Including_RTS is True, process also switches
   --  --RTS=.

   function Path_Or_File_Name (Path : Path_Name_Type) return String;
   --  Returns a file name if -df is used, otherwise return a path name

   -----------
   -- Mains --
   -----------

   --  Mains are stored in a table. An index is used to retrieve the mains
   --  from the table.

   package Mains is

      procedure Add_Main (Name : String);
      --  Add one main to the table

      procedure Set_Index (Index : Int);

      procedure Set_Location (Location : Source_Ptr);
      --  Set the location of the last main added. By default, the location is
      --  No_Location.

      procedure Delete;
      --  Empty the table

      procedure Reset;
      --  Reset the index to the beginning of the table

      function Next_Main return String;
      --  Increase the index and return the next main. If table is exhausted,
      --  return an empty string.

      function Get_Index return Int;

      function Get_Location return Source_Ptr;
      --  Get the location of the current main

      procedure Update_Main (Name : String);
      --  Update the file name of the current main

      function Number_Of_Mains return Natural;
      --  Returns the number of mains added with Add_Main since the last call
      --  to Delete.

   end Mains;

   ----------------------
   -- Marking Routines --
   ----------------------

   procedure Mark (Source_File : File_Name_Type; Index : Int := 0);
   --  Mark a unit, identified by its source file and, when Index is not 0, the
   --  index of the unit in the source file. Marking is used to signal that the
   --  unit has already been inserted in the Q.

   function Is_Marked
     (Source_File : File_Name_Type;
      Index       : Int := 0) return Boolean;
   --  Returns True if the unit was previously marked

   procedure Delete_All_Marks;
   --  Remove all file/index couples marked

end Makeutl;
