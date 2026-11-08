/*
 * Unit tests.
 * Copyright © 2026, albinoloverats ~ Software Development
 * email: webmaster@albinoloverats.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

#define fuse_get_context test_fuse_get_context
#define main stegfs_application_main
#include "../src/main.c"
#undef main
#undef fuse_get_context

struct fuse_context *test_fuse_get_context(void)
{
	static struct fuse_context context;
	return &context;
}

typedef struct
{
	char *image;
}
test_state_s;

static int setup_filesystem_config(void **state, bool show_bloc, uint64_t image_size, uint32_t copies)
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
	if (fd < 0 || ftruncate(fd, image_size) < 0)
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
			DEFAULT_HASH, DEFAULT_MAC, 10, copies, show_bloc) != STEGFS_INIT_OKAY)
	{
		unlink(test->image);
		free(test->image);
		free(test);
		return -1;
	}
	*state = test;
	return 0;
}

static int setup_filesystem(void **state)
{
	return setup_filesystem_config(state, false, 1024 * 1024, 2);
}

static int setup_filesystem_with_bloc(void **state)
{
	return setup_filesystem_config(state, true, 1024 * 1024, 2);
}

static int setup_small_filesystem(void **state)
{
	return setup_filesystem_config(state, false, 32 * SIZE_BYTE_BLOCK, 2);
}

static int setup_filesystem_one_copy(void **state)
{
	test_state_s *test = NULL;
	if (setup_filesystem_config(state, false, 1024 * 1024, 2) < 0)
		return -1;
	test = *state;
	stegfs_deinit();
	if (stegfs_init(test->image, true, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 1, false) != STEGFS_INIT_OKAY)
		return -1;
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

static bool append_superblock_tag(uint8_t *data, size_t *offset, uint64_t *count,
		uint8_t tag, const void *value, uint16_t length)
{
	size_t remaining = SIZE_BYTE_DATA - *offset;
	if (remaining < sizeof tag + sizeof(uint16_t)
			|| length > remaining - sizeof tag - sizeof(uint16_t))
		return false;
	uint16_t network_length = htons(length);
	memcpy(data + *offset, &tag, sizeof tag);
	*offset += sizeof tag;
	memcpy(data + *offset, &network_length, sizeof network_length);
	*offset += sizeof network_length;
	memcpy(data + *offset, value, length);
	*offset += length;
	(*count)++;
	return true;
}

static void write_superblock_with_options(test_state_s *test, uint8_t copies, uint32_t blocksize,
		uint32_t head_offset, const char *cipher_name, const char *version_name,
		const char *mac_name, uint64_t kdf_iterations, uint8_t duplication_length,
		int omitted_tag)
{
	stegfs_block_s block = { 0 };
	block.hash[0] = htonll(HASH_MAGIC_0);
	block.hash[1] = htonll(HASH_MAGIC_1);
	block.hash[2] = htonll(HASH_MAGIC_2);
	block.next = htonll(1024 * 1024 / SIZE_BYTE_BLOCK);

	size_t offset = sizeof(uint64_t);
	uint64_t tag_count = 0;
	assert_true(append_superblock_tag(block.data, &offset, &tag_count,
			TAG_STEGFS, STEGFS_NAME, strlen(STEGFS_NAME)));
	assert_true(append_superblock_tag(block.data, &offset, &tag_count,
			TAG_VERSION, version_name, strlen(version_name)));
	uint32_t network_value = htonl(blocksize);
	if (omitted_tag != TAG_BLOCKSIZE)
		assert_true(append_superblock_tag(block.data, &offset, &tag_count,
				TAG_BLOCKSIZE, &network_value, sizeof network_value));
	network_value = htonl(head_offset);
	if (omitted_tag != TAG_HEADER_OFFSET)
		assert_true(append_superblock_tag(block.data, &offset, &tag_count,
				TAG_HEADER_OFFSET, &network_value, sizeof network_value));
	if (omitted_tag != TAG_CIPHER)
		assert_true(append_superblock_tag(block.data, &offset, &tag_count,
				TAG_CIPHER, cipher_name, strlen(cipher_name)));
	const char *mode_name = mode_name_from_id(DEFAULT_MODE);
	const char *hash_name = hash_name_from_id(DEFAULT_HASH);
	assert_true(append_superblock_tag(block.data, &offset, &tag_count,
			TAG_MODE, mode_name, strlen(mode_name)));
	assert_true(append_superblock_tag(block.data, &offset, &tag_count,
			TAG_HASH, hash_name, strlen(hash_name)));
	assert_true(append_superblock_tag(block.data, &offset, &tag_count,
			TAG_MAC, mac_name, strlen(mac_name)));
	if (omitted_tag != TAG_DUPLICATION)
	{
		uint8_t duplication[sizeof(uint32_t)] = { copies, 0, 0, 0 };
		assert_true(append_superblock_tag(block.data, &offset, &tag_count,
				TAG_DUPLICATION, duplication, duplication_length));
	}
	uint64_t kdf = htonll(kdf_iterations);
	assert_true(append_superblock_tag(block.data, &offset, &tag_count,
			TAG_KDF, &kdf, sizeof kdf));

	tag_count = htonll(tag_count);
	memcpy(block.data, &tag_count, sizeof tag_count);

	int fd = open(test->image, O_RDWR);
	assert_true(fd >= 0);
	assert_int_equal(pwrite(fd, &block, sizeof block, 0), sizeof block);
	assert_int_equal(close(fd), 0);
}

static void write_superblock(test_state_s *test, uint8_t copies, uint32_t blocksize,
		uint32_t head_offset, const char *cipher_name, int omitted_tag)
{
	write_superblock_with_options(test, copies, blocksize, head_offset, cipher_name,
			STEGFS_VERSION, mac_name_from_id(DEFAULT_MAC), 10, sizeof(uint8_t), omitted_tag);
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
	if (!stegfs_file_write(cached->file))
		fail_msg("writing %s failed: %s", path, strerror(errno));
}

typedef struct
{
	unsigned id;
	int result;
}
concurrent_writer_s;

static void *concurrent_writer(void *argument)
{
	concurrent_writer_s *writer = argument;
	char path[64];
	char content[64];
	snprintf(path, sizeof path, "/concurrent-%u:secret", writer->id);
	int length = snprintf(content, sizeof content, "concurrent payload %u", writer->id);
	struct fuse_file_info info = { .flags = O_WRONLY };

	if (fuse_stegfs_functions.create(path, 0600, &info) != 0
			|| fuse_stegfs_functions.write(path, content, length, 0, &info) != length
			|| fuse_stegfs_functions.release(path, &info) != 0)
		writer->result = -1;
	return NULL;
}

static void *concurrent_reader(void *argument)
{
	concurrent_writer_s *reader = argument;
	char path[64];
	char expected[64];
	char actual[64] = { 0 };
	snprintf(path, sizeof path, "/concurrent-%u:secret", reader->id);
	int length = snprintf(expected, sizeof expected, "concurrent payload %u", reader->id);
	struct fuse_file_info info = { .flags = O_RDONLY };

	if (fuse_stegfs_functions.open(path, &info) != 0
			|| fuse_stegfs_functions.read(path, actual, sizeof actual, 0, &info) != length
			|| memcmp(actual, expected, length)
			|| fuse_stegfs_functions.release(path, &info) != 0)
		reader->result = -1;
	return NULL;
}

/*!
 * \brief Verifies concurrent FUSE writes and reads preserve each file's contents.
 */
