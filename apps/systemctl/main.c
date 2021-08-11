#include <stdio.h>
#include <unistd.h>
int main(int argc, char **argv)
{
	int i = 0;
	printf("argc:%d\n", argc);
	
	for(i = 0; i < argc; i++)
	{
		printf("argv[%d]:%s\n", i, argv[i]);
	}
	while(1)
	{
		sleep(1);
	}
	return 0;
}
