// { dg-do run { target *-*-freebsd* *-*-netbsd* *-*-linux* *-*-solaris* *-*-cygwin *-*-darwin* alpha*-*-osf* mips-sgi-irix6* } }
// { dg-options " -std=gnu++0x -pthread" { target *-*-freebsd* *-*-netbsd* *-*-linux* alpha*-*-osf* mips-sgi-irix6* } }
// { dg-options " -std=gnu++0x -pthreads" { target *-*-solaris* } }
// { dg-options " -std=gnu++0x " { target *-*-cygwin *-*-darwin* } }
// { dg-require-cstdint "" }
// { dg-require-gthreads "" }
// { dg-require-atomic-builtins "" }

// Copyright (C) 2009, 2010 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include <future>
#include <testsuite_hooks.h>


// Test promise::set_value() for deadlock by checking if the state is ready
// during construction and destruction of the associated state.

struct tester
{
  tester(int);
  tester(const tester&);
  tester() = delete;
  tester& operator=(const tester&);
};

std::promise<tester> pglobal;
std::future<tester> fglobal = pglobal.get_future();

tester::tester(int)
{
  bool test __attribute__((unused)) = true;
  VERIFY (!fglobal.wait_for(std::chrono::milliseconds(1)));
}

tester::tester(const tester&)
{
  bool test __attribute__((unused)) = true;
  // if this copy happens while a mutex is locked next line could deadlock:
  VERIFY (!fglobal.wait_for(std::chrono::milliseconds(1)));
}

tester& tester::operator=(const tester&)
{
  bool test __attribute__((unused)) = true;
  // if this copy happens while a mutex is locked next line could deadlock:
  VERIFY (!fglobal.wait_for(std::chrono::milliseconds(1)));
  return *this;
}

void test01()
{
  bool test __attribute__((unused)) = true;

  pglobal.set_value( tester(1) );

  VERIFY( fglobal.wait_for(std::chrono::milliseconds(1)) );
}

int main()
{
  test01();
  return 0;
}
