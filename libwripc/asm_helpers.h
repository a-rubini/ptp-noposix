// architecture-specific _do_call() functions

#ifdef __i386__


static int _do_call(void *func_ptr, void *args, int args_size)
{
	int rval;

	
  asm volatile (
	 "movl %%esp, %%edi\n"
	 "movl %%ebx, %%ecx\n"
	 "subl %%ebx, %%edi\n"
	 "shrl $2, %%ecx\n"
	 "cld\n"
	 "rep\n"
	 "movsl\n"
	 "subl %3, %%esp\n"
	 "call *%%eax\n"
	 "addl %3, %%esp\n"
   : "=a"(rval)
   : "a"(func_ptr), "S"(args), "b"(args_size)
   :  
   
	 );
	 
	 /* "si", "di", "ax", "bx", "cx", "memory" */
	 return rval;

}


#endif

#ifdef __arm__

extern int _do_call(void *func_ptr, void *args, int args_size);

#endif
