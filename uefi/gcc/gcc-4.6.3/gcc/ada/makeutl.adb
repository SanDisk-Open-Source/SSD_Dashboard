------------------------------------------------------------------------------
--                                                                          --
--                         GNAT COMPILER COMPONENTS                         --
--                                                                          --
--                              M A K E U T L                               --
--                                                                          --
--                                 B o d y                                  --
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

with ALI;      use ALI;
with Debug;
with Fname;
with Hostparm;
with Osint;    use Osint;
with Output;   use Output;
with Opt;      use Opt;
with Prj.Ext;
with Prj.Util;
with Snames;   use Snames;
with Table;
with Tempdir;

with Ada.Command_Line;  use Ada.Command_Line;

with GNAT.Case_Util;            use GNAT.Case_Util;
with GNAT.Directory_Operations; use GNAT.Directory_Operations;
with GNAT.HTable;

package body Makeutl is

   type Mark_Key is record
      File  : File_Name_Type;
      Index : Int;
   end record;
   --  Identify either a mono-unit source (when Index = 0) or a specific unit
   --  (index = 1's origin index of unit) in a multi-unit source.

   --  There follow many global undocumented declarations, comments needed ???

   Max_Mask_Num : constant := 2048;

   subtype Mark_Num is Union_Id range 0 .. Max_Mask_Num - 1;

   function Hash (Key : Mark_Key) return Mark_Num;

   package Marks is new GNAT.HTable.Simple_HTable
     (Header_Num => Mark_Num,
      Element    => Boolean,
      No_Element => False,
      Key        => Mark_Key,
      Hash       => Hash,
      Equal      => "=");
   --  A hash table to keep tracks of the marked units

   type Linker_Options_Data is record
      Project : Project_Id;
      Options : String_List_Id;
   end record;

   Linker_Option_Initial_Count : constant := 20;

   Linker_Options_Buffer : String_List_Access :=
     new String_List (1 .. Linker_Option_Initial_Count);

   Last_Linker_Option : Natural := 0;

   package Linker_Opts is new Table.Table (
     Table_Component_Type => Linker_Options_Data,
     Table_Index_Type     => Integer,
     Table_Low_Bound      => 1,
     Table_Initial        => 10,
     Table_Increment      => 100,
     Table_Name           => "Make.Linker_Opts");

   procedure Add_Linker_Option (Option : String);

   ---------
   -- Add --
   ---------

   procedure Add
     (Option : String_Access;
      To     : in out String_List_Access;
      Last   : in out Natural)
   is
   begin
      if Last = To'Last then
         declare
            New_Options : constant String_List_Access :=
                            new String_List (1 .. To'Last * 2);

         begin
            New_Options (To'Range) := To.all;

            --  Set all elements of the original options to null to avoid
            --  deallocation of copies.

            To.all := (others => null);

            Free (To);
            To := New_Options;
         end;
      end if;

      Last := Last + 1;
      To (Last) := Option;
   end Add;

   procedure Add
     (Option : String;
      To     : in out String_List_Access;
      Last   : in out Natural)
   is
   begin
      Add (Option => new String'(Option), To => To, Last => Last);
   end Add;

   -----------------------
   -- Add_Linker_Option --
   -----------------------

   procedure Add_Linker_Option (Option : String) is
   begin
      if Option'Length > 0 then
         if Last_Linker_Option = Linker_Options_Buffer'Last then
            declare
               New_Buffer : constant String_List_Access :=
                              new String_List
                                (1 .. Linker_Options_Buffer'Last +
                                        Linker_Option_Initial_Count);
            begin
               New_Buffer (Linker_Options_Buffer'Range) :=
                 Linker_Options_Buffer.all;
               Linker_Options_Buffer.all := (others => null);
               Free (Linker_Options_Buffer);
               Linker_Options_Buffer := New_Buffer;
            end;
         end if;

         Last_Linker_Option := Last_Linker_Option + 1;
         Linker_Options_Buffer (Last_Linker_Option) := new String'(Option);
      end if;
   end Add_Linker_Option;

   -------------------------
   -- Base_Name_Index_For --
   -------------------------

   function Base_Name_Index_For
     (Main            : String;
      Main_Index      : Int;
      Index_Separator : Character) return File_Name_Type
   is
      Result : File_Name_Type;

   begin
      Name_Len := 0;
      Add_Str_To_Name_Buffer (Base_Name (Main));

      --  Remove the extension, if any, that is the last part of the base name
      --  starting with a dot and following some characters.

      for J in reverse 2 .. Name_Len loop
         if Name_Buffer (J) = '.' then
            Name_Len := J - 1;
            exit;
         end if;
      end loop;

      --  Add the index info, if index is different from 0

      if Main_Index > 0 then
         Add_Char_To_Name_Buffer (Index_Separator);

         declare
            Img : constant String := Main_Index'Img;
         begin
            Add_Str_To_Name_Buffer (Img (2 .. Img'Last));
         end;
      end if;

      Result := Name_Find;
      return Result;
   end Base_Name_Index_For;

   ------------------------------
   -- Check_Source_Info_In_ALI --
   ------------------------------

   function Check_Source_Info_In_ALI
     (The_ALI : ALI_Id;
      Tree    : Project_Tree_Ref) return Boolean
   is
      Unit_Name : Name_Id;

   begin
      --  Loop through units

      for U in ALIs.Table (The_ALI).First_Unit ..
               ALIs.Table (The_ALI).Last_Unit
      loop
         --  Check if the file name is one of the source of the unit

         Get_Name_String (Units.Table (U).Uname);
         Name_Len  := Name_Len - 2;
         Unit_Name := Name_Find;

         if File_Not_A_Source_Of (Unit_Name, Units.Table (U).Sfile) then
            return False;
         end if;

         --  Loop to do same check for each of the withed units

         for W in Units.Table (U).First_With .. Units.Table (U).Last_With loop
            declare
               WR : ALI.With_Record renames Withs.Table (W);

            begin
               if WR.Sfile /= No_File then
                  Get_Name_String (WR.Uname);
                  Name_Len  := Name_Len - 2;
                  Unit_Name := Name_Find;

                  if File_Not_A_Source_Of (Unit_Name, WR.Sfile) then
                     return False;
                  end if;
               end if;
            end;
         end loop;
      end loop;

      --  Loop to check subunits and replaced sources

      for D in ALIs.Table (The_ALI).First_Sdep ..
               ALIs.Table (The_ALI).Last_Sdep
      loop
         declare
            SD : Sdep_Record renames Sdep.Table (D);

         begin
            Unit_Name := SD.Subunit_Name;

            if Unit_Name = No_Name then
               --  Check if this source file has been replaced by a source with
               --  a different file name.

               if Tree /= null and then Tree.Replaced_Source_Number > 0 then
                  declare
                     Replacement : constant File_Name_Type :=
                       Replaced_Source_HTable.Get
                         (Tree.Replaced_Sources, SD.Sfile);

                  begin
                     if Replacement /= No_File then
                        if Verbose_Mode then
                           Write_Line
                             ("source file" &
                              Get_Name_String (SD.Sfile) &
                              " has been replaced by " &
                              Get_Name_String (Replacement));
                        end if;

                        return False;
                     end if;
                  end;
               end if;

            else
               --  For separates, the file is no longer associated with the
               --  unit ("proc-sep.adb" is not associated with unit "proc.sep")
               --  so we need to check whether the source file still exists in
               --  the source tree: it will if it matches the naming scheme
               --  (and then will be for the same unit).

               if Find_Source
                    (In_Tree   => Project_Tree,
                     Project   => No_Project,
                     Base_Name => SD.Sfile) = No_Source
               then
                  --  If this is not a runtime file or if, when gnatmake switch
                  --  -a is used, we are not able to find this subunit in the
                  --  source directories, then recompilation is needed.

                  if not Fname.Is_Internal_File_Name (SD.Sfile)
                    or else
                      (Check_Readonly_Files
                        and then Full_Source_Name (SD.Sfile) = No_File)
                  then
                     if Verbose_Mode then
                        Write_Line
                          ("While parsing ALI file, file "
                           & Get_Name_String (SD.Sfile)
                           & " is indicated as containing subunit "
                           & Get_Name_String (Unit_Name)
                           & " but this does not match what was found while"
                           & " parsing the project. Will recompile");
                     end if;

                     return False;
                  end if;
               end if;
            end if;
         end;
      end loop;

      return True;
   end Check_Source_Info_In_ALI;

   --------------------------------
   -- Create_Binder_Mapping_File --
   --------------------------------

   function Create_Binder_Mapping_File return Path_Name_Type is
      Mapping_Path : Path_Name_Type := No_Path;

      Mapping_FD : File_Descriptor := Invalid_FD;
      --  A File Descriptor for an eventual mapping file

      ALI_Unit : Unit_Name_Type := No_Unit_Name;
      --  The unit name of an ALI file

      ALI_Name : File_Name_Type := No_File;
      --  The file name of the ALI file

      ALI_Project : Project_Id := No_Project;
      --  The project of the ALI file

      Bytes : Integer;
      OK    : Boolean := False;
      Unit  : Unit_Index;

      Status : Boolean;
      --  For call to Close

   begin
      Tempdir.Create_Temp_File (Mapping_FD, Mapping_Path);
      Record_Temp_File (Project_Tree, Mapping_Path);

      if Mapping_FD /= Invalid_FD then
         OK := True;

         --  Traverse all units

         Unit := Units_Htable.Get_First (Project_Tree.Units_HT);
         while Unit /= No_Unit_Index loop
            if Unit.Name /= No_Name then

               --  If there is a body, put it in the mapping

               if Unit.File_Names (Impl) /= No_Source
                 and then Unit.File_Names (Impl).Project /= No_Project
               then
                  Get_Name_String (Unit.Name);
                  Add_Str_To_Name_Buffer ("%b");
                  ALI_Unit := Name_Find;
                  ALI_Name :=
                    Lib_File_Name (Unit.File_Names (Impl).Display_File);
                  ALI_Project := Unit.File_Names (Impl).Project;

                  --  Otherwise, if there is a spec, put it in the mapping

               elsif Unit.File_Names (Spec) /= No_Source
                 and then Unit.File_Names (Spec).Project /= No_Project
               then
                  Get_Name_String (Unit.Name);
                  Add_Str_To_Name_Buffer ("%s");
                  ALI_Unit := Name_Find;
                  ALI_Name :=
                    Lib_File_Name (Unit.File_Names (Spec).Display_File);
                  ALI_Project := Unit.File_Names (Spec).Project;

               else
                  ALI_Name := No_File;
               end if;

               --  If we have something to put in the mapping then do it now.
               --  However, if the project is extended, we don't put anything
               --  in the mapping file, since we don't know where the ALI file
               --  is: it might be in the extended project object directory as
               --  well as in the extending project object directory.

               if ALI_Name /= No_File
                 and then ALI_Project.Extended_By = No_Project
                 and then ALI_Project.Extends = No_Project
               then
                  --  First check if the ALI file exists. If it does not, do
                  --  not put the unit in the mapping file.

                  declare
                     ALI : constant String := Get_Name_String (ALI_Name);

                  begin
                     --  For library projects, use the library ALI directory,
                     --  for other projects, use the object directory.

                     if ALI_Project.Library then
                        Get_Name_String
                          (ALI_Project.Library_ALI_Dir.Display_Name);
                     else
                        Get_Name_String
                          (ALI_Project.Object_Directory.Display_Name);
                     end if;

                     if not
                       Is_Directory_Separator (Name_Buffer (Name_Len))
                     then
                        Add_Char_To_Name_Buffer (Directory_Separator);
                     end if;

                     Add_Str_To_Name_Buffer (ALI);
                     Add_Char_To_Name_Buffer (ASCII.LF);

                     declare
                        ALI_Path_Name : constant String :=
                          Name_Buffer (1 .. Name_Len);

                     begin
                        if Is_Regular_File
                             (ALI_Path_Name (1 .. ALI_Path_Name'Last - 1))
                        then
                           --  First line is the unit name

                           Get_Name_String (ALI_Unit);
                           Add_Char_To_Name_Buffer (ASCII.LF);
                           Bytes :=
                             Write
                               (Mapping_FD,
                                Name_Buffer (1)'Address,
                                Name_Len);
                           OK := Bytes = Name_Len;

                           exit when not OK;

                           --  Second line it the ALI file name

                           Get_Name_String (ALI_Name);
                           Add_Char_To_Name_Buffer (ASCII.LF);
                           Bytes :=
                             Write
                               (Mapping_FD,
                                Name_Buffer (1)'Address,
                                Name_Len);
                           OK := (Bytes = Name_Len);

                           exit when not OK;

                           --  Third line it the ALI path name

                           Bytes :=
                             Write
                               (Mapping_FD,
                                ALI_Path_Name (1)'Address,
                                ALI_Path_Name'Length);
                           OK := (Bytes = ALI_Path_Name'Length);

                           --  If OK is False, it means we were unable to
                           --  write a line. No point in continuing with the
                           --  other units.

                           exit when not OK;
                        end if;
                     end;
                  end;
               end if;
            end if;

            Unit := Units_Htable.Get_Next (Project_Tree.Units_HT);
         end loop;

         Close (Mapping_FD, Status);

         OK := OK and Status;
      end if;

      --  If the creation of the mapping file was successful, we add the switch
      --  to the arguments of gnatbind.

      if OK then
         return Mapping_Path;

      else
         return No_Path;
      end if;
   end Create_Binder_Mapping_File;

   -----------------
   -- Create_Name --
   -----------------

   function Create_Name (Name : String) return File_Name_Type is
   begin
      Name_Len := 0;
      Add_Str_To_Name_Buffer (Name);
      return Name_Find;
   end Create_Name;

   function Create_Name (Name : String) return Name_Id is
   begin
      Name_Len := 0;
      Add_Str_To_Name_Buffer (Name);
      return Name_Find;
   end Create_Name;

   function Create_Name (Name : String) return Path_Name_Type is
   begin
      Name_Len := 0;
      Add_Str_To_Name_Buffer (Name);
      return Name_Find;
   end Create_Name;

   ----------------------
   -- Delete_All_Marks --
   ----------------------

   procedure Delete_All_Marks is
   begin
      Marks.Reset;
   end Delete_All_Marks;

   ----------------------------
   -- Executable_Prefix_Path --
   ----------------------------

   function Executable_Prefix_Path return String is
      Exec_Name : constant String := Command_Name;

      function Get_Install_Dir (S : String) return String;
      --  S is the executable name preceded by the absolute or relative path,
      --  e.g. "c:\usr\bin\gcc.exe". Returns the absolute directory where "bin"
      --  lies (in the example "C:\usr"). If the executable is not in a "bin"
      --  directory, return "".

      ---------------------
      -- Get_Install_Dir --
      ---------------------

      function Get_Install_Dir (S : String) return String is
         Exec      : String  := S;
         Path_Last : Integer := 0;

      begin
         for J in reverse Exec'Range loop
            if Exec (J) = Directory_Separator then
               Path_Last := J - 1;
               exit;
            end if;
         end loop;

         if Path_Last >= Exec'First + 2 then
            To_Lower (Exec (Path_Last - 2 .. Path_Last));
         end if;

         if Path_Last < Exec'First + 2
           or else Exec (Path_Last - 2 .. Path_Last) /= "bin"
           or else (Path_Last - 3 >= Exec'First
                     and then Exec (Path_Last - 3) /= Directory_Separator)
         then
            return "";
         end if;

         return Normalize_Pathname
                  (Exec (Exec'First .. Path_Last - 4),
                   Resolve_Links => Opt.Follow_Links_For_Dirs)
           & Directory_Separator;
      end Get_Install_Dir;

   --  Beginning of Executable_Prefix_Path

   begin
      --  For VMS, the path returned is always /gnu/

      if Hostparm.OpenVMS then
         return "/gnu/";
      end if;

      --  First determine if a path prefix was placed in front of the
      --  executable name.

      for J in reverse Exec_Name'Range loop
         if Exec_Name (J) = Directory_Separator then
            return Get_Install_Dir (Exec_Name);
         end if;
      end loop;

      --  If we get here, the user has typed the executable name with no
      --  directory prefix.

      declare
         Path : String_Access := Locate_Exec_On_Path (Exec_Name);
      begin
         if Path = null then
            return "";
         else
            declare
               Dir : constant String := Get_Install_Dir (Path.all);
            begin
               Free (Path);
               return Dir;
            end;
         end if;
      end;
   end Executable_Prefix_Path;

   --------------------------
   -- File_Not_A_Source_Of --
   --------------------------

   function File_Not_A_Source_Of
     (Uname : Name_Id;
      Sfile : File_Name_Type) return Boolean
   is
      Unit : constant Unit_Index :=
               Units_Htable.Get (Project_Tree.Units_HT, Uname);

      At_Least_One_File : Boolean := False;

   begin
      if Unit /= No_Unit_Index then
         for F in Unit.File_Names'Range loop
            if Unit.File_Names (F) /= null then
               At_Least_One_File := True;
               if Unit.File_Names (F).File = Sfile then
                  return False;
               end if;
            end if;
         end loop;

         if not At_Least_One_File then

            --  The unit was probably created initially for a separate unit
            --  (which are initially created as IMPL when both suffixes are the
            --  same). Later on, Override_Kind changed the type of the file,
            --  and the unit is no longer valid in fact.

            return False;
         end if;

         Verbose_Msg (Uname, "sources do not include ", Name_Id (Sfile));
         return True;
      end if;

      return False;
   end File_Not_A_Source_Of;

   ----------
   -- Hash --
   ----------

   function Hash (Key : Mark_Key) return Mark_Num is
   begin
      return Union_Id (Key.File) mod Max_Mask_Num;
   end Hash;

   ------------
   -- Inform --
   ------------

   procedure Inform (N : File_Name_Type; Msg : String) is
   begin
      Inform (Name_Id (N), Msg);
   end Inform;

   procedure Inform (N : Name_Id := No_Name; Msg : String) is
   begin
      Osint.Write_Program_Name;

      Write_Str (": ");

      if N /= No_Name then
         Write_Str ("""");

         declare
            Name : constant String := Get_Name_String (N);
         begin
            if Debug.Debug_Flag_F and then Is_Absolute_Path (Name) then
               Write_Str (File_Name (Name));
            else
               Write_Str (Name);
            end if;
         end;

         Write_Str (""" ");
      end if;

      Write_Str (Msg);
      Write_Eol;
   end Inform;

   ----------------------------
   -- Is_External_Assignment --
   ----------------------------

   function Is_External_Assignment
     (Tree : Prj.Tree.Project_Node_Tree_Ref;
      Argv : String) return Boolean
   is
      Start     : Positive := 3;
      Finish    : Natural := Argv'Last;

      pragma Assert (Argv'First = 1);
      pragma Assert (Argv (1 .. 2) = "-X");

   begin
      if Argv'Last < 5 then
         return False;

      elsif Argv (3) = '"' then
         if Argv (Argv'Last) /= '"' or else Argv'Last < 7 then
            return False;
         else
            Start := 4;
            Finish := Argv'Last - 1;
         end if;
      end if;

      return Prj.Ext.Check
        (Tree        => Tree,
         Declaration => Argv (Start .. Finish));
   end Is_External_Assignment;

   ---------------
   -- Is_Marked --
   ---------------

   function Is_Marked
     (Source_File : File_Name_Type;
      Index       : Int := 0) return Boolean
   is
   begin
      return Marks.Get (K => (File => Source_File, Index => Index));
   end Is_Marked;

   -----------------------------
   -- Linker_Options_Switches --
   -----------------------------

   function Linker_Options_Switches
     (Project  : Project_Id;
      In_Tree  : Project_Tree_Ref) return String_List
   is
      procedure Recursive_Add (Proj : Project_Id; Dummy : in out Boolean);
      --  The recursive routine used to add linker options

      -------------------
      -- Recursive_Add --
      -------------------

      procedure Recursive_Add (Proj : Project_Id; Dummy : in out Boolean) is
         pragma Unreferenced (Dummy);

         Linker_Package : Package_Id;
         Options        : Variable_Value;

      begin
         Linker_Package :=
           Prj.Util.Value_Of
             (Name        => Name_Linker,
              In_Packages => Proj.Decl.Packages,
              In_Tree     => In_Tree);

         Options :=
           Prj.Util.Value_Of
             (Name                    => Name_Ada,
              Index                   => 0,
              Attribute_Or_Array_Name => Name_Linker_Options,
              In_Package              => Linker_Package,
              In_Tree                 => In_Tree);

         --  If attribute is present, add the project with
         --  the attribute to table Linker_Opts.

         if Options /= Nil_Variable_Value then
            Linker_Opts.Increment_Last;
            Linker_Opts.Table (Linker_Opts.Last) :=
              (Project => Proj, Options => Options.Values);
         end if;
      end Recursive_Add;

      procedure For_All_Projects is
        new For_Every_Project_Imported (Boolean, Recursive_Add);

      Dummy : Boolean := False;

   --  Start of processing for Linker_Options_Switches

   begin
      Linker_Opts.Init;

      For_All_Projects (Project, Dummy, Imported_First => True);

      Last_Linker_Option := 0;

      for Index in reverse 1 .. Linker_Opts.Last loop
         declare
            Options : String_List_Id;
            Proj    : constant Project_Id :=
                        Linker_Opts.Table (Index).Project;
            Option  : Name_Id;
            Dir_Path : constant String :=
                         Get_Name_String (Proj.Directory.Name);

         begin
            Options := Linker_Opts.Table (Index).Options;
            while Options /= Nil_String loop
               Option := In_Tree.String_Elements.Table (Options).Value;
               Get_Name_String (Option);

               --  Do not consider empty linker options

               if Name_Len /= 0 then
                  Add_Linker_Option (Name_Buffer (1 .. Name_Len));

                  --  Object files and -L switches specified with relative
                  --  paths must be converted to absolute paths.

                  Test_If_Relative_Path
                    (Switch => Linker_Options_Buffer (Last_Linker_Option),
                     Parent => Dir_Path,
                     Including_L_Switch => True);
               end if;

               Options := In_Tree.String_Elements.Table (Options).Next;
            end loop;
         end;
      end loop;

      return Linker_Options_Buffer (1 .. Last_Linker_Option);
   end Linker_Options_Switches;

   -----------
   -- Mains --
   -----------

   package body Mains is

      type File_And_Loc is record
         File_Name : File_Name_Type;
         Index     : Int := 0;
         Location  : Source_Ptr := No_Location;
      end record;

      package Names is new Table.Table
        (Table_Component_Type => File_And_Loc,
         Table_Index_Type     => Integer,
         Table_Low_Bound      => 1,
         Table_Initial        => 10,
         Table_Increment      => 100,
         Table_Name           => "Makeutl.Mains.Names");
      --  The table that stores the mains

      Current : Natural := 0;
      --  The index of the last main retrieved from the table

      --------------
      -- Add_Main --
      --------------

      procedure Add_Main (Name : String) is
      begin
         Name_Len := 0;
         Add_Str_To_Name_Buffer (Name);
         Names.Increment_Last;
         Names.Table (Names.Last) := (Name_Find, 0, No_Location);
      end Add_Main;

      ------------
      -- Delete --
      ------------

      procedure Delete is
      begin
         Names.Set_Last (0);
         Mains.Reset;
      end Delete;

      ---------------
      -- Get_Index --
      ---------------

      function Get_Index return Int is
      begin
         if Current in Names.First .. Names.Last then
            return Names.Table (Current).Index;
         else
            return 0;
         end if;
      end Get_Index;

      ------------------
      -- Get_Location --
      ------------------

      function Get_Location return Source_Ptr is
      begin
         if Current in Names.First .. Names.Last then
            return Names.Table (Current).Location;
         else
            return No_Location;
         end if;
      end Get_Location;

      ---------------
      -- Next_Main --
      ---------------

      function Next_Main return String is
      begin
         if Current >= Names.Last then
            return "";
         else
            Current := Current + 1;
            return Get_Name_String (Names.Table (Current).File_Name);
         end if;
      end Next_Main;

      ---------------------
      -- Number_Of_Mains --
      ---------------------

      function Number_Of_Mains return Natural is
      begin
         return Names.Last;
      end Number_Of_Mains;

      -----------
      -- Reset --
      -----------

      procedure Reset is
      begin
         Current := 0;
      end Reset;

      ---------------
      -- Set_Index --
      ---------------

      procedure Set_Index (Index : Int) is
      begin
         if Names.Last > 0 then
            Names.Table (Names.Last).Index := Index;
         end if;
      end Set_Index;

      ------------------
      -- Set_Location --
      ------------------

      procedure Set_Location (Location : Source_Ptr) is
      begin
         if Names.Last > 0 then
            Names.Table (Names.Last).Location := Location;
         end if;
      end Set_Location;

      -----------------
      -- Update_Main --
      -----------------

      procedure Update_Main (Name : String) is
      begin
         if Current in Names.First .. Names.Last then
            Name_Len := 0;
            Add_Str_To_Name_Buffer (Name);
            Names.Table (Current).File_Name := Name_Find;
         end if;
      end Update_Main;
   end Mains;

   ----------
   -- Mark --
   ----------

   procedure Mark (Source_File : File_Name_Type; Index : Int := 0) is
   begin
      Marks.Set (K => (File => Source_File, Index => Index), E => True);
   end Mark;

   -----------------------
   -- Path_Or_File_Name --
   -----------------------

   function Path_Or_File_Name (Path : Path_Name_Type) return String is
      Path_Name : constant String := Get_Name_String (Path);
   begin
      if Debug.Debug_Flag_F then
         return File_Name (Path_Name);
      else
         return Path_Name;
      end if;
   end Path_Or_File_Name;

   ---------------------------
   -- Test_If_Relative_Path --
   ---------------------------

   procedure Test_If_Relative_Path
     (Switch               : in out String_Access;
      Parent               : String;
      Including_L_Switch   : Boolean := True;
      Including_Non_Switch : Boolean := True;
      Including_RTS        : Boolean := False)
   is
   begin
      if Switch /= null then
         declare
            Sw    : String (1 .. Switch'Length);
            Start : Positive;

         begin
            Sw := Switch.all;

            if Sw (1) = '-' then
               if Sw'Length >= 3
                 and then (Sw (2) = 'A'
                            or else Sw (2) = 'I'
                            or else (Including_L_Switch and then Sw (2) = 'L'))
               then
                  Start := 3;

                  if Sw = "-I-" then
                     return;
                  end if;

               elsif Sw'Length >= 4
                 and then (Sw (2 .. 3) = "aL"
                            or else Sw (2 .. 3) = "aO"
                            or else Sw (2 .. 3) = "aI")
               then
                  Start := 4;

               elsif Including_RTS
                 and then Sw'Length >= 7
                 and then Sw (2 .. 6) = "-RTS="
               then
                  Start := 7;

               else
                  return;
               end if;

               --  Because relative path arguments to --RTS= may be relative
               --  to the search directory prefix, those relative path
               --  arguments are converted only when they include directory
               --  information.

               if not Is_Absolute_Path (Sw (Start .. Sw'Last)) then
                  if Parent'Length = 0 then
                     Do_Fail
                       ("relative search path switches ("""
                        & Sw
                        & """) are not allowed");

                  elsif Including_RTS then
                     for J in Start .. Sw'Last loop
                        if Sw (J) = Directory_Separator then
                           Switch :=
                             new String'
                               (Sw (1 .. Start - 1) &
                                Parent &
                                Directory_Separator &
                                Sw (Start .. Sw'Last));
                           return;
                        end if;
                     end loop;

                  else
                     Switch :=
                       new String'
                         (Sw (1 .. Start - 1) &
                          Parent &
                          Directory_Separator &
                          Sw (Start .. Sw'Last));
                  end if;
               end if;

            elsif Including_Non_Switch then
               if not Is_Absolute_Path (Sw) then
                  if Parent'Length = 0 then
                     Do_Fail
                       ("relative paths (""" & Sw & """) are not allowed");
                  else
                     Switch := new String'(Parent & Directory_Separator & Sw);
                  end if;
               end if;
            end if;
         end;
      end if;
   end Test_If_Relative_Path;

   -------------------
   -- Unit_Index_Of --
   -------------------

   function Unit_Index_Of (ALI_File : File_Name_Type) return Int is
      Start  : Natural;
      Finish : Natural;
      Result : Int := 0;

   begin
      Get_Name_String (ALI_File);

      --  First, find the last dot

      Finish := Name_Len;

      while Finish >= 1 and then Name_Buffer (Finish) /= '.' loop
         Finish := Finish - 1;
      end loop;

      if Finish = 1 then
         return 0;
      end if;

      --  Now check that the dot is preceded by digits

      Start := Finish;
      Finish := Finish - 1;

      while Start >= 1 and then Name_Buffer (Start - 1) in '0' .. '9' loop
         Start := Start - 1;
      end loop;

      --  If there are no digits, or if the digits are not preceded by the
      --  character that precedes a unit index, this is not the ALI file of
      --  a unit in a multi-unit source.

      if Start > Finish
        or else Start = 1
        or else Name_Buffer (Start - 1) /= Multi_Unit_Index_Character
      then
         return 0;
      end if;

      --  Build the index from the digit(s)

      while Start <= Finish loop
         Result := Result * 10 +
                     Character'Pos (Name_Buffer (Start)) - Character'Pos ('0');
         Start := Start + 1;
      end loop;

      return Result;
   end Unit_Index_Of;

   -----------------
   -- Verbose_Msg --
   -----------------

   procedure Verbose_Msg
     (N1                : Name_Id;
      S1                : String;
      N2                : Name_Id := No_Name;
      S2                : String  := "";
      Prefix            : String := "  -> ";
      Minimum_Verbosity : Opt.Verbosity_Level_Type := Opt.Low)
   is
   begin
      if not Opt.Verbose_Mode
        or else Minimum_Verbosity > Opt.Verbosity_Level
      then
         return;
      end if;

      Write_Str (Prefix);
      Write_Str ("""");
      Write_Name (N1);
      Write_Str (""" ");
      Write_Str (S1);

      if N2 /= No_Name then
         Write_Str (" """);
         Write_Name (N2);
         Write_Str (""" ");
      end if;

      Write_Str (S2);
      Write_Eol;
   end Verbose_Msg;

   procedure Verbose_Msg
     (N1                : File_Name_Type;
      S1                : String;
      N2                : File_Name_Type := No_File;
      S2                : String  := "";
      Prefix            : String := "  -> ";
      Minimum_Verbosity : Opt.Verbosity_Level_Type := Opt.Low)
   is
   begin
      Verbose_Msg
        (Name_Id (N1), S1, Name_Id (N2), S2, Prefix, Minimum_Verbosity);
   end Verbose_Msg;

end Makeutl;
