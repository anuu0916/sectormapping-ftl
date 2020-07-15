#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "sectormap.h"

extern FILE *devicefp;


void initialize_flash_memory();

int main(int argc, char *argv[])
{
	// 아래 세 개의 변수는 테스트할 때 필요하면 사용하기 바람
	FILE *workloadfp;
	char sectorbuf[SECTOR_SIZE];
	int lpn;

	// 가상 flash memory의 파일명은 'flashmemory'을 가정함
	devicefp = fopen("flashmemory", "w+b");	

	if(devicefp == NULL)
	{
		printf("file open error\n");
		exit(1);
	}

	initialize_flash_memory();

	ftl_open();

	//
	// ftl_write() 및 ftl_read() 테스트를 위한 코드를 자유자재로 만드세요
	//


	fclose(devicefp);

	return 0;
}

//
// initialize flash memory where each byte are set to 'OxFF'
// 
void initialize_flash_memory()
{
    	char *blockbuf;
	int i;

	blockbuf = (char *)malloc(BLOCK_SIZE);
	memset(blockbuf, 0xFF, BLOCK_SIZE);

	for(i = 0; i < BLOCKS_PER_DEVICE; i++)
	{
		fwrite(blockbuf, BLOCK_SIZE, 1, devicefp);
	}

	free(blockbuf);

	return;
}
