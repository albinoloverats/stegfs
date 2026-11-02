#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define main stegfs_application_main
#include "../src/main.c"
#undef main

typedef struct
{
	char *image;
}
test_state_s;

static int setup_filesystem(void **state)
{
	test_state_s *test = calloc(1, sizeof *test);
	if (!test)
		return -1;
	test->image = strdup("/tmp/stegfs-unit-XXXXXX");
	if (!test->image)
	{
		free(test);
		return -1;
	}
	int fd = mkstemp(test->image);
	if (fd < 0 || ftruncate(fd, 1024 * 1024) < 0)
	{
		if (fd >= 0)
			close(fd);
		unlink(test->image);
		free(test->image);
		free(test);
		return -1;
	}
	close(fd);
	if (stegfs_init(test->image, true, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false) != STEGFS_INIT_OKAY)
	{
		unlink(test->image);
		free(test->image);
		free(test);
		return -1;
	}
	*state = test;
	return 0;
}

static int teardown_filesystem(void **state)
{
	test_state_s *test = *state;
	stegfs_deinit();
	if (test)
	{
		unlink(test->image);
		free(test->image);
		free(test);
	}
	return 0;
}

static void write_test_file(const char *path, const uint8_t *data, size_t size)
{
	stegfs_file_create(path, true);
	stegfs_cache_s *cached = stegfs_cache_exists(path, NULL);
	assert_non_null(cached);
	assert_non_null(cached->file);
	cached->file->size = size;
	if (size)
	{
		cached->file->data = malloc(size);
		assert_non_null(cached->file->data);
		memcpy(cached->file->data, data, size);
	}
	assert_true(stegfs_file_will_fit(cached->file));
	assert_true(stegfs_file_write(cached->file));
}

static void test_storage_round_trip_and_repeated_stat(void **state)
{
	(void)state;
	const size_t size = 8000;
	uint8_t *original = malloc(size);
	assert_non_null(original);
	for (size_t i = 0; i < size; i++)
		original[i] = (uint8_t)(i * 37U);
	write_test_file("/sample:secret", original, size);

	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("sample");
	file.pass = strdup("secret");
	assert_non_null(file.path);
	assert_non_null(file.name);
	assert_non_null(file.pass);
	assert_true(stegfs_file_stat(&file));
	uint64_t used = stegfs_info().blocks.used;
	assert_true(stegfs_file_stat(&file));
	assert_int_equal(stegfs_info().blocks.used, used);
	assert_true(stegfs_file_read(&file));
	assert_int_equal(file.size, size);
	assert_memory_equal(file.data, original, size);

	free(file.path);
	free(file.name);
	free(file.pass);
	free(file.data);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(file.blocks[i]);
	free(original);
}

static void test_empty_file_round_trip(void **state)
{
	(void)state;
	write_test_file("/empty:secret", NULL, 0);

	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("empty");
	file.pass = strdup("secret");
	assert_true(stegfs_file_read(&file));
	assert_int_equal(file.size, 0);
	assert_null(file.data);

	free(file.path);
	free(file.name);
	free(file.pass);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(file.blocks[i]);
}

static void test_sparse_write_truncate_and_read(void **state)
{
	(void)state;
	struct fuse_file_info info = { .flags = O_WRONLY };
	assert_int_equal(fuse_stegfs_create("/sparse:secret", 0600, &info), 0);
	assert_int_equal(fuse_stegfs_open("/sparse:secret", &info), 0);
	assert_int_equal(fuse_stegfs_write("/sparse:secret", "data", 4, 5, &info), 4);
	assert_int_equal(fuse_stegfs_release("/sparse:secret", &info), 0);

	info.flags = O_WRONLY;
	assert_int_equal(fuse_stegfs_open("/sparse:secret", &info), 0);
	assert_int_equal(fuse_stegfs_ftruncate("/sparse:secret", 12, &info), 0);
	assert_int_equal(fuse_stegfs_release("/sparse:secret", &info), 0);

	info.flags = O_RDONLY;
	assert_int_equal(fuse_stegfs_open("/sparse:secret", &info), 0);
	char output[12];
	memset(output, 0xff, sizeof output);
	assert_int_equal(fuse_stegfs_read("/sparse:secret", output, sizeof output, 0, &info), sizeof output);
	assert_memory_equal(output, "\0\0\0\0\0data\0\0\0\0", sizeof output);
	assert_int_equal(fuse_stegfs_read("/sparse:secret", output, sizeof output, 12, &info), 0);
	assert_int_equal(fuse_stegfs_read("/sparse:secret", output, sizeof output, -1, &info), -EINVAL);
	assert_int_equal(fuse_stegfs_release("/sparse:secret", &info), 0);
}

