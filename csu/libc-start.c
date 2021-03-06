/* Copyright (C) 1998-2006, 2007 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ldsodefs.h>
#include <bp-start.h>
#include <bp-sym.h>

extern void __libc_init_first (int argc, char **argv, char **envp);

extern int __libc_multiple_libcs;

#include <tls.h>
#ifndef SHARED
# include <dl-osinfo.h>
extern void __pthread_initialize_minimal (void);
# ifndef THREAD_SET_STACK_GUARD
/* Only exported for architectures that don't store the stack guard canary
   in thread local area.  */
uintptr_t __stack_chk_guard attribute_relro;
# endif
#endif

#ifdef HAVE_PTR_NTHREADS
/* We need atomic operations.  */
# include <atomic.h>
#endif

#ifdef HAVE_ZRT
#include <zrt.h>
#include <irt_syscalls.h>
#include <nvram/nvram.h> //NVRAM_MAX_RECORDS_IN_SECTION
#endif

#ifdef LIBC_START_MAIN
# ifdef LIBC_START_DISABLE_INLINE
#  define STATIC static
# else
#  define STATIC static inline __attribute__ ((always_inline))
# endif
#else
# define STATIC
# define LIBC_START_MAIN BP_SYM (__libc_start_main)
#endif

#ifdef MAIN_AUXVEC_ARG
/* main gets passed a pointer to the auxiliary.  */
# define MAIN_AUXVEC_DECL	, void *
# define MAIN_AUXVEC_PARAM	, auxvec
#else
# define MAIN_AUXVEC_DECL
# define MAIN_AUXVEC_PARAM
#endif

#pragma GCC push_options
#pragma GCC optimize ("O0")

STATIC int LIBC_START_MAIN (int (*main) (int, char **, char **
					 MAIN_AUXVEC_DECL),
			    int argc,
			    char *__unbounded *__unbounded ubp_av,
#ifdef LIBC_START_MAIN_AUXVEC_ARG
			    ElfW(auxv_t) *__unbounded auxvec,
#endif
			    __typeof (main) init,
			    void (*fini) (void),
			    void (*rtld_fini) (void),
			    void *__unbounded stack_end)
    __attribute__ ((noreturn));


/* Note: the fini parameter is ignored here for shared library.  It
   is registered with __cxa_atexit.  This had the disadvantage that
   finalizers were called in more than one place.  */
