float ret_float() { return 1.0; }
double ret_double() { return 1.0; }

void test_float(double a)
{
	*(volatile double *) 0xdeadbee0 = a;
}

main()
{
  float x = 123.345;
 	test_float(x);
}