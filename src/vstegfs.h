/*
 * vstegfs ~ a virtual steganographic file system for linux
 * Copyright (c) 2007-2009, albinoloverats ~ Software Development
 * email: vstegfs@albinoloverats.net
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

#ifndef _VSTEGFS_H_
  #define _VSTEGFS_H_

  #include <inttypes.h>

  #define VERSION  "2009??"

  #define SIZE_BLOCKS    8192  // blocks per MB
  #define SIZE_BLOCK      128  // block size in bytes

  #define SIZE_BYTE         8  // size in bits
  #define SIZE_WORD        16
  #define SIZE_DWORD       32
  #define SIZE_QWORD       64

  #define LENGTH_PATH      16  // bytes per block
  #define LENGTH_DATA      80  // bytes per block
  #define LENGTH_CHECKSUM  24  // bytes per block
  #define LENGTH_NEXT       8  // bytes per block

  #define OFFSET_PATH       0  // bytes offset from start of block
  #define OFFSET_DATA      16  // bytes offset from start of block
  #define OFFSET_CHECKSUM  96  // bytes offset from start of block
  #define OFFSET_NEXT     120  // bytes offset from start of block

  #define TIGER_BITS      192
  #define TIGER_BYTES      24
  #define TIGER_128_BITS  128
  #define TIGER_128_BYTES  16

  #define SERPENT_BYTES   128
  #define SERPENT_BYTES_B  16

  #define FILE_COPIES      10  // number of redundent copies of each file
  #define FILE_HEADERS      3  // number of copies of the header block

  //typedef struct v_block
  //{
  //    uint8_t path[0x10];
  //    uint8_t data[0x50];
  //    uint8_t hash[0x18];
  //    uint64_t next;
  //}
  //v_block;

  /*
   * main functions
   */
  uint32_t file_write(FILE *, size_t, char *, char *, char *);
  uint32_t file_read(FILE *, char *, char *, char *);
  uint32_t file_check(char *, char *, char *);
  uint64_t *file_find(char *, char *, char *);
  uint32_t file_unlink(char *, char *, char *);

  uint32_t block_write(char *, char *, uint64_t, char *, uint32_t *);
  uint64_t block_read(char *, char *, char *, uint32_t *);
  uint32_t block_check(char *, uint64_t);

  void block_encrypt(char *, char *, uint32_t *);
  void block_decrypt(char *, char *, uint32_t *);

  void *hash_data(char *, uint64_t);
  void *hash_path(char *, uint64_t);

  uint64_t check_endian(uint64_t);


  /*
   * common application functions
   */
  uint64_t show_help(void);
  uint64_t show_licence(void);
  uint64_t show_usage(void);
  uint64_t show_version(void);

#endif /* _VSTEGFS_H_ */