static void test_concurrent_fuse_requests_preserve_filesystem_state(void **state)
{
	(void)state;
	enum { THREADS = 8 };
	pthread_t threads[THREADS];
	concurrent_writer_s writers[THREADS] = { 0 };
	for (unsigned i = 0; i < THREADS; i++)
	{
		writers[i].id = i;
		assert_int_equal(pthread_create(&threads[i], NULL, concurrent_writer, &writers[i]), 0);
	}
	for (unsigned i = 0; i < THREADS; i++)
	{
		assert_int_equal(pthread_join(threads[i], NULL), 0);
		assert_int_equal(writers[i].result, 0);
	}

	/* Verify every payload only after all concurrent writers have finished. */
	for (unsigned i = 0; i < THREADS; i++)
	{
		writers[i].result = 0;
		assert_int_equal(pthread_create(&threads[i], NULL, concurrent_reader, &writers[i]), 0);
	}
	for (unsigned i = 0; i < THREADS; i++)
	{
		assert_int_equal(pthread_join(threads[i], NULL), 0);
		assert_int_equal(writers[i].result, 0);
	}
}

/*!
 * \brief Verifies overlapping handles enforce access modes and share live file state.
 */
static void test_overlapping_handles_keep_independent_access_and_lifetime(void **state)
{
	(void)state;
	const uint8_t original[] = "shared initial contents";
	const uint8_t replacement[] = "updated";
	write_test_file("/shared:secret", original, sizeof original);

	struct fuse_file_info reader = { .flags = O_RDONLY };
	struct fuse_file_info writer = { .flags = O_WRONLY };
	char output[sizeof original] = { 0 };
	assert_int_equal(fuse_stegfs_functions.open("/shared:secret", &reader), 0);
	assert_int_equal(fuse_stegfs_functions.open("/shared:secret", &writer), 0);
	assert_int_equal(fuse_stegfs_functions.write("/shared:secret", (const char *)replacement,
			sizeof replacement, 0, &reader), -EBADF);
	assert_int_equal(fuse_stegfs_functions.read("/shared:secret", output, sizeof output,
			0, &writer), -EBADF);
	assert_int_equal(fuse_stegfs_functions.write("/shared:secret", (const char *)replacement,
			sizeof replacement, 0, &writer), sizeof replacement);
	assert_int_equal(fuse_stegfs_functions.read("/shared:secret", output, sizeof output,
			0, &reader), sizeof output);
	assert_memory_equal(output, replacement, sizeof replacement);
	assert_memory_equal(output + sizeof replacement, original + sizeof replacement,
			sizeof original - sizeof replacement);
	assert_int_equal(fuse_stegfs_functions.ftruncate("/shared:secret", sizeof replacement, &writer), 0);
	memset(output, 0, sizeof output);
	assert_int_equal(fuse_stegfs_functions.read("/shared:secret", output, sizeof output,
			0, &reader), sizeof replacement);
	assert_memory_equal(output, replacement, sizeof replacement);

	/* Releasing one handle must not invalidate another handle's shared file state. */
	struct fuse_file_info wrong_password = { .flags = O_RDONLY };
	assert_int_equal(fuse_stegfs_functions.open("/shared:wrong", &wrong_password), -EACCES);
	assert_int_equal(fuse_stegfs_functions.release("/shared:secret", &writer), 0);
	assert_int_equal(fuse_stegfs_functions.read("/shared:secret", output, sizeof output,
			0, &reader), sizeof replacement);
	assert_memory_equal(output, replacement, sizeof replacement);
	assert_int_equal(fuse_stegfs_functions.release("/shared:secret", &reader), 0);

	stegfs_file_s persisted = { 0 };
	persisted.path = strdup("/");
	persisted.name = strdup("shared");
	persisted.pass = strdup("secret");
	assert_true(stegfs_file_read(&persisted));
	assert_int_equal(persisted.size, sizeof replacement);
	assert_memory_equal(persisted.data, replacement, sizeof replacement);
	free(persisted.path);
	free(persisted.name);
	free(persisted.pass);
	free(persisted.data);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(persisted.blocks[i]);
}

/*!
 * \brief Verifies an unlinked open file remains distinct from a recreated path.
 */
