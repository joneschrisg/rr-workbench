/* Copyright (C) 1993, 1996, 1997, 1998, 1999, 2002, 2003
   Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  */

#include "libioP.h"
#include <string.h>


size_t strlen_wrapper(const char* s)
{
    return strlen(s);
}

int _IO_fputs (const char *str, _IO_FILE *fp)
{
  __asm__(                      /* str = 4(%esp) */
      "push %ebp\n\t"           /* str = 8(%esp) */
      "movl %esp, %ebp\n\t"     /* str = 8(%ebp) */

      /* Save regs to compare before calling strlen(). */
      "movl $0xe0, %eax\n\t"
      "int $0x80\n\t"           /* syscall(SYS_gettid) */

      "movl 0x8(%ebp), %eax\n\t" /* %eax = str */
      "push %ebx\n\t"            /* save instrumentation regs */
      "push %esi\n\t"
      "push %edi\n\t"
      "push %ebp\n\t"

      "push %eax\n\t"            /* push |str| arg */
      "movl $0xdeadd00d, %edi\n\t" /* enable magic BB tracking */
      "call strlen_wrapper\n\t" /* %eax = strlen(str) */
      "addl $0x4, %esp\n\t"

      /* Save regs for post-strlen() compare. */
      "movl $0x14, %eax\n\t"
      "int $0x80\n\t"           /* syscall(SYS_getpid) */

      "pop %ebp\n\t"            /* restore instrumentation regs */
      "pop %edi\n\t"
      "pop %esi\n\t"
      "pop %ebx\n\t"

      "movl 0xc(%ebp), %eax\n\t" /* push fp */
      "push %eax\n\t"
      "movl 0x8(%ebp), %eax\n\t" /* push str */
      "push %eax\n\t"
      "call _IO_fputs_rest\n\t" /* %eax = _IO_fputs_rest(str, fp) */
      "addl $0x8, %esp\n\t"

      "pop %ebp\n\t"
      );
}

int
_IO_fputs_rest (str, fp)
      const char *str;
      _IO_FILE *fp;
{
  _IO_size_t len = strlen (str);
  int result = EOF;
  CHECK_FILE (fp, EOF);
  _IO_acquire_lock (fp);

  if ((_IO_vtable_offset (fp) != 0 || _IO_fwide (fp, -1) == -1)
      && _IO_sputn (fp, str, len) == len)
    result = 1;
  _IO_release_lock (fp);
  return result;
}
libc_hidden_def (_IO_fputs)

#ifdef weak_alias
weak_alias (_IO_fputs, fputs)

# ifndef _IO_MTSAFE_IO
weak_alias (_IO_fputs, fputs_unlocked)
libc_hidden_ver (_IO_fputs, fputs_unlocked)
# endif
#endif
