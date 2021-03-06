#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <zconf.h>
#include "cmockery.h"

#include "c.h"

#include "postgres.h"

/* Ignore ereport */
#include "utils/elog.h"
#undef ereport
#undef errcode
#undef errmsg
#define ereport
#define errcode
#define errmsg

#include "utils/palloc.h"
#include "utils/memutils.h"

#include "../cdbsrlz.c"

/*
 * Returns the amount of memory needed for zlib internal allocations
 *
 * isWrite: Set to true if opening a file for writing
 */
int
zlib_memory_needed(bool isWrite)
{
	/*
	 * This values are set in zconf.h, based on local configuration
	 */
	int def_mem_level = MAX_MEM_LEVEL;
	if (MAX_MEM_LEVEL >= 8)
	{
		def_mem_level = 8;
	}
	int def_wbits = MAX_WBITS;
	int memZlib = -1;

	/*
	 * The formula for computing the memory needed is described in
	 * zconf.h. For zlib 1.2.3, it is as follows:
	 *
	 * The memory requirements for deflate are (in bytes):
	 *      (1 << (windowBits+2)) + (1 << (memLevel+9))
	 * that is: 128K for windowBits=15 plus 128K for memLevel=8
	 * (default values) plus a few kilobytes for small objects.
	 *
	 * The memory requirements for inflate are (in bytes)
	 *      1 << windowBits
	 * that is, 32K for windowBits=15 (default value) plus a few
	 * kilobytes for small objects.
	 */
	if (isWrite)
	{
		memZlib =  (1 << (def_wbits+2)) +  (1 << (def_mem_level+9));
	}
	else
	{
		memZlib = (1 << def_wbits);
	}

	return memZlib;
}

char *uncompressedString = NULL;
char *compressedString = NULL;
int compressed_size = 0;

/*
 * Populates the global variable 'input' with a randomly generated string of length 'size'.
 */
void
populate_string(int size)
{
	static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
	uncompressedString = (char *) calloc(size + 1, sizeof(char));
	for (int i = 0; i < size; i++)
	{
		int key = rand() % (int)(sizeof(charset)-1);
		uncompressedString[i] = charset[key];
	}
	uncompressedString[size] = '\0';
}

/*
 * Test that compress_string uses palloc() to allocate memory for zlib internal buffers.
 */
void
test__compress_string__palloc_compress(void **state)
{

	populate_string(1 << 16);

	int uncompressed_size = strlen(uncompressedString);

	MemoryContextSetPeakSpace(TopMemoryContext, 0);
	Size beforeAlloc = MemoryContextGetPeakSpace(TopMemoryContext);

	compressedString = compress_string(uncompressedString, uncompressed_size, &compressed_size);
	assert_true(NULL != compressedString);

	Size afterAlloc = MemoryContextGetPeakSpace(TopMemoryContext);

	int memZlib = zlib_memory_needed(true/* isWrite */);

	assert_true(afterAlloc - beforeAlloc > memZlib);
}

/*
 * Test that uncompress_string uses palloc() to allocate memory for zlib internal buffers.
 * This test uses the compressed string generated by the first test as an input, so it must run after it.
 */
void
test__uncompress_string__palloc_uncompress(void **state)
{

	/*
	 * compressedOutput must be already initialized by running the test__compress_string__palloc_compress test first.
	 */
	assert_true(NULL != compressedString);
	assert_true(0 != compressed_size);

	MemoryContextSetPeakSpace(TopMemoryContext, 0);
	Size beforeAlloc = MemoryContextGetPeakSpace(TopMemoryContext);
	int uncompressed_size = 0;

	char *output = uncompress_string(compressedString, compressed_size, &uncompressed_size);
	assert_true(NULL != output);
	assert_true(strlen(uncompressedString) == uncompressed_size);

	Size afterAlloc = MemoryContextGetPeakSpace(TopMemoryContext);

	int memZlib = zlib_memory_needed(false/* isWrite */);

	assert_true(afterAlloc - beforeAlloc > memZlib);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] =
	{
		unit_test(test__compress_string__palloc_compress),
		unit_test(test__uncompress_string__palloc_uncompress)
	};

	MemoryContextInit();

	return run_tests(tests);
}
