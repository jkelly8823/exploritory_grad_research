		if (strncmp(resolved_basedir, resolved_name, resolved_basedir_len) == 0) {
#endif
			if (resolved_name_len > resolved_basedir_len &&
				resolved_name[resolved_basedir_len] != PHP_DIR_SEPARATOR) {
				return -1;
			} else {
				/* File is in the right directory */
				return 0;
			}
		} else {
			/* /openbasedir/ and /openbasedir are the same directory */
			if (resolved_basedir_len == (resolved_name_len + 1) && resolved_basedir[resolved_basedir_len - 1] == PHP_DIR_SEPARATOR) {
#if defined(PHP_WIN32) || defined(NETWARE)
				if (strncasecmp(resolved_basedir, resolved_name, resolved_name_len) == 0) {
#else
				if (strncmp(resolved_basedir, resolved_name, resolved_name_len) == 0) {
#endif
					return 0;
				}
			}
			return -1;
		}
	} else {
		/* Unable to resolve the real path, return -1 */
		return -1;
	}
}
/* }}} */

PHPAPI int php_check_open_basedir(const char *path TSRMLS_DC)
{
	return php_check_open_basedir_ex(path, 1 TSRMLS_CC);
}

/* {{{ php_check_open_basedir
 */
PHPAPI int php_check_open_basedir_ex(const char *path, int warn TSRMLS_DC)
{
	/* Only check when open_basedir is available */
	if (PG(open_basedir) && *PG(open_basedir)) {
		char *pathbuf;
		char *ptr;
		char *end;

		pathbuf = estrdup(PG(open_basedir));

		ptr = pathbuf;

		while (ptr && *ptr) {
			end = strchr(ptr, DEFAULT_DIR_SEPARATOR);
			if (end != NULL) {
				*end = '\0';
				end++;
			}

			if (php_check_specific_open_basedir(ptr, path TSRMLS_CC) == 0) {
				efree(pathbuf);
				return 0;
			}

			ptr = end;
		}
		if (warn) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s)", path, PG(open_basedir));
		}
		efree(pathbuf);
		errno = EPERM; /* we deny permission to open it */
		return -1;
	}

	/* Nothing to check... */
	return 0;
}
/* }}} */

/* {{{ php_check_safe_mode_include_dir
 */
PHPAPI int php_check_safe_mode_include_dir(const char *path TSRMLS_DC)
{
	if (PG(safe_mode)) {
		if (PG(safe_mode_include_dir) && *PG(safe_mode_include_dir)) {
			char *pathbuf;
			char *ptr;
			char *end;
			char resolved_name[MAXPATHLEN];

			/* Resolve the real path into resolved_name */
			if (expand_filepath(path, resolved_name TSRMLS_CC) == NULL) {
				return -1;
			}
			pathbuf = estrdup(PG(safe_mode_include_dir));
			ptr = pathbuf;

			while (ptr && *ptr) {
				end = strchr(ptr, DEFAULT_DIR_SEPARATOR);
				if (end != NULL) {
					*end = '\0';
					end++;
				}

				/* Check the path */
#ifdef PHP_WIN32
				if (strncasecmp(ptr, resolved_name, strlen(ptr)) == 0)
#else
				if (strncmp(ptr, resolved_name, strlen(ptr)) == 0)
#endif
				{
					/* File is in the right directory */
					efree(pathbuf);
					return 0;
				}

				ptr = end;
			}
			efree(pathbuf);
		}
		return -1;
	}

	/* Nothing to check... */
	return 0;
}
/* }}} */

/* {{{ php_fopen_and_set_opened_path
 */
static FILE *php_fopen_and_set_opened_path(const char *path, const char *mode, char **opened_path TSRMLS_DC)
{
	FILE *fp;

	if (php_check_open_basedir((char *)path TSRMLS_CC)) {
		return NULL;
	}
	fp = VCWD_FOPEN(path, mode);
	if (fp && opened_path) {
		*opened_path = expand_filepath(path, NULL TSRMLS_CC);
	}
	return fp;
}
/* }}} */

/* {{{ php_fopen_primary_script
 */
