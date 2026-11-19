/*
 * Heart of The Alien Redux: small platform helpers
 * See platform.h for why this exists.
 */

#include <stdlib.h>
#include <stdio.h>

#include "platform.h"

#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>
#include <io.h>

static wchar_t *utf8_to_wide(const char *text)
{
	int length;
	wchar_t *wide;

	if (text == NULL)
	{
		return NULL;
	}

	length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
		NULL, 0);
	if (length <= 0)
	{
		return NULL;
	}

	wide = (wchar_t *)malloc((size_t)length * sizeof(wchar_t));
	if (wide == NULL)
	{
		return NULL;
	}

	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
		wide, length) <= 0)
	{
		free(wide);
		return NULL;
	}

	return wide;
}

static void widen_mode(const char *mode, wchar_t *wide, size_t size)
{
	size_t i;

	for (i = 0; mode[i] != '\0' && i + 1 < size; i++)
	{
		wide[i] = (wchar_t)(unsigned char)mode[i];
	}

	wide[i] = L'\0';
}

FILE *hota_fopen(const char *path, const char *mode)
{
	wchar_t wmode[16];
	wchar_t *wpath;
	FILE *fp;

	wpath = utf8_to_wide(path);
	if (wpath == NULL)
	{
		/* not valid UTF-8: let the C runtime have a try */
		return fopen(path, mode);
	}

	widen_mode(mode, wmode, sizeof(wmode) / sizeof(wmode[0]));
	fp = _wfopen(wpath, wmode);
	free(wpath);
	return fp;
}

int hota_file_readable(const char *path)
{
	wchar_t *wpath;
	int readable;

	wpath = utf8_to_wide(path);
	if (wpath == NULL)
	{
		return 0;
	}

	/* 4 == R_OK */
	readable = (_waccess(wpath, 4) == 0);
	free(wpath);
	return readable;
}

char **hota_utf8_argv(int *argc)
{
	wchar_t **wide_argv;
	char **argv;
	int count;
	int i;

	wide_argv = CommandLineToArgvW(GetCommandLineW(), &count);
	if (wide_argv == NULL || count <= 0)
	{
		if (wide_argv != NULL)
		{
			LocalFree(wide_argv);
		}
		return NULL;
	}

	argv = (char **)calloc((size_t)count + 1, sizeof(char *));
	if (argv == NULL)
	{
		LocalFree(wide_argv);
		return NULL;
	}

	for (i = 0; i < count; i++)
	{
		int length;

		length = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1,
			NULL, 0, NULL, NULL);
		if (length <= 0)
		{
			continue;
		}

		argv[i] = (char *)malloc((size_t)length);
		if (argv[i] == NULL)
		{
			continue;
		}

		if (WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1,
			argv[i], length, NULL, NULL) <= 0)
		{
			free(argv[i]);
			argv[i] = NULL;
		}
	}

	LocalFree(wide_argv);
	*argc = count;
	return argv;
}

#endif /* _WIN32 */