static void test_reject_invalid_image_size(void **state)
{
	test_state_s *test = *state;
	stegfs_deinit();
	int fd = open(test->image, O_RDWR);
	assert_true(fd >= 0);
	assert_int_equal(ftruncate(fd, SIZE_BYTE_BLOCK + 1), 0);
	assert_int_equal(close(fd), 0);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_CORRUPT_TAG);
}

static void test_initialization_preserves_stdin(void **state)
{
	(void)state;
	assert_true(fcntl(STDIN_FILENO, F_GETFD) >= 0);
}

static void test_reject_capacity_without_deleting_file(void **state)
{
	(void)state;
	const uint8_t original[] = "keep";
	write_test_file("/preserve:secret", original, sizeof original);
	stegfs_cache_s *cached = stegfs_cache_exists("/preserve:secret", NULL);
	assert_non_null(cached);
	stegfs_file_s candidate = *cached->file;
	candidate.size = UINT64_MAX;
	errno = 0;
	assert_false(stegfs_file_will_fit(&candidate));
	assert_int_equal(errno, ENOSPC);
	assert_non_null(stegfs_cache_exists("/preserve:secret", NULL));
}

static void test_wrong_password_is_rejected(void **state)
{
	(void)state;
	const uint8_t original[] = "authenticated";
	write_test_file("/private:secret", original, sizeof original);
	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("private");
	file.pass = strdup("incorrect");
	assert_false(stegfs_file_read(&file));
	assert_int_equal(errno, ENOENT);
	free(file.path);
	free(file.name);
	free(file.pass);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(file.blocks[i]);
}

static void test_read_falls_back_to_unmodified_copy(void **state)
{
	(void)state;
	const size_t size = 8000;
	uint8_t *original = malloc(size);
	assert_non_null(original);
	memset(original, 0x5a, size);
	write_test_file("/duplicated:secret", original, size);
	stegfs_cache_s *cached = stegfs_cache_exists("/duplicated", NULL);
	assert_non_null(cached);
	assert_non_null(cached->file->blocks[0]);
	assert_true(cached->file->blocks[0][0] > 0);

	stegfs_s filesystem = stegfs_info();
	uint64_t block_index = cached->file->blocks[0][1] % (filesystem.size / filesystem.blocksize);
	uint8_t *corrupt = (uint8_t *)filesystem.memory + block_index * filesystem.blocksize
		+ sizeof(uint64_t) * SIZE_LONG_PATH;
	corrupt[0] ^= 0x01;

	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("duplicated");
	file.pass = strdup("secret");
	assert_true(stegfs_file_read(&file));
	assert_memory_equal(file.data, original, size);
	free(file.path);
	free(file.name);
	free(file.pass);
	free(file.data);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(file.blocks[i]);
	free(original);
}

static void test_block_path_parser_bounds(void **state)
{
	(void)state;
	uint64_t block = 0;
	assert_true(block_index_from_path("/bloc/123", &block));
	assert_int_equal(block, 123);
	assert_false(block_index_from_path("/bloc/not-a-number", &block));
	assert_false(block_index_from_path("/bloc/18446744073709551616", &block));
}

static const struct CMUnitTest tests[] =
{
	cmocka_unit_test_setup_teardown(test_storage_round_trip_and_repeated_stat, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_empty_file_round_trip, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_sparse_write_truncate_and_read, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_reject_invalid_image_size, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_initialization_preserves_stdin, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_reject_capacity_without_deleting_file, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_wrong_password_is_rejected, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_read_falls_back_to_unmodified_copy, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_block_path_parser_bounds, setup_filesystem, teardown_filesystem)
};

int main(void)
{
	return _cmocka_run_group_tests("stegfs", tests, sizeof tests / sizeof tests[0], NULL, NULL);
}