static void test_unlink_open_file_then_recreate_same_path(void **state)
{
	(void)state;
	const uint8_t original[] = "old generation";
	const uint8_t replacement[] = "new generation";
	write_test_file("/recreated:secret", original, sizeof original);
	uint64_t blocks_before = stegfs_info().blocks.used;

	struct fuse_file_info old_reader = { .flags = O_RDONLY };
	assert_int_equal(fuse_stegfs_functions.open("/recreated:secret", &old_reader), 0);
	assert_int_equal(fuse_stegfs_functions.unlink("/recreated:secret"), 0);
	struct stat stbuf = { 0 };
	assert_int_equal(fuse_stegfs_functions.getattr("/recreated:secret", &stbuf), -ENOENT);

	char output[sizeof original] = { 0 };
	assert_int_equal(fuse_stegfs_functions.read("/recreated:secret", output, sizeof output,
			0, &old_reader), sizeof original);
	assert_memory_equal(output, original, sizeof original);

	/* Reusing the pathname creates a separate generation from the unlinked open file. */
	struct fuse_file_info new_writer = { .flags = O_RDWR };
	assert_int_equal(fuse_stegfs_functions.create("/recreated:secret", 0600, &new_writer), 0);
	open_file_s *old_opened = open_file_lookup(&old_reader);
	open_file_s *new_opened = open_file_lookup(&new_writer);
	assert_non_null(old_opened);
	assert_non_null(new_opened);
	assert_ptr_not_equal(old_opened->file, new_opened->file);
	assert_false(new_opened->file->unlinked);
	assert_int_equal(fuse_stegfs_functions.write("/recreated:secret", (const char *)replacement,
			sizeof replacement, 0, &new_writer), sizeof replacement);
	assert_int_equal(fuse_stegfs_functions.release("/recreated:secret", &new_writer), 0);
	assert_int_equal(fuse_stegfs_functions.release("/recreated:secret", &old_reader), 0);
	assert_int_equal(stegfs_info().blocks.used, blocks_before);
	struct fuse_file_info new_reader = { .flags = O_RDONLY };
	assert_int_equal(fuse_stegfs_functions.open("/recreated:secret", &new_reader), 0);
	memset(output, 0, sizeof output);
	assert_int_equal(fuse_stegfs_functions.read("/recreated:secret", output, sizeof output,
			0, &new_reader), sizeof replacement);
	assert_memory_equal(output, replacement, sizeof replacement);
	assert_int_equal(fuse_stegfs_functions.release("/recreated:secret", &new_reader), 0);
}

/*!
 * \brief Verifies releasing a newly created empty file persists it for reopening.
 */
static void test_empty_fuse_create_is_persisted_on_release(void **state)
{
	(void)state;
	struct fuse_file_info created = { .flags = O_RDONLY };
	assert_int_equal(fuse_stegfs_functions.create("/empty-created:secret", 0600, &created), 0);
	assert_int_equal(fuse_stegfs_functions.release("/empty-created:secret", &created), 0);

	struct fuse_file_info reopened = { .flags = O_RDONLY };
	assert_int_equal(fuse_stegfs_functions.open("/empty-created:secret", &reopened), 0);
	char buffer = 0;
	assert_int_equal(fuse_stegfs_functions.read("/empty-created:secret", &buffer, 1, 0, &reopened), 0);
	assert_int_equal(fuse_stegfs_functions.release("/empty-created:secret", &reopened), 0);
}

/*!
 * \brief Verifies file contents survive storage round-trip and repeated stat calls.
 */
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

/*!
 * \brief Verifies storage round-trips for payloads around block boundaries.
 */
static void test_storage_round_trips_at_block_boundaries(void **state)
{
	(void)state;
	const size_t sizes[] = { 1, SIZE_BYTE_HEAD, SIZE_BYTE_HEAD + 1,
		SIZE_BYTE_HEAD + SIZE_BYTE_DATA, SIZE_BYTE_HEAD + SIZE_BYTE_DATA + 1,
		2 * SIZE_BYTE_DATA + SIZE_BYTE_HEAD };
	for (size_t n = 0; n < sizeof sizes / sizeof sizes[0]; n++)
	{
		size_t size = sizes[n];
		uint8_t *original = malloc(size);
		assert_non_null(original);
		for (size_t i = 0; i < size; i++)
			original[i] = (uint8_t)(i + n * 13);
		char path[64];
		snprintf(path, sizeof path, "/boundary%zu:secret", n);
		write_test_file(path, original, size);

		stegfs_file_s file = { 0 };
		file.path = strdup("/");
		char name[32];
		snprintf(name, sizeof name, "boundary%zu", n);
		file.name = strdup(name);
		file.pass = strdup("secret");
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
}

/*!
 * \brief Verifies file data persists after normal filesystem reinitialization.
 */
static void test_storage_persists_across_normal_remount(void **state)
{
	test_state_s *test = *state;
	stegfs_deinit();
	write_superblock(test, 2, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_OKAY);
	assert_int_equal(stegfs_info().version, VERSION_202X_XX);

	const size_t size = SIZE_BYTE_HEAD + SIZE_BYTE_DATA + 17;
	uint8_t *original = malloc(size);
	assert_non_null(original);
	for (size_t i = 0; i < size; i++)
		original[i] = (uint8_t)(i * 19U + 7U);
	write_test_file("/persistent:secret", original, size);
	stegfs_deinit();

	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_OKAY);
	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("persistent");
	file.pass = strdup("secret");
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

/*!
 * \brief Verifies formatted-image data can be read after a normal remount.
 */
static void test_storage_persists_across_formatter_image(void **state)
{
	test_state_s *test = *state;
	stegfs_deinit();
	assert_int_equal(unlink(test->image), 0);

	pid_t child = fork();
	assert_true(child >= 0);
	if (!child)
	{
		int output = open("/dev/null", O_WRONLY);
		if (output >= 0)
		{
			dup2(output, STDOUT_FILENO);
			close(output);
		}
		execl("./mkstegfs", "mkstegfs", "-z", "1M", "-x", "2",
				"-i", "10", test->image, (char *)NULL);
		_exit(EXIT_FAILURE);
	}
	int status = 0;
	assert_int_equal(waitpid(child, &status, 0), child);
	assert_true(WIFEXITED(status));
	assert_int_equal(WEXITSTATUS(status), EXIT_SUCCESS);

	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_OKAY);
	const size_t size = SIZE_BYTE_HEAD + 2 * SIZE_BYTE_DATA + 29;
	uint8_t *original = malloc(size);
	assert_non_null(original);
	for (size_t i = 0; i < size; i++)
		original[i] = (uint8_t)(i * 31U + 5U);
	write_test_file("/formatted:secret", original, size);
	stegfs_deinit();

	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_OKAY);
	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("formatted");
	file.pass = strdup("secret");
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