STATIC int
LIBC_START_MAIN (int (*main) (int, char **, char ** MAIN_AUXVEC_DECL),
		 int argc, char *__unbounded *__unbounded ubp_av,
#ifdef LIBC_START_MAIN_AUXVEC_ARG
		 ElfW(auxv_t) *__unbounded auxvec,
#endif
		 __typeof (main) init,
		 void (*fini) (void),
		 void (*rtld_fini) (void), void *__unbounded stack_end)
{
#if __BOUNDED_POINTERS__
    char **argv;
#else
# define argv ubp_av
#endif

    /* Result of the 'main' function.  */
    int result;

    __libc_multiple_libcs = &_dl_starting_up && !_dl_starting_up;

#ifndef SHARED
    char *__unbounded *__unbounded ubp_ev = &ubp_av[argc + 1];
  
#ifdef HAVE_ZRT
    /*Initialize syscall functions*/
    init_irt_table ();

    /*For libzrt.so it's will be used original arguments, nvram args
      will be ignored*/
# ifndef __ZRT_SO
    /*basic setup of args, envs*/
    argc = 1;
    argv = alloca( sizeof(char*)*2 );
    ubp_ev = alloca( sizeof(char*) );
    argv[0] = "stub\0";
    argv[1] = NULL;
    ubp_ev[0] = NULL;
# endif //__ZRT_SO
#endif //HAVE_ZRT

    INIT_ARGV_and_ENVIRON;

#ifndef __ZRT_SO
    /* Store the lowest stack address.  This is done in ld.so if this is
       the code for the DSO.  */
    __libc_stack_end = stack_end;

# ifdef HAVE_AUX_VECTOR
    /* First process the auxiliary vector since we need to find the
       program header to locate an eventually present PT_TLS entry.  */
#  ifndef LIBC_START_MAIN_AUXVEC_ARG
    ElfW(auxv_t) *__unbounded auxvec;
    {
	char *__unbounded *__unbounded evp = ubp_ev;
	while (*evp++ != NULL)
	    ;
	auxvec = (ElfW(auxv_t) *__unbounded) evp;
    }
#  endif
    _dl_aux_init (auxvec);
# endif
# ifdef DL_SYSDEP_OSCHECK
    if (!__libc_multiple_libcs)
	{
	    /* This needs to run to initiliaze _dl_osversion before TLS
	       setup might check it.  */
	    DL_SYSDEP_OSCHECK (__libc_fatal);
	}
# endif

#endif //__ZRT_SO

    /* Initialize the thread library at least a bit since the libgcc
       functions are using thread functions if these are available and
       we need to setup errno.  */
    __pthread_initialize_minimal ();

#ifdef HAVE_ZRT
    /*try to init zrt just after TLS setted up*/
    struct zcalls_init_t* zcalls;
    if ( ZCALLS_INIT == __query_zcalls(ZCALLS_INIT, (void**)&zcalls) &&
	 zcalls ){
	/*run zcall init*/
	zcalls->init();
    }
#endif

    /* TODO(mseaborn): In the long term we could implement a futex
       syscall for NaCl and so this ad-hoc initialisation would not be
       necessary.  See:
       http://code.google.com/p/nativeclient/issues/detail?id=1244  */
# ifdef __native_client__
    __nacl_futex_init ();
# endif

#ifndef __ZRT_SO
    /* Set up the stack checker's canary.  */
    uintptr_t stack_chk_guard = _dl_setup_stack_chk_guard ();
# ifdef THREAD_SET_STACK_GUARD
    THREAD_SET_STACK_GUARD (stack_chk_guard);
# else
    __stack_chk_guard = stack_chk_guard;
# endif
#endif //__ZRT_SO
#endif

    /* Register the destructor of the dynamic linker if there is any.  */
    if (__builtin_expect (rtld_fini != NULL, 1))
	__cxa_atexit ((void (*) (void *)) rtld_fini, NULL, NULL);

#ifndef __ZRT_SO
# ifndef SHARED
    /* Call the initializer of the libc.  This is only needed here if we
       are compiling for the static library in which case we haven't
       run the constructors in `_dl_start_user'.  */
    __libc_init_first (argc, argv, __environ);

    /* Register the destructor of the program, if any.  */
    if (fini)
	__cxa_atexit ((void (*) (void *)) fini, NULL, NULL);

    /* Some security at this point.  Prevent starting a SUID binary where
       the standard file descriptors are not opened.  We have to do this
       only for statically linked applications since otherwise the dynamic
       loader did the work already.  */
    if (__builtin_expect (__libc_enable_secure, 0))
	__libc_check_standard_fds ();
# endif

    /* Call the initializer of the program, if any.  */
# ifdef SHARED
    if (__builtin_expect (GLRO(dl_debug_mask) & DL_DEBUG_IMPCALLS, 0))
	GLRO(dl_debug_printf) ("\ninitialize program: %s\n\n", argv[0]);
# endif

    /*glibc init moved after zrt init*/

#ifdef SHARED
    /* Auditing checkpoint: we have a new object.  */
    if (__builtin_expect (GLRO(dl_naudit) > 0, 0))
	{
	    struct audit_ifaces *afct = GLRO(dl_audit);
	    struct link_map *head = GL(dl_ns)[LM_ID_BASE]._ns_loaded;
	    for (unsigned int cnt = 0; cnt < GLRO(dl_naudit); ++cnt)
		{
		    if (afct->preinit != NULL)
			afct->preinit (&head->l_audit[cnt].cookie);

		    afct = afct->next;
		}
	}
#endif

#ifdef SHARED
    if (__builtin_expect (GLRO(dl_debug_mask) & DL_DEBUG_IMPCALLS, 0))
	GLRO(dl_debug_printf) ("\ntransferring control: %s\n\n", argv[0]);
#endif

#endif //__ZRT_SO

#ifdef HAVE_CLEANUP_JMP_BUF
    /* Memory for the cancellation buffer.  */
    struct pthread_unwind_buf unwind_buf;

    int not_first_call;
    not_first_call = setjmp ((struct __jmp_buf_tag *) unwind_buf.cancel_jmp_buf);
    if (__builtin_expect (! not_first_call, 1))	{
	struct pthread *self = THREAD_SELF;

	/* Store old info.  */
	unwind_buf.priv.data.prev = THREAD_GETMEM (self, cleanup_jmp_buf);
	unwind_buf.priv.data.cleanup = THREAD_GETMEM (self, cleanup);

	/* Store the new cleanup handler info.  */
	THREAD_SETMEM (self, cleanup_jmp_buf, &unwind_buf);

#ifdef HAVE_ZRT
	/*try to init zrt if available*/
	struct zcalls_zrt_t* zcalls_zrt_init;
	if ( ZCALLS_ZRT == __query_zcalls(ZCALLS_ZRT, (void**)&zcalls_zrt_init) ){
	    if ( zcalls_zrt_init && zcalls_zrt_init->zrt_setup ){
		zcalls_zrt_init->zrt_setup();
	    }
	}
#endif

#ifdef HAVE_ZRT
	/*setup args, envs just after warmup in zrt_setup*/
	char **nvram_args;
	char **nvram_envs;
	char  *args_buf;
	char  *envs_buf;

	/*init env & args readed from nvram*/  
	struct zcalls_env_args_init_t* zcalls_env_args_init;
	if ( ZCALLS_ENV_ARGS_INIT == __query_zcalls(ZCALLS_ENV_ARGS_INIT, 
						    (void**)&zcalls_env_args_init) ){
	    if ( zcalls_env_args_init && 
		 zcalls_env_args_init->read_nvram_get_args_envs &&
		 zcalls_env_args_init->get_nvram_args_envs ){
		/*retrieve lengths of args & env variables*/
		int arg_buf_size;
		int env_buf_size;
		int env_count;
		zcalls_env_args_init->read_nvram_get_args_envs( &arg_buf_size, 
								&env_buf_size, &env_count);
		/*preallocate array to save args*/
		nvram_args = alloca( NVRAM_MAX_RECORDS_IN_SECTION * sizeof(char*) );
		/*preallocate buffer to copy for arguments parsed from nvram*/
		args_buf = alloca( arg_buf_size +1 ); /*+null term char*/

		/*preallocate array to save envs*/
		nvram_envs = alloca( (env_count+1) * sizeof(char*) );
		/*preallocate buffer to copy for env vars  parsed from nvram*/
		envs_buf = alloca( env_buf_size +1 ); /*+null term char*/
	  
		/*retrieve args & envs into two-dimentional arrays*/
		zcalls_env_args_init->get_nvram_args_envs( nvram_args, args_buf, arg_buf_size,
							   nvram_envs, envs_buf, env_buf_size);
		/*calculate args count*/
		int arg_count=0;
		while( nvram_args[arg_count] != NULL )
		    ++arg_count;

#ifndef __ZRT_SO
		/*update args by values from nvram*/
		argc = arg_count;
		argv = nvram_args;
		__environ MAIN_AUXVEC_PARAM = nvram_envs;
#endif //__ZRT_SO
	    }
	}
	extern char **__libc_argv attribute_hidden;
	/*update internal glibc arguments*/
	__libc_argv = argv;
#endif

	/*in case if using zrt.so init function should not be NULL and 
	saves nvram_envs into system environment, so __environ globabal var should not be changed*/
	if (init)
	    (*init) (argc, argv, nvram_envs);

#ifdef HAVE_ZRT
	/*premain callback*/
	if ( zcalls_zrt_init && zcalls_zrt_init->zrt_premain ){
	    zcalls_zrt_init->zrt_premain();
	}
#endif

	/* Run the program.  */
	result = main (argc, argv, nvram_envs);
    }
    else
	{
	    /* Remove the thread-local data.  */
# ifdef SHARED
	    PTHFCT_CALL (ptr__nptl_deallocate_tsd, ());
# else
	    extern void __nptl_deallocate_tsd (void) __attribute ((weak));
	    __nptl_deallocate_tsd ();
# endif

	    /* One less thread.  Decrement the counter.  If it is zero we
	       terminate the entire process.  */
	    result = 0;
# ifdef SHARED
	    unsigned int *ptr = __libc_pthread_functions.ptr_nthreads;
	    PTR_DEMANGLE (ptr);
# else
	    extern unsigned int __nptl_nthreads __attribute ((weak));
	    unsigned int *const ptr = &__nptl_nthreads;
# endif

	    if (! atomic_decrement_and_test (ptr))
		/* Not much left to do but to exit the thread, not the process.  */
		__exit_thread (0);
	}
#else
    /* Nothing fancy, just call the function.  */
    result = main (argc, argv, __environ MAIN_AUXVEC_PARAM);
#endif

#ifndef __ZRT_SO
    exit (result);
#endif
}

#pragma GCC pop_options