PHPAPI int php_fopen_primary_script(zend_file_handle *file_handle TSRMLS_DC)
{
	FILE *fp;
#ifndef PHP_WIN32
	struct stat st;
#endif
	char *path_info;
	char *filename = NULL;
	char *resolved_path = NULL;
	int length;

	path_info = SG(request_info).request_uri;
#if HAVE_PWD_H
	if (PG(user_dir) && *PG(user_dir) && path_info && '/' == path_info[0] && '~' == path_info[1]) {
		char *s = strchr(path_info + 2, '/');

		if (s) {			/* if there is no path name after the file, do not bother */
			char user[32];			/* to try open the directory */
			struct passwd *pw;
#if defined(ZTS) && defined(HAVE_GETPWNAM_R) && defined(_SC_GETPW_R_SIZE_MAX)
			struct passwd pwstruc;
			long pwbuflen = sysconf(_SC_GETPW_R_SIZE_MAX);
			char *pwbuf;

			if (pwbuflen < 1) {
				return FAILURE;
			}

			pwbuf = emalloc(pwbuflen);
#endif
			length = s - (path_info + 2);
			if (length > (int)sizeof(user) - 1) {
				length = sizeof(user) - 1;
			}
			memcpy(user, path_info + 2, length);
			user[length] = '\0';
#if defined(ZTS) && defined(HAVE_GETPWNAM_R) && defined(_SC_GETPW_R_SIZE_MAX)
			if (getpwnam_r(user, &pwstruc, pwbuf, pwbuflen, &pw)) {
				efree(pwbuf);
				return FAILURE;
			}
#else
			pw = getpwnam(user);
#endif
			if (pw && pw->pw_dir) {
				spprintf(&filename, 0, "%s%c%s%c%s", pw->pw_dir, PHP_DIR_SEPARATOR, PG(user_dir), PHP_DIR_SEPARATOR, s + 1); /* Safe */
			} else {
				filename = SG(request_info).path_translated;
			} 
#if defined(ZTS) && defined(HAVE_GETPWNAM_R) && defined(_SC_GETPW_R_SIZE_MAX)
			efree(pwbuf);
#endif
		}
	} else
#endif
	if (PG(doc_root) && path_info && (length = strlen(PG(doc_root))) &&
		IS_ABSOLUTE_PATH(PG(doc_root), length)) {
		filename = emalloc(length + strlen(path_info) + 2);
		if (filename) {
			memcpy(filename, PG(doc_root), length);
			if (!IS_SLASH(filename[length - 1])) {	/* length is never 0 */
				filename[length++] = PHP_DIR_SEPARATOR;
			}
			if (IS_SLASH(path_info[0])) {
				length--;
			}
			strcpy(filename + length, path_info);
		}
	} else {
		filename = SG(request_info).path_translated;
	}


	if (filename) {
		resolved_path = zend_resolve_path(filename, strlen(filename) TSRMLS_CC);
	}

	if (!resolved_path) {
		if (SG(request_info).path_translated != filename) {
			STR_FREE(filename);
		}
		/* we have to free SG(request_info).path_translated here because
		 * php_destroy_request_info assumes that it will get
		 * freed when the include_names hash is emptied, but
		 * we're not adding it in this case */
		STR_FREE(SG(request_info).path_translated);
		SG(request_info).path_translated = NULL;
		return FAILURE;
	}
	fp = VCWD_FOPEN(resolved_path, "rb");

#ifndef PHP_WIN32
	/* refuse to open anything that is not a regular file */
	if (fp && (0 > fstat(fileno(fp), &st) || !S_ISREG(st.st_mode))) {
		fclose(fp);
		fp = NULL;
	}
#endif

	if (!fp) {
		if (SG(request_info).path_translated != filename) {
			STR_FREE(filename);
		}
		STR_FREE(SG(request_info).path_translated);	/* for same reason as above */
		SG(request_info).path_translated = NULL;
		return FAILURE;
	}

	file_handle->opened_path = resolved_path;

	if (SG(request_info).path_translated != filename) {
		STR_FREE(SG(request_info).path_translated);	/* for same reason as above */
		SG(request_info).path_translated = filename;
	}

	file_handle->filename = SG(request_info).path_translated;
	file_handle->free_filename = 0;
	file_handle->handle.fp = fp;
	file_handle->type = ZEND_HANDLE_FP;

	return SUCCESS;
}
/* }}} */

/* {{{ php_resolve_path
 * Returns the realpath for given filename according to include path
 */
