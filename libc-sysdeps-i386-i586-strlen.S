/* strlen -- Compute length of NUL terminated string.
   Highly optimized version for ix86, x>=5.
   Copyright (C) 1995,1996,1997,2000,2002,2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper, <drepper@gnu.ai.mit.edu>.

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
   <http://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include "asm-syntax.h"
#include "bp-sym.h"
#include "bp-asm.h"

/* This version is especially optimized for the i586 (and following?)
   processors.  This is mainly done by using the two pipelines.  The
   version optimized for i486 is weak in this aspect because to get
   as much parallelism we have to execute some *more* instructions.

   The code below is structured to reflect the pairing of the instructions
   as *I think* it is.  I have no processor data book to verify this.
   If you find something you think is incorrect let me know.  */


/* The magic value which is used throughout in the whole code.  */
#define magic 0xfefefeff

#define PARMS	LINKAGE		/* no space for saved regs */
#define STR	PARMS


#define BB_INST 1
/*#define LABEL_INST 1*/
/*#define UNROLL_INST 1*/
#define ZERO_TEST_INST 1
        

	.text
ENTRY (BP_SYM (strlen))
	ENTER

	movl STR(%esp), %eax
	CHECK_BOUNDS_LOW (%eax, STR(%esp))

        
#if BB_INST
        cmpl $0xdeadd00d, %edi
        je strlen_instr

        push %ebx
        push %esi
        push %edi
        push %ebp
        push $0x0               /* did_instrument = false */
        jmp strlen_begin_impl

strlen_instr:
        xorl %ebx, %ebx         /* counter_0 = 0 */
        xorl %esi, %esi         /* counter_1 = 0 */
        xorl %edi, %edi         /* counter_2 = 0 */
        xorl %ebp, %ebp         /* counter_3 = 0 */
        push $0x1               /* did_instrument = true */

strlen_begin_impl:
#endif

        
	movl $3, %edx		/* load mask (= 3) */

	andl %eax, %edx		/* separate last two bits of address */

	jz L(1)			/* aligned => start loop */
	jp L(0)			/* exactly two bits set */

	cmpb %dh, (%eax)	/* is byte NUL? */
	je L(2)			/* yes => return */

	incl %eax		/* increment pointer */
	cmpb %dh, (%eax)	/* is byte NUL? */

	je L(2)			/* yes => return */

	incl %eax		/* increment pointer */
	xorl $2, %edx

	jz L(1)


L(0):
#if LABEL_INST
	incl %ebx               /* ++counter_0 */
#endif
        

	cmpb %dh, (%eax)	/* is byte NUL? */
	je L(2)			/* yes => return */

	incl %eax		/* increment pointer */
	xorl %edx, %edx		/* We need %edx == 0 for later */

      /* We exit the loop if adding MAGIC_BITS to LONGWORD fails to
	 change any of the hole bits of LONGWORD.

	 1) Is this safe?  Will it catch all the zero bytes?
	 Suppose there is a byte with all zeros.  Any carry bits
	 propagating from its left will fall into the hole at its
	 least significant bit and stop.  Since there will be no
	 carry from its most significant bit, the LSB of the
	 byte to the left will be unchanged, and the zero will be
	 detected.

	 2) Is this worthwhile?  Will it ignore everything except
	 zero bytes?  Suppose every byte of LONGWORD has a bit set
	 somewhere.  There will be a carry into bit 8.	If bit 8
	 is set, this will carry into bit 16.  If bit 8 is clear,
	 one of bits 9-15 must be set, so there will be a carry
	 into bit 16.  Similarly, there will be a carry into bit
	 24.  If one of bits 24-31 is set, there will be a carry
	 into bit 32 (=carry flag), so all of the hole bits will
	 be changed.

	 Note: %edx == 0 in any case here.  */


L(1):   
#if LABEL_INST
	incl %esi               /* ++counter_1 */
#endif
        

        /* Unrolled iteration 0 */
#if UNROLL_INST
        incl %ebx               /* ++counter_0 */
