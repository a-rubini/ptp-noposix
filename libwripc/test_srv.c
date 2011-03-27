#include <stdio.h>
#include <string.h>

#include "wr_ipc.h"
#include "structs.h"

int bigfunc(int a, int b, int c, int d, int e, int f, int g, int h)
{
	printf("Call bigfunc: %d %d %d %d %d %d %d %d\n" ,a,b,c,d,e,f,g,h);
	return a+b+c+d+e+f+g+h;
}

void add(float *rval, float a, float b)
{
	printf("Call add: %.3f + %.3f\n", a,b);
 *rval = a + b;
	

}

int string_test(char *string, int a, int b)
{
	printf("Call string_test: a = %d, b= %d, string = '%s'\n", a,b,string);
	return 12345;
}

void structure_test(struct test_struct *s)
{
	printf("Call structure_test: %s got %d apples and %d peas of total value %.3f\n",s->name, s->apples, s->peas, s->value);
}

void get_state(struct state_struct *rval, int request)
{
	printf("Call get_state: request =  %d\n", request);
	
	rval->t = 123.0;
	strcpy(rval->state_name, "SomeState");
	rval->x = request;
}


int main()
{
  wripc_handle_t srv;

  srv = wripc_create_server("test");

//	add(10000, 1.0, 2.0);

  wripc_export(srv, T_INT32, "bigfunc", bigfunc, 8, T_INT32, T_INT32, T_INT32, T_INT32, T_INT32, T_INT32, T_INT32, T_INT32 );

  wripc_export(srv, T_FLOAT, "add", add, 2, T_FLOAT, T_FLOAT);
  wripc_export(srv, T_INT32, "sub", add, 2, T_INT32, T_INT32);

  wripc_export(srv, T_INT32, "string_test", string_test, 3, T_STRING, T_INT32, T_INT32);

  wripc_export(srv, T_VOID, "structure_test", structure_test, 1, T_STRUCT(struct test_struct));
  wripc_export(srv, T_STRUCT(struct state_struct), "get_state", get_state, 1, T_INT32);

  for(;;) wripc_process(srv);
  
  return 0;
}