PHPAPI char *php_resolve_path(const char *filename, int filename_length, const char *path TSRMLS_DC)
{
	char resolved_path[MAXPATHLEN];
	char trypath[MAXPATHLEN];
	const char *ptr, *end, *p;
	char *actual_path;
	php_stream_wrapper *wrapper;

	if (!filename) {
		return NULL;
	}

	/* Don't resolve paths which contain protocol (except of file://) */
	for (p = filename; isalnum((int)*p) || *p == '+' || *p == '-' || *p == '.'; p++);
	if ((*p == ':') && (p - filename > 1) && (p[1] == '/') && (p[2] == '/')) {
		wrapper = php_stream_locate_url_wrapper(filename, &actual_path, STREAM_OPEN_FOR_INCLUDE TSRMLS_CC);
		if (wrapper == &php_plain_files_wrapper) {
			if (tsrm_realpath(actual_path, resolved_path TSRMLS_CC)) {
				return estrdup(resolved_path);
			}
		}
		return NULL;
	}

	if ((*filename == '.' && 
	     (IS_SLASH(filename[1]) || 
	      ((filename[1] == '.') && IS_SLASH(filename[2])))) ||
	    IS_ABSOLUTE_PATH(filename, filename_length) ||
	    !path ||
	    !*path) {
		if (tsrm_realpath(filename, resolved_path TSRMLS_CC)) {
			return estrdup(resolved_path);
		} else {
			return NULL;
		}
	}

	ptr = path;
	while (ptr && *ptr) {
		/* Check for stream wrapper */
		int is_stream_wrapper = 0;

		for (p = ptr; isalnum((int)*p) || *p == '+' || *p == '-' || *p == '.'; p++);
		if ((*p == ':') && (p - ptr > 1) && (p[1] == '/') && (p[2] == '/')) {
			/* .:// or ..:// is not a stream wrapper */
			if (p[-1] != '.' || p[-2] != '.' || p - 2 != ptr) {
				p += 3;
				is_stream_wrapper = 1;
			}
		}
		end = strchr(p, DEFAULT_DIR_SEPARATOR);
		if (end) {
			if ((end-ptr) + 1 + filename_length + 1 >= MAXPATHLEN) {
				ptr = end + 1;
				continue;
			}
			memcpy(trypath, ptr, end-ptr);
			trypath[end-ptr] = '/';
			memcpy(trypath+(end-ptr)+1, filename, filename_length+1);
			ptr = end+1;
		} else {
			int len = strlen(ptr);

			if (len + 1 + filename_length + 1 >= MAXPATHLEN) {
				break;
			}
			memcpy(trypath, ptr, len);
			trypath[len] = '/';
			memcpy(trypath+len+1, filename, filename_length+1);
			ptr = NULL;
		}
		actual_path = trypath;
		if (is_stream_wrapper) {
			wrapper = php_stream_locate_url_wrapper(trypath, &actual_path, STREAM_OPEN_FOR_INCLUDE TSRMLS_CC);
			if (!wrapper) {
				continue;
			} else if (wrapper != &php_plain_files_wrapper) {
				if (wrapper->wops->url_stat) {
					php_stream_statbuf ssb;

					if (SUCCESS == wrapper->wops->url_stat(wrapper, trypath, 0, &ssb, NULL TSRMLS_CC)) {
						return estrdup(trypath);
					}
				}
				continue;
			}
		}
		if (tsrm_realpath(actual_path, resolved_path TSRMLS_CC)) {
			return estrdup(resolved_path);
		}
	} /* end provided path */

	/* check in calling scripts' current working directory as a fall back case
	 */
	if (zend_is_executing(TSRMLS_C)) {
		char *exec_fname = zend_get_executed_filename(TSRMLS_C);
		int exec_fname_length = strlen(exec_fname);

		while ((--exec_fname_length >= 0) && !IS_SLASH(exec_fname[exec_fname_length]));
		if (exec_fname && exec_fname[0] != '[' &&
		    exec_fname_length > 0 &&
		    exec_fname_length + 1 + filename_length + 1 < MAXPATHLEN) {
			memcpy(trypath, exec_fname, exec_fname_length + 1);
			memcpy(trypath+exec_fname_length + 1, filename, filename_length+1);
			actual_path = trypath;

			/* Check for stream wrapper */
			for (p = trypath; isalnum((int)*p) || *p == '+' || *p == '-' || *p == '.'; p++);
			if ((*p == ':') && (p - trypath > 1) && (p[1] == '/') && (p[2] == '/')) {
				wrapper = php_stream_locate_url_wrapper(trypath, &actual_path, STREAM_OPEN_FOR_INCLUDE TSRMLS_CC);
				if (!wrapper) {
					return NULL;
				} else if (wrapper != &php_plain_files_wrapper) {
					if (wrapper->wops->url_stat) {
						php_stream_statbuf ssb;

						if (SUCCESS == wrapper->wops->url_stat(wrapper, trypath, 0, &ssb, NULL TSRMLS_CC)) {
							return estrdup(trypath);
						}
					}
					return NULL;
				}
			}

			if (tsrm_realpath(actual_path, resolved_path TSRMLS_CC)) {
				return estrdup(resolved_path);
			}
		}
	}

	return NULL;
}
/* }}} */

