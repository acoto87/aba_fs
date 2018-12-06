/*
 ============================================================================
 Name        : AbaFormater.c
 Author      : Aba
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#define FUSE_USE_VERSION 25
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include "formater.h"

int main(int argc, char *argv[]) {
	if(argc <= 1)
		return -1;
	int result;
	if(argc == 2)
		result = FormatDisk(argv[1], 20971520, 1024);
	else
		result = FormatDisk(argv[1], atol(argv[2]), atol(argv[3]));

	return result;
}
