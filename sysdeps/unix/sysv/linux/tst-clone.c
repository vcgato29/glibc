/* Test for proper error/errno handling in clone.
   Copyright (C) 2006 Free Software Foundation, Inc.
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

/* BZ #2386 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

int child_fn(void *arg)
{
  puts ("FAIL: in child_fn(); should not be here");
  exit(1);
}

static int
do_test (void)
{
  int result;

#ifdef __ia64__
  result = __clone2(child_fn, NULL, 0, 0, NULL, NULL, NULL);
#else
  result = clone(child_fn, NULL, (int) NULL, NULL);
#endif

  if (errno != EINVAL || result != -1)
    {
      printf ("FAIL: clone()=%d (wanted -1) errno=%d (wanted %d)\n",
              result, errno, EINVAL);
      return 1;
    }

  puts ("All OK");
  return 0;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"