/*!
 * \brief Verifies overwriting a file persists the replacement contents.
 */
static void test_overwrite_existing_file_and_remount(void **state)
{
	test_state_s *test = *state;
	const uint8_t original[] = "original contents that span";
	const uint8_t replacement[] = "new";
	write_test_file("/overwrite:secret", original, sizeof original);
	struct fuse_file_info info = { .flags = O_RDWR };
	assert_int_equal(fuse_stegfs_open("/overwrite:secret", &info), 0);
	assert_int_equal(fuse_stegfs_ftruncate("/overwrite:secret", 0, &info), 0);
	assert_int_equal(fuse_stegfs_write("/overwrite:secret", (const char *)replacement,
			sizeof replacement, 0, &info), sizeof replacement);
	assert_int_equal(fuse_stegfs_release("/overwrite:secret", &info), 0);

	stegfs_deinit();
	assert_int_equal(stegfs_init(test->image, true, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_OKAY);
	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("overwrite");
	file.pass = strdup("secret");
	assert_true(stegfs_file_read(&file));
	assert_int_equal(file.size, sizeof replacement);
	assert_memory_equal(file.data, replacement, sizeof replacement);
	free(file.path);
	free(file.name);
	free(file.pass);
	free(file.data);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(file.blocks[i]);
}

/*!
 * \brief Verifies an empty file can be written, read, and reopened.
 */
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

/*!
 * \brief Verifies sparse writes produce zero-filled gaps and valid read bounds.
 */
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

/*!
 * \brief Verifies initialization rejects backing images with invalid sizes.
 */
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

/*!
 * \brief Verifies initialization rejects an impossible superblock tag count.
 */
static void test_reject_malformed_superblock_tag_count(void **state)
{
	test_state_s *test = *state;
	stegfs_deinit();
	int fd = open(test->image, O_RDWR);
	assert_true(fd >= 0);

	/* Keep the magic valid so initialization rejects the impossible tag count itself. */
	stegfs_block_s block = { 0 };
	block.hash[0] = htonll(HASH_MAGIC_0);
	block.hash[1] = htonll(HASH_MAGIC_1);
	block.hash[2] = htonll(HASH_MAGIC_2);
	uint64_t tag_count = htonll(TAG_MAX + 1);
	memcpy(block.data, &tag_count, sizeof tag_count);
	assert_int_equal(pwrite(fd, &block, sizeof block, 0), sizeof block);
	assert_int_equal(close(fd), 0);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_CORRUPT_TAG);
}

/*!
 * \brief Verifies initialization rejects a TLV value truncated by the superblock.
 */
static void test_reject_truncated_superblock_tlv(void **state)
{
	test_state_s *test = *state;
	stegfs_deinit();
	int fd = open(test->image, O_RDWR);
	assert_true(fd >= 0);

	/* The declared value length exceeds the bytes available in the superblock. */
	stegfs_block_s block = { 0 };
	block.hash[0] = htonll(HASH_MAGIC_0);
	block.hash[1] = htonll(HASH_MAGIC_1);
	block.hash[2] = htonll(HASH_MAGIC_2);
	uint64_t tag_count = htonll(1);
	memcpy(block.data, &tag_count, sizeof tag_count);
	block.data[sizeof tag_count] = TAG_STEGFS;
	uint16_t length = htons(UINT16_MAX);
	memcpy(block.data + sizeof tag_count + sizeof(uint8_t), &length, sizeof length);
	assert_int_equal(pwrite(fd, &block, sizeof block, 0), sizeof block);
	assert_int_equal(close(fd), 0);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_CORRUPT_TAG);
}

/*!
 * \brief Verifies superblock metadata values, required tags, and version handling.
 */
static void test_superblock_metadata_validation_matrix(void **state)
{
	test_state_s *test = *state;
	stegfs_deinit();

	/* Distinguish invalid metadata values from malformed or missing tags. */
	write_superblock(test, 2, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			"not-a-cipher", -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_INVALID_TAG);

	write_superblock(test, 2, SIZE_BYTE_BLOCK + 1, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_CORRUPT_TAG);

	write_superblock(test, 2, SIZE_BYTE_BLOCK, 1,
			cipher_name_from_id(DEFAULT_CIPHER), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_CORRUPT_TAG);

	write_superblock(test, 0, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_INVALID_TAG);

	write_superblock(test, COPIES_MAX + 1, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_INVALID_TAG);

	write_superblock(test, 2, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), TAG_HEADER_OFFSET);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_MISSING_TAG);

	write_superblock(test, 1, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 1, false), STEGFS_INIT_OKAY);
	stegfs_deinit();

	write_superblock(test, COPIES_MAX, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, COPIES_MAX, false), STEGFS_INIT_OKAY);
	assert_int_equal(stegfs_info().copies, COPIES_MAX);

	write_superblock_with_options(test, 2, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), "unsupported", mac_name_from_id(DEFAULT_MAC),
			10, sizeof(uint8_t), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_OLD_STEGFS);

	write_superblock_with_options(test, 2, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), STEGFS_VERSION, "not-a-mac",
			10, sizeof(uint8_t), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_INVALID_TAG);

	write_superblock_with_options(test, 2, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), STEGFS_VERSION, mac_name_from_id(DEFAULT_MAC),
			0, sizeof(uint8_t), -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_INVALID_TAG);

	write_superblock_with_options(test, 2, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), STEGFS_VERSION, mac_name_from_id(DEFAULT_MAC),
			10, 2, -1);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_INVALID_TAG);

	write_superblock(test, 2, SIZE_BYTE_BLOCK, OFFSET_BYTE_HEAD,
			cipher_name_from_id(DEFAULT_CIPHER), -1);
	int fd = open(test->image, O_RDWR);
	assert_true(fd >= 0);
	uint64_t bad_magic = 0;
	assert_int_equal(pwrite(fd, &bad_magic, sizeof bad_magic, offsetof(stegfs_block_s, hash)), sizeof bad_magic);
	assert_int_equal(close(fd), 0);
	assert_int_equal(stegfs_init(test->image, false, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_NOT_STEGFS);
}