/* {{{ php_fopen_with_path
 * Tries to open a file with a PATH-style list of directories.
 * If the filename starts with "." or "/", the path is ignored.
 */
PHPAPI FILE *php_fopen_with_path(const char *filename, const char *mode, const char *path, char **opened_path TSRMLS_DC)
{
	char *pathbuf, *ptr, *end;
	char *exec_fname;
	char trypath[MAXPATHLEN];
	struct stat sb;
	FILE *fp;
	int path_length;
	int filename_length;
	int exec_fname_length;

	if (opened_path) {
		*opened_path = NULL;
	}

	if (!filename) {
		return NULL;
	}

	filename_length = strlen(filename);

	/* Relative path open */
	if (*filename == '.') {
		if (PG(safe_mode) && (!php_checkuid(filename, mode, CHECKUID_CHECK_MODE_PARAM))) {
			return NULL;
		}
		return php_fopen_and_set_opened_path(filename, mode, opened_path TSRMLS_CC);
	}

	/*
	 * files in safe_mode_include_dir (or subdir) are excluded from
	 * safe mode GID/UID checks
	 */

	/* Absolute path open */
	if (IS_ABSOLUTE_PATH(filename, filename_length)) {
		if (php_check_safe_mode_include_dir(filename TSRMLS_CC) == 0) {
			/* filename is in safe_mode_include_dir (or subdir) */
			return php_fopen_and_set_opened_path(filename, mode, opened_path TSRMLS_CC);
		}
		if (PG(safe_mode) && (!php_checkuid(filename, mode, CHECKUID_CHECK_MODE_PARAM))) {
			return NULL;
		}
		return php_fopen_and_set_opened_path(filename, mode, opened_path TSRMLS_CC);
	}

	if (!path || (path && !*path)) {
		if (PG(safe_mode) && (!php_checkuid(filename, mode, CHECKUID_CHECK_MODE_PARAM))) {
			return NULL;
		}
		return php_fopen_and_set_opened_path(filename, mode, opened_path TSRMLS_CC);
	}

	/* check in provided path */
	/* append the calling scripts' current working directory
	 * as a fall back case
	 */
	if (zend_is_executing(TSRMLS_C)) {
		exec_fname = zend_get_executed_filename(TSRMLS_C);
		exec_fname_length = strlen(exec_fname);
		path_length = strlen(path);

		while ((--exec_fname_length >= 0) && !IS_SLASH(exec_fname[exec_fname_length]));
		if ((exec_fname && exec_fname[0] == '[') || exec_fname_length <= 0) {
			/* [no active file] or no path */
			pathbuf = estrdup(path);
		} else {
			pathbuf = (char *) emalloc(exec_fname_length + path_length + 1 + 1);
			memcpy(pathbuf, path, path_length);
			pathbuf[path_length] = DEFAULT_DIR_SEPARATOR;
			memcpy(pathbuf + path_length + 1, exec_fname, exec_fname_length);
			pathbuf[path_length + exec_fname_length + 1] = '\0';
		}
	} else {
		pathbuf = estrdup(path);
	}

	ptr = pathbuf;

	while (ptr && *ptr) {
		end = strchr(ptr, DEFAULT_DIR_SEPARATOR);
		if (end != NULL) {
			*end = '\0';
			end++;
		}
		if (snprintf(trypath, MAXPATHLEN, "%s/%s", ptr, filename) >= MAXPATHLEN) {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "%s/%s path was truncated to %d", ptr, filename, MAXPATHLEN);
		}
		if (PG(safe_mode)) {
			if (VCWD_STAT(trypath, &sb) == 0) {
				/* file exists ... check permission */
				if (php_check_safe_mode_include_dir(trypath TSRMLS_CC) == 0 ||
					php_checkuid(trypath, mode, CHECKUID_CHECK_MODE_PARAM)
				) {
					/* UID ok, or trypath is in safe_mode_include_dir */
					fp = php_fopen_and_set_opened_path(trypath, mode, opened_path TSRMLS_CC);
				} else {
					fp = NULL;
				}
				efree(pathbuf);
				return fp;
			}
		}
		fp = php_fopen_and_set_opened_path(trypath, mode, opened_path TSRMLS_CC);
		if (fp) {
			efree(pathbuf);
			return fp;
		}
		ptr = end;
	} /* end provided path */

	efree(pathbuf);
	return NULL;
}
/* }}} */

