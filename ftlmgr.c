// 주의사항
// 1. sectormap.h에 정의되어 있는 상수 변수를 우선적으로 사용해야 함
// 2. sectormap.h에 정의되어 있지 않을 경우 본인이 이 파일에서 만들어서 사용하면 됨
// 3. 필요한 data structure가 필요하면 이 파일에서 정의해서 쓰기 바람(sectormap.h에 추가하면 안됨)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "sectormap.h"
// 필요한 경우 헤더 파일을 추가하시오.

int dd_read(int ppn, char *pagebuf);
int dd_write(int ppn, char *pagebuf);
int dd_erase(int pbn);
int do_garbagecollection(int freepage, int lsn);

FILE *flashfp; //flash file pointer
typedef struct AddressMappingTable{
	int lpn;
	int ppn;
}adt;
adt address_mapping_table[DATAPAGES_PER_DEVICE];
int freeblock;
char pagebuf[PAGE_SIZE];
int ret;
SpareData sparedata[BLOCKS_PER_DEVICE*PAGES_PER_BLOCK];

//
// flash memory를 처음 사용할 때 필요한 초기화 작업, 예를 들면 address mapping table에 대한
// 초기화 등의 작업을 수행한다. 따라서, 첫 번째 ftl_write() 또는 ftl_read()가 호출되기 전에
// file system에 의해 반드시 먼저 호출이 되어야 한다.
//
void ftl_open()
{
	int i,j;
	int count = 0;

	//
	// address mapping table 초기화
    // address mapping table에서 lpn 수는 DATAPAGES_PER_DEVICE 동일

	for(int i=0; i<DATAPAGES_PER_DEVICE; i++){
		address_mapping_table[i].lpn = i;
		address_mapping_table[i].ppn = -1;
	}
	
	// free block's ppn 초기화
	freeblock = DATABLKS_PER_DEVICE;
	
	//sparedata
	for(int i=0; i<BLOCKS_PER_DEVICE*PAGES_PER_BLOCK; i++){
		sparedata[i].lpn = -1;
		if(i/PAGES_PER_BLOCK == freeblock)
			sparedata[i].is_invalid = TRUE;
		else
			sparedata[i].is_invalid = FALSE;
	}


	return;
}

//
// 이 함수를 호출하기 전에 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 한다.
// 즉, 이 함수에서 메모리를 할당받으면 안된다.
//
void ftl_read(int lsn, char *sectorbuf)
{
	memset(pagebuf, (char)0xFF, PAGE_SIZE);
	memset(sectorbuf, (char)0xFF, SECTOR_SIZE);
	int ppn = address_mapping_table[lsn].ppn;

	if(ppn == -1){
		printf("No data in that area\n");
		return;
	}

	if((ret = dd_read(ppn, pagebuf)) < 0){
		fprintf(stderr, "read error\n");
		exit(1);
	}

	memcpy(sectorbuf, pagebuf, SECTOR_SIZE);

	return;
}

void ftl_write(int lsn, char *sectorbuf)
{
	int i, j;
	int freepage = -1;

	if((DATAPAGES_PER_DEVICE-1) < lsn){
		fprintf(stderr, "lsn %d is larger than actual size\n", lsn);
	}
	
	for(i=0; i<BLOCKS_PER_DEVICE*PAGES_PER_BLOCK; i++){ //free page search
		if(sparedata[i].is_invalid == FALSE){
			freepage = i;
			break;
		}
	}

	if(freepage == -1)
		freepage = do_garbagecollection(freepage, lsn);

	memset(pagebuf, (char)0xFF, PAGE_SIZE);
	memcpy(pagebuf, sectorbuf, strlen(sectorbuf));
	pagebuf[SECTOR_SIZE] = lsn;
	
	if(address_mapping_table[lsn].ppn == -1){
		if((ret = dd_write(freepage, pagebuf)) < 0){
			fprintf(stderr, "write error\n");
			exit(1); 
		}
		address_mapping_table[lsn].ppn = freepage;
		sparedata[freepage].lpn = lsn;
		sparedata[freepage].is_invalid = TRUE;
	}
	else{
		if((ret = dd_write(freepage, pagebuf)) < 0){
			fprintf(stderr, "write error\n");
			exit(1); 
		}
		sparedata[address_mapping_table[lsn].ppn].lpn = -1;
		address_mapping_table[lsn].ppn = freepage;
		sparedata[freepage].lpn = lsn;
		sparedata[freepage].is_invalid = TRUE;
	}

	return;
}

int do_garbagecollection(int freepage, int lsn)
{
	int i;
	int garbageblock = -1;
	int curlsn;
	int pagenum;

	for(i=0; i<BLOCKS_PER_DEVICE*PAGES_PER_BLOCK; i++){ //garbage page search
		if(i/PAGES_PER_BLOCK != freeblock && sparedata[i].lpn == -1 || sparedata[i].lpn == lsn){
			garbageblock = i/PAGES_PER_BLOCK;
			break;
		}
	}
	
	for(i=0; i<PAGES_PER_BLOCK; i++){
		memset(pagebuf, (char)0xFF, PAGE_SIZE);
		pagenum = garbageblock*PAGES_PER_BLOCK + i;
	
		if(sparedata[pagenum].lpn == -1 || sparedata[pagenum].lpn == lsn){
			sparedata[freeblock*PAGES_PER_BLOCK+i].is_invalid = FALSE;
		}
		else{
			dd_read(pagenum, pagebuf);
			curlsn = pagebuf[SECTOR_SIZE];
			dd_write(freeblock*PAGES_PER_BLOCK + i, pagebuf);
			address_mapping_table[curlsn].ppn = freeblock*PAGES_PER_BLOCK + i;
			sparedata[freeblock*PAGES_PER_BLOCK+i].lpn = curlsn;
			sparedata[freeblock*PAGES_PER_BLOCK+i].is_invalid = TRUE;
		}
		sparedata[pagenum].lpn = -1;
	}

	if((ret = dd_erase(garbageblock)) < 0){
		fprintf(stderr, "erase error\n");
		exit(1);
	}

	//free block 초기화
	freeblock = garbageblock;
	for(i=0; i<PAGES_PER_BLOCK; i++){
		sparedata[freeblock*PAGES_PER_BLOCK + i].is_invalid = TRUE;
		sparedata[freeblock*PAGES_PER_BLOCK + i].lpn = -1;
	}

	for(i=0; i<BLOCKS_PER_DEVICE*PAGES_PER_BLOCK; i++){ //free page search
		if(sparedata[i].is_invalid == FALSE){
			freepage = i;
			break;
		}
	}

	return freepage;
}


void ftl_print()
{

	printf("lpn ppn\n");
	for(int i=0; i<DATAPAGES_PER_DEVICE; i++)
		printf("%d %d\n", address_mapping_table[i].lpn, address_mapping_table[i].ppn);

	printf("free block's pbn = %d\n", freeblock);


	return;
}
