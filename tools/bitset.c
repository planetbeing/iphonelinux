#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		printf("Usage: %s <number>\n", argv[0]);
		return 0;
	}

	uint64_t number = strtoll(argv[1], NULL, 0);

	int i;
	for(i = 31; i >= 0; --i)
	{
		printf("%02d ", i);
	}

	printf("\n");

	for(i = 31; i >= 0; --i)
	{
		if((number & (1 << i)) != 0)
			printf(" 1 ");
		else
			printf(" 0 ");
	}

	printf("\n");

	return 0;
}
