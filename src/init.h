/*
 * stegfs ~ a steganographic file system for unix-like systems
 * Copyright © 2007-2015, albinoloverats ~ Software Development
 * email: stegfs@albinoloverats.net
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

#ifndef _STEGFS_INIT_H_
#define _STEGFS_INIT_H_

#include <gcrypt.h>

#define GIT_COMMIT_LENGTH 7

#define MKFS_NAME "mkstegfs"

#define STEGFS_USAGE "<file system> <mount point> [FUSE options ...]"
#define MKFS_USAGE "<file system> [-s size] [-f] [-r]"

/*!
 * \brief  Structure of expected options
 *
 * Structure returned from init() with values for any expected
 * options.
 */
typedef struct
{
	char *fs;
	char *mount;

	enum gcry_cipher_algos cipher; /*!< The cryptoraphic cipher selected by the user */
	enum gcry_cipher_modes mode;   /*!< The hash function selected by the user */
	enum gcry_md_algos     hash;   /*!< The encryption mode selected by the user */
	uint32_t duplicates;           /*!< Number of duplicates of each file */

	uint64_t size;                 /*!< File system size (mkfs) */

	bool paranoid:1;               /*!< Paranoid mode */

	bool force:1;                  /*!< Force file system creation (mkfs) */
	bool rewrite_sb:1;             /*!< Rewrite superblock (mkfs) */
	bool dry_run:1;                /*!< Create file system dry-run */

	bool help:1;                   /*!< -h argument was passed (FUSE) */
}
args_t;

/*!
 * \brief           Application init function
 * \param[in]  argc Number of command line arguments
 * \param[in]  argv Command line arguments
 * \param[out] fuse Command line arguments that can be passed to FUSE
 *
 * \return          Any command line options that were set
 *
 * Provide simple command line argument parsing, and pass back whatever
 * options where set. Removes a lot of the cruft from the legacy common
 * code that used to exist here.
 */
extern args_t init(int, char **, char **);

/*!
 * \brief         Show list of command line options
 *
 * Show list of command line options, and ways to invoke the application.
 * Usually when --help is given as a command line argument.
 */
extern void show_help(void) __attribute__((noreturn));

/*!
  * \brief        Show brief GPL licence text
  *
  * Display a brief overview of the GNU GPL v3 licence, such as when the
  * command line argument is --licence.
  */
extern void show_licence(void) __attribute__((noreturn));

/*!
 * \brief         Show simple usage instructions
 *
 * Display simple application usage instruction.
 */
extern void show_usage(void) __attribute__((noreturn));

/*!
 * \brief         Show application version
 *
 * Display the version of the application.
 */
extern void show_version(void) __attribute__((noreturn));

extern bool is_stegfs(void);

#endif /* ! _STEGFS_INIT_H_ */
