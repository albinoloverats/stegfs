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


  /* size (in bytes) for various blocks of data */
  #define SB_SERPENT 0x10               /*  16 bytes -- 128 bits */
  #define SB_TIGER   0x18               /*  24 bytes -- 192 bits */

  #define SB_BLOCK   0x80               /* 128 bytes -- full block */
  #define SB_PATH    SB_TIGER*2/3       /*  16 bytes -- 2/3 size of full tiger hash */
  #define SB_DATA    0x50               /*  80 bytes */
  #define SB_HASH    SB_TIGER           /*  24 bytes */
  #define SB_NEXT    0x08               /*   8 bytes */

  #define BLOCK_PER_MB 0x2000

  #define MAX_COPIES 9

  #define ROOT_PATH "vstegfs"

  typedef struct vstat_t
  {
      uint64_t  fs;
      FILE     *file;
      uint64_t *size;
      char     *name;
      char     *path;
      char     *pass;
  }
  vstat_t;

  typedef struct vblock_t
  {
      uint64_t path[0x02];              /*  16 bytes */
      uint8_t  data[0x50];              /*  80 bytes */
      uint64_t hash[0x03];              /*  24 bytes */
      uint64_t next[0x01];              /*   8 bytes */
  }
  vblock_t;


  extern  int64_t vstegfs_save(vstat_t);
  extern  int64_t vstegfs_open(vstat_t);
  extern uint64_t vstegfs_find(vstat_t);
  extern   void   vstegfs_kill(vstat_t);

  #ifdef _VSTEG_S_
    static MCRYPT  vstegfs_crypt_init(vstat_t *, uint8_t);
    static uint64_t vstegfs_header(vstat_t *, vblock_t *);
    static int64_t block_open(uint64_t, uint64_t, MCRYPT, vblock_t *);
    static int64_t vstegfs_block_save(uint64_t, uint64_t, MCRYPT, vblock_t *);
    static bool is_block_ours(uint64_t, uint64_t, uint64_t *);
    static uint64_t calc_next_block(uint64_t, char *);
  #endif /* _VSTEG_S_ */
  
#endif /* _VSTEGFS_H_ */
