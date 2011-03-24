
.global _do_call

_do_call:
	stmfd sp!, {r4, r5, r6, r7, fp, lr}
	mov r4, r1 // r4 = args_buffer
	mov lr, r0 // lr = func_ptr
	mov r5, r2 // r5 = args_size
	

	ldr r0, [r4, #0]
	ldr r1, [r4, #4]
	ldr r2, [r4, #8]
	ldr r3, [r4, #12]
	
	mov fp, sp

	sub r5, #16
	cmp r5, #0
	blt do_call
	
	add r4, #16
	mov r7, sp
	sub r7, r5
	sub sp, r5
	
copy_args: 
	ldr r6, [r4]
	str r6, [r7]
	add r7, #4
	add r4, #4
  sub r5, #4
	cmp r5, #0
bne copy_args

do_call:

	mov r6, lr
  mov lr, pc
	bx r6
	
	mov sp, fp

	ldmfd sp!, {r4, r5, r6, r7, fp, lr}
	bx lr
