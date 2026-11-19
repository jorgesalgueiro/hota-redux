/*
 * Heart of The Alien Redux: small platform helpers
 *
 * SDL gives the program UTF-8 paths, but the C runtime on Windows opens
 * files through the ANSI code page. These wrappers convert the path to
 * UTF-16 and use the wide-character C runtime instead, so game data can
 * live in a folder with accents or any other non-ASCII characters.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdio.h>

#ifdef _WIN32

FILE *hota_fopen(const char *path, const char *mode);
int hota_file_readable(const char *path);

/** Re-parses the Windows command line as UTF-8.
    The C runtime converts argv to the ANSI code page, which loses or
    corrupts non-ASCII paths. The caller keeps using the returned array
    for the lifetime of the process.
    @param argc  receives the number of arguments
    @returns a NULL-terminated UTF-8 argv, or NULL on failure
*/
char **hota_utf8_argv(int *argc);

#else

#include <unistd.h>

#define hota_fopen fopen
#define hota_file_readable(path) (access((path), R_OK) == 0)

#endif

#endif /* PLATFORM_H */