#endif

        
	movl (%eax), %ecx	/* get word (= 4 bytes) in question */
	addl $4, %eax		/* adjust pointer for *next* word */

	subl %ecx, %edx		/* first step to negate word */
	addl $magic, %ecx	/* add magic word */

	decl %edx		/* complete negation of word */
	jnc L(3)		/* previous addl caused overflow? */

	xorl %ecx, %edx		/* (word+magic)^word */

	andl $~magic, %edx	/* any of the carry flags set? */

	jne L(3)		/* yes => determine byte */


        /* Unrolled iteration 1 */
#if UNROLL_INST
        incl %esi               /* ++counter_1 */
#endif
        
        
	movl (%eax), %ecx	/* get word (= 4 bytes) in question */
	addl $4, %eax		/* adjust pointer for *next* word */

	subl %ecx, %edx		/* first step to negate word */
	addl $magic, %ecx	/* add magic word */

	decl %edx		/* complete negation of word */
	jnc L(3)		/* previous addl caused overflow? */

	xorl %ecx, %edx		/* (word+magic)^word */

	andl $~magic, %edx	/* any of the carry flags set? */

	jne L(3)		/* yes => determine byte */


        /* Unrolled iteration 2 */
#if UNROLL_INST
        incl %edi               /* ++counter_2 */
#endif

        
	movl (%eax), %ecx	/* get word (= 4 bytes) in question */
	addl $4, %eax		/* adjust pointer for *next* word */

	subl %ecx, %edx		/* first step to negate word */
	addl $magic, %ecx	/* add magic word */

	decl %edx		/* complete negation of word */
	jnc L(3)		/* previous addl caused overflow? */

	xorl %ecx, %edx		/* (word+magic)^word */

	andl $~magic, %edx	/* any of the carry flags set? */

	jne L(3)		/* yes => determine byte */


        /* Unrolled iteration 3 */
#if UNROLL_INST
        incl %ebp               /* ++counter_3 */
#endif
        
	movl (%eax), %ecx	/* get word (= 4 bytes) in question */
	addl $4, %eax		/* adjust pointer for *next* word */

	subl %ecx, %edx		/* first step to negate word */
	addl $magic, %ecx	/* add magic word */

	decl %edx		/* complete negation of word */
	jnc L(3)		/* previous addl caused overflow? */

	xorl %ecx, %edx		/* (word+magic)^word */

	andl $~magic, %edx	/* any of the carry flags set? */

	je L(1)			/* no => start loop again */


L(3):
#if LABEL_INST
	incl %ebp               /* ++counter_3 */
#endif
        

	subl $4, %eax		/* correct too early pointer increment */
	subl $magic, %ecx

        
        
#if ZERO_TEST_INST
	incl %ebx               /* ++counter_0 */
#endif

        
	cmpb $0, %cl		/* lowest byte NUL? */
	jz L(2)			/* yes => return */

        
#if ZERO_TEST_INST
	incl %esi               /* ++counter_1 */
#endif

        
	inc %eax		/* increment pointer */
	testb %ch, %ch		/* second byte NUL? */

	jz L(2)			/* yes => return */

	shrl $16, %ecx		/* make upper bytes accessible */
	incl %eax		/* increment pointer */

        
#if ZERO_TEST_INST
	incl %edi               /* ++counter_2 */
#endif

        
	cmpb $0, %cl		/* is third byte NUL? */
	jz L(2)			/* yes => return */

        
#if ZERO_TEST_INST
	incl %ebp               /* ++counter_3 */
#endif

        
	incl %eax		/* increment pointer */


L(2):
#if LABEL_INST
	incl %edi               /* ++counter_2 */
#endif

        
#if BB_INST
        pop %edx                /* did_instrument <- pop() */
        cmp $0x1, %edx
        je done                 /* if (did_instrument) return; */

        pop %ebp                /* else */
        pop %edi
        pop %esi
        pop %ebx
done:
#endif      

        
	CHECK_BOUNDS_HIGH (%eax, STR(%esp), jb)
	subl STR(%esp), %eax	/* now compute the length as difference
				   between start and terminating NUL
				   character */
	LEAVE
	ret
END (BP_SYM (strlen))
libc_hidden_builtin_def (strlen)
