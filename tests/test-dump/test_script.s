	.file	"test_scripting_and_inference"
	.text
	.globl	add
	.p2align	4
	.type	add,@function
add:
	.cfi_startproc
	movl	%edi, -4(%rsp)
	movl	%esi, -8(%rsp)
	leal	(%rdi,%rsi), %eax
	retq
.Lfunc_end0:
	.size	add, .Lfunc_end0-add
	.cfi_endproc

	.globl	__luv_user_main
	.p2align	4
	.type	__luv_user_main,@function
__luv_user_main:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdx, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdi, -24(%rbp)
	testq	%rsi, %rsi
	jle	.LBB1_4
	movq	-16(%rbp), %rax
	movq	%rsp, %rdx
	leaq	-16(%rdx), %rcx
	movq	%rcx, %rsp
	movl	$0, -16(%rdx)
	.p2align	4
.LBB1_2:
	movslq	(%rcx), %rdx
	cmpq	%rax, %rdx
	jge	.LBB1_4
	incl	(%rcx)
	jmp	.LBB1_2
.LBB1_4:
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	__luv_user_main, .Lfunc_end1-__luv_user_main
	.cfi_endproc

	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	4, 0x0
.LCPI2_0:
	.long	1
	.long	2
	.long	3
	.long	4
	.text
	.p2align	4
	.type	__luv_init,@function
__luv_init:
	.cfi_startproc
	subq	$56, %rsp
	.cfi_def_cfa_offset 64
	movl	$10, %edi
	movl	$20, %esi
	callq	add@PLT
	movl	%eax, 8(%rsp)
	movaps	.LCPI2_0(%rip), %xmm0
	movups	%xmm0, 40(%rsp)
	leaq	40(%rsp), %rax
	movq	%rax, 16(%rsp)
	movq	$4, 24(%rsp)
	movq	$4, 32(%rsp)
	movl	$42, 12(%rsp)
	addq	$56, %rsp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end2:
	.size	__luv_init, .Lfunc_end2-__luv_init
	.cfi_endproc

	.globl	main
	.p2align	4
	.type	main,@function
main:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%r15
	.cfi_def_cfa_offset 24
	pushq	%r14
	.cfi_def_cfa_offset 32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	pushq	%rax
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	.cfi_offset %rbp, -16
	movq	%rsi, %rbx
	movl	%edi, %ebp
	leaq	.L__unnamed_1(%rip), %rdi
	xorl	%r15d, %r15d
	xorl	%eax, %eax
	callq	printf@PLT
	callq	__luv_init
	leaq	.L__unnamed_2(%rip), %rdi
	xorl	%eax, %eax
	callq	printf@PLT
	movl	%ebp, %r14d
	leaq	(,%r14,8), %rdi
	callq	malloc@PLT
	cmpq	%r14, %r15
	jge	.LBB3_3
	.p2align	4
.LBB3_2:
	movq	(%rbx,%r15,8), %rcx
	movq	%rcx, (%rax,%r15,8)
	incq	%r15
	cmpq	%r14, %r15
	jl	.LBB3_2
.LBB3_3:
	movq	%rax, %rdi
	movq	%r14, %rsi
	movq	%r14, %rdx
	callq	__luv_user_main@PLT
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%r14
	.cfi_def_cfa_offset 24
	popq	%r15
	.cfi_def_cfa_offset 16
	popq	%rbp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end3:
	.size	main, .Lfunc_end3-main
	.cfi_endproc

	.type	.L__unnamed_1,@object
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_1:
	.asciz	"RUNTIME: Calling __luv_init\n"
	.size	.L__unnamed_1, 29

	.type	.L__unnamed_2,@object
.L__unnamed_2:
	.asciz	"RUNTIME: Calling __luv_user_main\n"
	.size	.L__unnamed_2, 34

	.section	".note.GNU-stack","",@progbits