/*!
 * \brief Verifies filesystem initialization does not close standard input.
 */
static void test_initialization_preserves_stdin(void **state)
{
	(void)state;
	assert_true(fcntl(STDIN_FILENO, F_GETFD) >= 0);
}

/*!
 * \brief Verifies repeated initialization and deinitialization are safe.
 */
static void test_repeated_init_and_deinit(void **state)
{
	test_state_s *test = *state;
	assert_int_equal(stegfs_init(test->image, true, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_OKAY);
	assert_true(fcntl(STDIN_FILENO, F_GETFD) >= 0);
	stegfs_deinit();
	stegfs_deinit();
	assert_int_equal(stegfs_init(test->image, true, DEFAULT_CIPHER, DEFAULT_MODE,
			DEFAULT_HASH, DEFAULT_MAC, 10, 2, false), STEGFS_INIT_OKAY);
}

/*!
 * \brief Verifies capacity rejection leaves an existing cached file intact.
 */
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

/*!
 * \brief Verifies failed allocation releases partial blocks without damaging files.
 */
static void test_small_image_allocation_failure_releases_partial_blocks(void **state)
{
	(void)state;
	const uint8_t original[] = "keep";
	write_test_file("/keep:secret", original, sizeof original);
	uint64_t used_before = stegfs_info().blocks.used;
	assert_true(used_before > 1);

	stegfs_file_s failed = { 0 };
	failed.path = strdup("/");
	failed.name = strdup("too-large");
	failed.pass = strdup("secret");
	failed.size = SIZE_BYTE_DATA - OFFSET_BYTE_HEAD + 15 * SIZE_BYTE_DATA;
	failed.data = calloc(failed.size, 1);
	assert_non_null(failed.data);
	errno = 0;
	assert_false(stegfs_file_write(&failed));
	assert_int_equal(errno, ENOSPC);
	/* Failed allocation must leave the allocator reusable and the old file intact. */
	assert_int_equal(stegfs_info().blocks.used, used_before);

	const uint8_t subsequent[] = "small";
	write_test_file("/after-failure:secret", subsequent, sizeof subsequent);
	assert_true(stegfs_info().blocks.used > used_before);

	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("keep");
	file.pass = strdup("secret");
	assert_true(stegfs_file_read(&file));
	assert_int_equal(file.size, sizeof original);
	assert_memory_equal(file.data, original, sizeof original);

	free(failed.path);
	free(failed.name);
	free(failed.pass);
	free(failed.data);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(failed.blocks[i]);
	free(file.path);
	free(file.name);
	free(file.pass);
	free(file.data);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(file.blocks[i]);
}

/*!
 * \brief Verifies failed growth preserves the prior contents and block allocation.
 */
static void test_existing_file_growth_failure_preserves_contents_and_blocks(void **state)
{
	(void)state;
	const size_t inline_capacity = SIZE_BYTE_DATA - OFFSET_BYTE_HEAD;
	const size_t original_size = inline_capacity + 1;
	const size_t grown_size = inline_capacity + 6 * SIZE_BYTE_DATA;
	uint8_t *original = malloc(original_size);
	assert_non_null(original);
	for (size_t i = 0; i < original_size; i++)
		original[i] = (uint8_t)(i * 17U + 3U);
	write_test_file("/grow:secret", original, original_size);

	uint8_t *filler = calloc(inline_capacity + 250 * SIZE_BYTE_DATA, 1);
	assert_non_null(filler);
	write_test_file("/filler:secret", filler, inline_capacity + 250 * SIZE_BYTE_DATA);
	free(filler);

	stegfs_cache_s *cached = stegfs_cache_exists("/grow:secret", NULL);
	assert_non_null(cached);
	assert_non_null(cached->file);
	stegfs_file_s candidate = *cached->file;
	candidate.size = grown_size;
	errno = 0;
	assert_false(stegfs_file_will_fit(&candidate));
	assert_int_equal(errno, ENOSPC);

	uint64_t used_before = stegfs_info().blocks.used;
	cached->file->data = realloc(cached->file->data, grown_size);
	assert_non_null(cached->file->data);
	memset(cached->file->data + original_size, 0, grown_size - original_size);
	cached->file->size = grown_size;
	errno = 0;
	assert_false(stegfs_file_write(cached->file));
	assert_int_equal(errno, ENOSPC);
	/* No newly reserved blocks or partial contents may survive a failed growth. */
	assert_int_equal(stegfs_info().blocks.used, used_before);

	stegfs_file_s persisted = { 0 };
	persisted.path = strdup("/");
	persisted.name = strdup("grow");
	persisted.pass = strdup("secret");
	assert_true(stegfs_file_read(&persisted));
	assert_int_equal(persisted.size, original_size);
	assert_memory_equal(persisted.data, original, original_size);

	free(persisted.path);
	free(persisted.name);
	free(persisted.pass);
	free(persisted.data);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(persisted.blocks[i]);
	free(original);
}

/*!
 * \brief Verifies the maximum file capacity and rejection one block beyond it.
 */
static void test_file_capacity_exact_fit_and_one_block_over(void **state)
{
	(void)state;
	const uint64_t inline_capacity = SIZE_BYTE_DATA - OFFSET_BYTE_HEAD;
	stegfs_file_s file = { 0 };
	file.size = inline_capacity + 510 * SIZE_BYTE_DATA;
	assert_true(stegfs_file_will_fit(&file));
	file.size++;
	assert_false(stegfs_file_will_fit(&file));
	assert_int_equal(errno, EFBIG);
}

/*!
 * \brief Verifies multi-block growth, shrinkage, and zero-filled extension.
 */
static void test_file_growth_and_shrink_across_multiple_blocks(void **state)
{
	(void)state;
	const size_t initial_size = SIZE_BYTE_HEAD + 4 * SIZE_BYTE_DATA + 31;
	const size_t shrink_size = SIZE_BYTE_HEAD + SIZE_BYTE_DATA + 9;
	const size_t extended_size = shrink_size + 11;
	uint8_t *original = malloc(initial_size);
	assert_non_null(original);
	for (size_t i = 0; i < initial_size; i++)
		original[i] = (uint8_t)(i * 23U + 11U);

	struct fuse_file_info info = { .flags = O_WRONLY };
	assert_int_equal(fuse_stegfs_create("/resize-many:secret", 0600, &info), 0);
	assert_int_equal(fuse_stegfs_open("/resize-many:secret", &info), 0);
	assert_int_equal(fuse_stegfs_write("/resize-many:secret", (char *)original,
			initial_size, 0, &info), initial_size);
	assert_int_equal(fuse_stegfs_release("/resize-many:secret", &info), 0);
	uint64_t allocated_before_shrink = stegfs_info().blocks.used;

	info.flags = O_RDWR;
	assert_int_equal(fuse_stegfs_open("/resize-many:secret", &info), 0);
	assert_int_equal(fuse_stegfs_ftruncate("/resize-many:secret", shrink_size, &info), 0);
	assert_int_equal(fuse_stegfs_release("/resize-many:secret", &info), 0);
	assert_true(stegfs_info().blocks.used < allocated_before_shrink);

	assert_int_equal(fuse_stegfs_open("/resize-many:secret", &info), 0);
	assert_int_equal(fuse_stegfs_ftruncate("/resize-many:secret", extended_size, &info), 0);
	assert_int_equal(fuse_stegfs_release("/resize-many:secret", &info), 0);
	info.flags = O_RDONLY;
	assert_int_equal(fuse_stegfs_open("/resize-many:secret", &info), 0);
	uint8_t *output = calloc(extended_size, sizeof *output);
	assert_non_null(output);
	assert_int_equal(fuse_stegfs_read("/resize-many:secret", (char *)output,
			extended_size, 0, &info), extended_size);
	assert_memory_equal(output, original, shrink_size);
	for (size_t i = shrink_size; i < extended_size; i++)
		assert_int_equal(output[i], 0);
	assert_int_equal(fuse_stegfs_release("/resize-many:secret", &info), 0);
	free(output);
	free(original);
}

/*!
 * \brief Verifies file reads fail when the supplied password is incorrect.
 */
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

/*!
 * \brief Verifies file reads recover from a damaged data replica.
 */
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

	/* One damaged replica should be bypassed in favor of an intact copy. */
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

/*!
 * \brief Verifies file reads locate a valid backup when the first inode is corrupt.
 */
static void test_read_uses_backup_when_first_inode_is_corrupt(void **state)
{
	(void)state;
	const size_t size = SIZE_BYTE_HEAD + SIZE_BYTE_DATA + 53;
	uint8_t *original = malloc(size);
	assert_non_null(original);
	memset(original, 0x6d, size);
	write_test_file("/inode-backup:secret", original, size);
	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("inode-backup");
	file.pass = strdup("secret");
	assert_true(stegfs_file_stat(&file));
	stegfs_s filesystem = stegfs_info();
	uint64_t block_index = file.inodes[0] % (filesystem.size / filesystem.blocksize);
	uint8_t *corrupt = (uint8_t *)filesystem.memory
		+ block_index * filesystem.blocksize + sizeof(uint64_t) * SIZE_LONG_PATH;
	corrupt[0] ^= 0x01;

	/* Data blocks remain intact; inode discovery must continue to its backup. */
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

/*!
 * \brief Verifies file reads fail when every stored data replica is corrupt.
 */
static void test_read_fails_when_all_data_copies_are_corrupt(void **state)
{
	(void)state;
	const size_t size = SIZE_BYTE_HEAD + 2 * SIZE_BYTE_DATA + 7;
	uint8_t *original = malloc(size);
	assert_non_null(original);
	memset(original, 0x39, size);
	write_test_file("/all-corrupt:secret", original, size);
	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("all-corrupt");
	file.pass = strdup("secret");
	assert_true(stegfs_file_stat(&file));
	stegfs_s filesystem = stegfs_info();
	for (unsigned copy = 0; copy < filesystem.copies; copy++)
		for (uint64_t block = 1; block <= file.blocks[copy][0]; block++)
		{
			uint64_t index = file.blocks[copy][block] % (filesystem.size / filesystem.blocksize);
			uint8_t *corrupt = (uint8_t *)filesystem.memory
				+ index * filesystem.blocksize + sizeof(uint64_t) * SIZE_LONG_PATH;
			corrupt[0] ^= 0x01;
		}
	errno = 0;
	/* With every replica damaged, recovery must fail rather than return partial data. */
	assert_false(stegfs_file_read(&file));
	assert_true(errno == ENOENT || errno == EIO);
	free(file.path);
	free(file.name);
	free(file.pass);
	free(file.data);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(file.blocks[i]);
	free(original);
}

/*!
 * \brief Verifies deletion releases storage and cache entries for reuse.
 */
static void test_file_delete_releases_file_and_cache(void **state)
{
	(void)state;
	const uint8_t content[] = "delete me";
	write_test_file("/remove:secret", content, sizeof content);
	assert_non_null(stegfs_cache_exists("/remove", NULL));

	stegfs_file_s file = { 0 };
	file.path = strdup("/");
	file.name = strdup("remove");
	file.pass = strdup("secret");
	stegfs_file_delete(&file);
	assert_null(stegfs_cache_exists("/remove", NULL));
	assert_false(stegfs_file_stat(&file));
	assert_int_equal(stegfs_info().blocks.used, 1);
	write_test_file("/remove:secret", content, sizeof content);
	assert_non_null(stegfs_cache_exists("/remove", NULL));
	free(file.path);
	free(file.name);
	free(file.pass);
	for (unsigned i = 0; i < COPIES_MAX; i++)
		free(file.blocks[i]);
}

/*!
 * \brief Verifies FUSE create, delete, and directory operations report expected errors.
 */
static void test_fuse_create_delete_and_directory_errors(void **state)
{
	(void)state;
	assert_int_equal(fuse_stegfs_open("/missing:secret", NULL), -ENOENT);
	assert_int_equal(fuse_stegfs_mkdir("/folder", 0700), 0);
	assert_int_equal(fuse_stegfs_mkdir("/folder", 0700), -EEXIST);
	assert_int_equal(fuse_stegfs_create("/folder/item:secret", 0600, NULL), 0);
	assert_int_equal(fuse_stegfs_create("/folder/item:secret", 0600, NULL), -EEXIST);
	assert_int_equal(fuse_stegfs_unlink("/folder"), -EISDIR);
	assert_int_equal(fuse_stegfs_rmdir("/folder"), -ENOTEMPTY);
	assert_int_equal(fuse_stegfs_unlink("/folder/item:secret"), 0);
	assert_int_equal(fuse_stegfs_unlink("/folder/item:secret"), -ENOENT);
	assert_int_equal(fuse_stegfs_create("/folder/item:secret", 0600, NULL), 0);
	assert_int_equal(fuse_stegfs_unlink("/folder/item:secret"), 0);
	assert_int_equal(fuse_stegfs_rmdir("/folder"), 0);
	assert_int_equal(fuse_stegfs_rmdir("/missing"), -ENOENT);
}

/*!
 * \brief Verifies nested cache paths can be removed and recreated cleanly.
 */
static void test_nested_cache_removal_and_recreation(void **state)
{
	(void)state;
	assert_int_equal(fuse_stegfs_mkdir("/one", 0700), 0);
	assert_int_equal(fuse_stegfs_mkdir("/one/two", 0700), 0);
	assert_int_equal(fuse_stegfs_create("/one/two/item:secret", 0600, NULL), 0);
	assert_int_equal(fuse_stegfs_rmdir("/one"), -ENOTEMPTY);
	assert_int_equal(fuse_stegfs_unlink("/one/two/item:secret"), 0);
	assert_int_equal(fuse_stegfs_rmdir("/one/two"), 0);
	assert_int_equal(fuse_stegfs_rmdir("/one"), 0);
	assert_int_equal(fuse_stegfs_mkdir("/one", 0700), 0);
	assert_int_equal(fuse_stegfs_mkdir("/one/two", 0700), 0);
	assert_int_equal(fuse_stegfs_create("/one/two/again:secret", 0600, NULL), 0);
	assert_int_equal(fuse_stegfs_unlink("/one/two/again:secret"), 0);
	assert_int_equal(fuse_stegfs_rmdir("/one/two"), 0);
	assert_int_equal(fuse_stegfs_rmdir("/one"), 0);
}

typedef struct
{
	char names[16][64];
	off_t offsets[16];
	size_t count;
	size_t limit;
}
directory_entries_s;

static int record_directory_entry(void *buffer, const char *name, const struct stat *st, off_t offset)
{
	(void)st;
	directory_entries_s *entries = buffer;
	if (entries->count == entries->limit)
		return 1;
	assert_true(entries->count < sizeof entries->names / sizeof entries->names[0]);
	snprintf(entries->names[entries->count], sizeof entries->names[0], "%s", name);
	entries->offsets[entries->count] = offset;
	entries->count++;
	return 0;
}

/*!
 * \brief Verifies directory reads resume from offsets and stop at buffer capacity.
 */
static void test_readdir_continuation_and_buffer_full(void **state)
{
	(void)state;
	assert_int_equal(fuse_stegfs_mkdir("/alpha", 0700), 0);
	assert_int_equal(fuse_stegfs_mkdir("/beta", 0700), 0);
	directory_entries_s first = { .limit = 3 };
	assert_int_equal(fuse_stegfs_readdir("/", &first, record_directory_entry, 0, NULL), 0);
	assert_int_equal(first.count, 3);
	assert_string_equal(first.names[0], ".");
	assert_string_equal(first.names[1], "..");
	assert_string_equal(first.names[2], "alpha");
	assert_int_equal(first.offsets[2], 3);

	/* Resume after the last emitted entry so the second page contains only the remainder. */
	directory_entries_s second = { .limit = 16 };
	assert_int_equal(fuse_stegfs_readdir("/", &second, record_directory_entry, first.offsets[2], NULL), 0);
	assert_int_equal(second.count, 1);
	assert_string_equal(second.names[0], "beta");
	assert_int_equal(second.offsets[0], 4);
	assert_int_equal(fuse_stegfs_readdir("/", &second, record_directory_entry, -1, NULL), -EINVAL);
}

/*!
 * \brief Verifies truncation, zero-filled extension, and invalid I/O offsets.
 */
static void test_truncate_shrink_extend_and_invalid_offsets(void **state)
{
	(void)state;
	struct fuse_file_info info = { .flags = O_WRONLY };
	assert_int_equal(fuse_stegfs_create("/resize:secret", 0600, &info), 0);
	assert_int_equal(fuse_stegfs_open("/resize:secret", &info), 0);
	assert_int_equal(fuse_stegfs_write("/resize:secret", "abcdefgh", 8, 0, &info), 8);
	assert_int_equal(fuse_stegfs_ftruncate("/resize:secret", -1, &info), -EINVAL);
	assert_int_equal(fuse_stegfs_release("/resize:secret", &info), 0);

	assert_int_equal(fuse_stegfs_open("/resize:secret", &info), 0);
	assert_int_equal(fuse_stegfs_ftruncate("/resize:secret", 3, &info), 0);
	assert_int_equal(fuse_stegfs_ftruncate("/resize:secret", 6, &info), 0);
	assert_int_equal(fuse_stegfs_release("/resize:secret", &info), 0);

	info.flags = O_RDONLY;
	assert_int_equal(fuse_stegfs_open("/resize:secret", &info), 0);
	char output[6] = { 0 };
	assert_int_equal(fuse_stegfs_read("/resize:secret", output, sizeof output, 0, &info), sizeof output);
	assert_memory_equal(output, "abc\0\0\0", sizeof output);
	assert_int_equal(fuse_stegfs_write("/resize:secret", "x", 1, 0, &info), -EBADF);
	assert_int_equal(fuse_stegfs_write("/resize:secret", "x", 1, -1, &info), -EINVAL);
	assert_int_equal(fuse_stegfs_release("/resize:secret", &info), 0);
}

/*!
 * \brief Verifies filesystem statistics reflect blocks allocated by stored files.
 */
static void test_statfs_tracks_used_blocks(void **state)
{
	(void)state;
	struct statvfs stats;
	assert_int_equal(fuse_stegfs_statfs("/", &stats), 0);
	assert_int_equal(stats.f_blocks, 1024 * 1024 / SIZE_BYTE_BLOCK - 1);
	assert_int_equal(stats.f_bfree, stats.f_blocks);

	const uint8_t content[] = "allocation";
	write_test_file("/allocated:secret", content, sizeof content);
	assert_int_equal(fuse_stegfs_statfs("/", &stats), 0);
	assert_int_equal(stats.f_bfree, stats.f_blocks - 2);
}

/*!
 * \brief Verifies block-device callbacks reject malformed and out-of-range indices.
 */
static void test_bloc_callbacks_reject_bad_indices(void **state)
{
	(void)state;
	char target[128];
	assert_int_equal(fuse_stegfs_readlink("/bloc/18446744073709551615", target, sizeof target), -ENOENT);
	assert_int_equal(fuse_stegfs_readlink("/bloc/not-a-number", target, sizeof target), -ENOENT);
	struct stat attributes;
	assert_int_equal(fuse_stegfs_getattr("/bloc/18446744073709551615", &attributes), -ENOENT);
	assert_int_equal(fuse_stegfs_getattr("/bloc/not-a-number", &attributes), -ENOENT);
	assert_int_equal(fuse_stegfs_rmdir("/bloc"), -EBUSY);
}

/*!
 * \brief Verifies allocated block entries expose their attributes and link targets.
 */
static void test_bloc_getattr_and_readlink_for_allocated_block(void **state)
{
	(void)state;
	const uint8_t content[] = "visible block";
	write_test_file("/visible:secret", content, sizeof content);
	stegfs_s filesystem = stegfs_info();
	uint64_t blocks = filesystem.size / filesystem.blocksize;
	bool found = false;
	for (uint64_t i = 1; i < blocks; i++)
		if (filesystem.blocks.file[i])
		{
			char path[64];
			char target[128];
			struct stat attributes;
			snprintf(path, sizeof path, "/bloc/%" PRIu64, i);
			assert_int_equal(fuse_stegfs_getattr(path, &attributes), 0);
			assert_true(S_ISLNK(attributes.st_mode));
			assert_int_equal(fuse_stegfs_readlink(path, target, sizeof target), 0);
			assert_string_equal(target, filesystem.blocks.file[i]);
			found = true;
			break;
		}
	assert_true(found);
}

/*!
 * \brief Verifies block path parsing handles valid, malformed, and overflowing indices.
 */
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
	cmocka_unit_test_setup_teardown(test_concurrent_fuse_requests_preserve_filesystem_state, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_overlapping_handles_keep_independent_access_and_lifetime, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_unlink_open_file_then_recreate_same_path, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_empty_fuse_create_is_persisted_on_release, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_storage_round_trips_at_block_boundaries, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_storage_persists_across_normal_remount, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_storage_persists_across_formatter_image, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_overwrite_existing_file_and_remount, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_empty_file_round_trip, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_sparse_write_truncate_and_read, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_reject_invalid_image_size, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_reject_malformed_superblock_tag_count, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_reject_truncated_superblock_tlv, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_superblock_metadata_validation_matrix, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_initialization_preserves_stdin, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_repeated_init_and_deinit, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_reject_capacity_without_deleting_file, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_small_image_allocation_failure_releases_partial_blocks, setup_small_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_existing_file_growth_failure_preserves_contents_and_blocks, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_file_capacity_exact_fit_and_one_block_over, setup_filesystem_one_copy, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_file_growth_and_shrink_across_multiple_blocks, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_wrong_password_is_rejected, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_read_falls_back_to_unmodified_copy, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_read_uses_backup_when_first_inode_is_corrupt, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_read_fails_when_all_data_copies_are_corrupt, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_file_delete_releases_file_and_cache, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_fuse_create_delete_and_directory_errors, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_nested_cache_removal_and_recreation, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_readdir_continuation_and_buffer_full, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_truncate_shrink_extend_and_invalid_offsets, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_statfs_tracks_used_blocks, setup_filesystem, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_bloc_callbacks_reject_bad_indices, setup_filesystem_with_bloc, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_bloc_getattr_and_readlink_for_allocated_block, setup_filesystem_with_bloc, teardown_filesystem),
	cmocka_unit_test_setup_teardown(test_block_path_parser_bounds, setup_filesystem, teardown_filesystem)
};

int main(void)
{
	return _cmocka_run_group_tests("stegfs", tests, sizeof tests / sizeof tests[0], NULL, NULL);
}
