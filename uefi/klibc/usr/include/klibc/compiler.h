/*
 * klibc/compiler.h
 *
 * Various compiler features
 */

#ifndef _KLIBC_COMPILER_H
#define _KLIBC_COMPILER_H

/* Specific calling conventions */
/* __cdecl is used when we want varadic and non-varadic functions to have
   the same binary calling convention. */
#ifdef __i386__
# ifdef __GNUC__
#  define __cdecl __attribute__((cdecl,regparm(0)))
# else
  /* Most other C compilers have __cdecl as a keyword */
# endif
#else
# define __cdecl		/* Meaningless on non-i386 */
#endif

/*
 * How to declare a function which should be inlined or instantiated locally
 */
#ifdef __GNUC__
# ifdef __GNUC_STDC_INLINE__
#  define __static_inline static __inline__ __attribute__((__gnu_inline__))
# else
#  define __static_inline static __inline__
# endif
#else
# define __static_inline inline	/* Just hope this works... */
#endif

/*
 * How to declare a function which should be inlined or have a call to
 * an external module
 */
#ifdef __GNUC__
# ifdef __GNUC_STDC_INLINE__
#  define __extern_inline extern __inline__ __attribute__((__gnu_inline__))
# else
#  define __extern_inline extern __inline__
# endif
#else
# define __extern_inline inline	/* Just hope this works... */
#endif

/* How to declare a function that *must* be inlined */
/* Use "extern inline" even in the gcc3+ case to avoid warnings in ctype.h */
#ifdef __GNUC__
# if __GNUC__ >= 3
#  define __must_inline __extern_inline __attribute__((__always_inline__))
# else
#  define __must_inline extern __inline__
# endif
#else
# define __must_inline inline	/* Just hope this works... */
#endif

/* How to declare a function that does not return */
#ifdef __GNUC__
# define __noreturn void __attribute__((noreturn))
#else
# define __noreturn void
#endif

/* "const" function:

     Many functions do not examine any values except their arguments,
     and have no effects except the return value.  Basically this is
     just slightly more strict class than the `pure' attribute above,
     since function is not allowed to read global memory.

     Note that a function that has pointer arguments and examines the
     data pointed to must _not_ be declared `const'.  Likewise, a
     function that calls a non-`const' function usually must not be
     `const'.  It does not make sense for a `const' function to return
     `void'.
*/
#ifdef __GNUC__
# define __constfunc __attribute__((const))
#else
# define __constfunc
#endif
#undef __attribute_const__
#define __attribute_const__ __constfunc

/* "pure" function:

     Many functions have no effects except the return value and their
     return value depends only on the parameters and/or global
     variables.  Such a function can be subject to common subexpression
     elimination and loop optimization just as an arithmetic operator
     would be.  These functions should be declared with the attribute
     `pure'.
*/
#ifdef __GNUC__
# define __purefunc __attribute__((pure))
#else
# define __purefunc
#endif
#undef __attribute_pure__
#define __attribute_pure__ __purefunc

/* Format attribute */
#ifdef __GNUC__
# define __formatfunc(t,f,a) __attribute__((format(t,f,a)))
#else
# define __formatfunc(t,f,a)
#endif

/* malloc() function (returns unaliased pointer) */
#if defined(__GNUC__) && (__GNUC__ >= 3)
# define __mallocfunc __attribute__((malloc))
#else
# define __mallocfunc
#endif

/* likely/unlikely */
#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95))
# define __likely(x)   __builtin_expect(!!(x), 1)
# define __unlikely(x) __builtin_expect(!!(x), 0)
#else
# define __likely(x)   (!!(x))
# define __unlikely(x) (!!(x))
#endif

/* Possibly unused function */
#ifdef __GNUC__
# define __unusedfunc	__attribute__((unused))
#else
# define __unusedfunc
#endif

/* It's all user space... */
#define __user

/* The bitwise attribute: disallow arithmetric operations */
#ifdef __CHECKER__		/* sparse only */
# define __bitwise	__attribute__((bitwise))
#else
# define __bitwise
#endif

/* Shut up unused warnings */
#ifdef __GNUC__
# define __attribute_used__ __attribute__((used))
#else
# define __attribute_used__
#endif

/* Compiler pragma to make an alias symbol */
#define __ALIAS(__t, __f, __p, __a) \
  __t __f __p __attribute__((weak, alias(#__a)));

#endif
