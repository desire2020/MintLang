default rel
extern printf
global main
SECTION .text
main:
	mov	rdi, __format_str
	mov	rsi, __conststr_0
	call	printf
	xor	rax, rax
	ret
SECTION .text
__format_str:
	db	"%s", 0
__conststr_0:
	db	0
