#include <stdio.h>

#include "wr_ipc.h"
#include "structs.h"

main()
{
  wripc_handle_t cli;
	int res_int;
	float result, a,b;
	
	struct state_struct s;
	
	
  cli = wripc_connect("test");

	wripc_call(cli, "bigfunc", &res_int, 8, A_INT32(1), A_INT32(2), A_INT32(3), A_INT32(4),  A_INT32(5), A_INT32(6), A_INT32(7), A_INT32(8));
	printf("1+...+8 = %d\n", res_int);

  a=2.2; b= 4.4;
  wripc_call(cli, "add", &result, 2, A_FLOAT(a), A_FLOAT(b));
	printf("%.1f + %.1f = %.1f\n", a,b,result);

  wripc_call(cli, "get_state", &s, 1, A_INT32(12345));

  printf("state->name %s\n", s.state_name);
  printf("state->t %.3f\n", s.t);
  printf("state->x %d\n", s.x);


	struct test_struct s2;
	s2.apples = 10;
	s2.peas = 15;
	strcpy(s2.name, "Javier");
	s2.value = 17.50;
	
//	wripc_call(cli, "structure_test", &result, 1, A_STRUCT(s2));
	

	wripc_call(cli, "string_test", &res_int, 3, A_STRING("Hello, world"), A_INT32(1), A_INT32(100));
	printf("rval = %d\n", res_int);

	usleep(1000);
	wripc_close(cli);
	
}
