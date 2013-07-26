/*
 * Error functions
 *
 * Copyright (c) 2010-2012, Joachim Metz <jbmetz@users.sourceforge.net>
 *
 * Refer to AUTHORS for acknowledgements.
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined( _LIBMFCACHE_INTERNAL_ERROR_H )
#define _LIBMFCACHE_INTERNAL_ERROR_H

#include <common.h>
#include <types.h>

#include <stdio.h>

#if !defined( HAVE_LOCAL_LIBMFCACHE )
#include <libmfcache/error.h>
#endif

#include "libmfcache_extern.h"

#if defined( __cplusplus )
extern "C" {
#endif

#if !defined( HAVE_LOCAL_LIBMFCACHE )

LIBMFCACHE_EXTERN \
void libmfcache_error_free(
      libmfcache_error_t **error );

LIBMFCACHE_EXTERN \
int libmfcache_error_fprint(
     libmfcache_error_t *error,
     FILE *stream );

LIBMFCACHE_EXTERN \
int libmfcache_error_sprint(
     libmfcache_error_t *error,
     char *string,
     size_t size );

LIBMFCACHE_EXTERN \
int libmfcache_error_backtrace_fprint(
     libmfcache_error_t *error,
     FILE *stream );

LIBMFCACHE_EXTERN \
int libmfcache_error_backtrace_sprint(
     libmfcache_error_t *error,
     char *string,
     size_t size );

#endif /* !defined( HAVE_LOCAL_LIBMFCACHE ) */

#if defined( __cplusplus )
}
#endif

#endif

