! { dg-do compile }
!
! PR 45290: [F08] pointer initialization
!
! Contributed by Janus Weil <janus@gcc.gnu.org>

module m1
 implicit none
 type :: t
   integer, pointer :: p
   integer :: i
 end type
 integer, target :: i
 type(t), target :: x
 integer, pointer :: p1 => i
 integer, pointer :: p2 => p1   ! { dg-error "must have the TARGET attribute" }
 integer, pointer :: p3 => x%p  ! { dg-error "must have the TARGET attribute" }
 integer, pointer :: p4 => x%i
end module m1


module m2

 type :: t
   procedure(s), pointer, nopass :: ppc
 end type
 type(t) :: x
 procedure(s), pointer :: pp1 => s
 procedure(s), pointer :: pp2 => pp1    ! { dg-error "may not be a procedure pointer" }
 procedure(s), pointer :: pp3 => t%ppc  ! { dg-error "Syntax error" }

contains

  subroutine s
  end subroutine

end module m2

! { dg-final { cleanup-modules "m1 m2" } }