/* {{{ php_strip_url_passwd
 */
PHPAPI char *php_strip_url_passwd(char *url)
{
	register char *p, *url_start;

	if (url == NULL) {
		return "";
	}

	p = url;

	while (*p) {
		if (*p == ':' && *(p + 1) == '/' && *(p + 2) == '/') {
			/* found protocol */
			url_start = p = p + 3;

			while (*p) {
				if (*p == '@') {
					int i;

					for (i = 0; i < 3 && url_start < p; i++, url_start++) {
						*url_start = '.';
					}
					for (; *p; p++) {
						*url_start++ = *p;
					}
					*url_start=0;
					break;
				}
				p++;
			}
			return url;
		}
		p++;
	}
	return url;
}
/* }}} */

/* {{{ expand_filepath
 */
PHPAPI char *expand_filepath(const char *filepath, char *real_path TSRMLS_DC)
{
	return expand_filepath_ex(filepath, real_path, NULL, 0 TSRMLS_CC);
}
/* }}} */

/* {{{ expand_filepath_ex
 */
PHPAPI char *expand_filepath_ex(const char *filepath, char *real_path, const char *relative_to, size_t relative_to_len TSRMLS_DC)
{
	cwd_state new_state;
	char cwd[MAXPATHLEN];
	int copy_len;

	if (!filepath[0]) {
		return NULL;
	} else if (IS_ABSOLUTE_PATH(filepath, strlen(filepath))) {
		cwd[0] = '\0';
	} else {
		const char *iam = SG(request_info).path_translated;
		const char *result;
		if (relative_to) {
			if (relative_to_len > MAXPATHLEN-1U) {
				return NULL;
			}
			result = relative_to;
			memcpy(cwd, relative_to, relative_to_len+1U);
		} else {
			result = VCWD_GETCWD(cwd, MAXPATHLEN);
		}

		if (!result && (iam != filepath)) {
			int fdtest = -1;

			fdtest = VCWD_OPEN(filepath, O_RDONLY);
			if (fdtest != -1) {
				/* return a relative file path if for any reason
				 * we cannot cannot getcwd() and the requested,
				 * relatively referenced file is accessible */
				copy_len = strlen(filepath) > MAXPATHLEN - 1 ? MAXPATHLEN - 1 : strlen(filepath);
				real_path = estrndup(filepath, copy_len);
				close(fdtest);
				return real_path;
			} else {
				cwd[0] = '\0';
			}
		} else if (!result) {
			cwd[0] = '\0';
		}
	}

	new_state.cwd = strdup(cwd);
	new_state.cwd_length = strlen(cwd);

	if (virtual_file_ex(&new_state, filepath, NULL, CWD_FILEPATH)) {
		free(new_state.cwd);
		return NULL;
	}

	if (real_path) {
		copy_len = new_state.cwd_length > MAXPATHLEN - 1 ? MAXPATHLEN - 1 : new_state.cwd_length;
		memcpy(real_path, new_state.cwd, copy_len);
		real_path[copy_len] = '\0';
	} else {
		real_path = estrndup(new_state.cwd, new_state.cwd_length);
	}
	free(new_state.cwd);

	return real_path;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */