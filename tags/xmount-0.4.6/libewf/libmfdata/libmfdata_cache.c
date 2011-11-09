/*
 * The cache functions
 *
 * Copyright (c) 2010-2011, Joachim Metz <jbmetz@users.sourceforge.net>
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

#include <common.h>
#include <memory.h>
#include <types.h>

#include <liberror.h>
#include <libnotify.h>

#include "libmfdata_cache_value.h"
#include "libmfdata_date_time.h"
#include "libmfdata_definitions.h"
#include "libmfdata_cache.h"
#include "libmfdata_cache_value.h"
#include "libmfdata_types.h"

/* Initializes the cache
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_initialize(
     libmfdata_cache_t **cache,
     int maximum_cache_entries,
     liberror_error_t **error )
{
	libmfdata_internal_cache_t *internal_cache = NULL;
	static char *function                      = "libmfdata_cache_initialize";

	if( cache == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache.",
		 function );

		return( -1 );
	}
	if( maximum_cache_entries <= 0 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_VALUE_ZERO_OR_LESS,
		 "%s: invalid maximum cache entries value zero or less.",
		 function );

		return( -1 );
	}
	if( *cache == NULL )
	{
		internal_cache = memory_allocate_structure(
		                  libmfdata_internal_cache_t );

		if( internal_cache == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_INSUFFICIENT,
			 "%s: unable to create internal cache.",
			 function );

			goto on_error;
		}
		if( memory_set(
		     internal_cache,
		     0,
		     sizeof( libmfdata_internal_cache_t ) ) == NULL )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_MEMORY,
			 LIBERROR_MEMORY_ERROR_SET_FAILED,
			 "%s: unable to clear cache.",
			 function );

			goto on_error;
		}
		if( libmfdata_array_initialize(
		     &( internal_cache->entries ),
		     maximum_cache_entries,
		     error ) != 1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
			 "%s: unable to create entries array.",
			 function );

			goto on_error;
		}
		*cache = (libmfdata_cache_t *) internal_cache;
	}
	return( 1 );

on_error:
	if( internal_cache != NULL )
	{
		memory_free(
		 internal_cache );
	}
	return( -1 );
}

/* Frees the cache
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_free(
     libmfdata_cache_t **cache,
     liberror_error_t **error )
{
	libmfdata_internal_cache_t *internal_cache = NULL;
	static char *function                      = "libmfdata_cache_free";
	int result                                 = 1;

	if( cache == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache.",
		 function );

		return( -1 );
	}
	if( *cache != NULL )
	{
		internal_cache = (libmfdata_internal_cache_t *) *cache;
		*cache         = NULL;

		if( libmfdata_array_free(
		     &( internal_cache->entries ),
		     &libmfdata_cache_value_free,
		     error ) != 1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
			 "%s: unable to free the entries array.",
			 function );

			result = -1;
		}
		memory_free(
		 internal_cache );
	}
	return( result );
}

/* Clones (duplicates) the cache, not the cache values
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_clone(
     libmfdata_cache_t **destination_cache,
     libmfdata_cache_t *source_cache,
     liberror_error_t **error )
{
	libmfdata_internal_cache_t *internal_source_cache = NULL;
	static char *function                             = "libmfdata_cache_clone";
	int number_of_cache_entries                       = 0;

	if( destination_cache == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid destination cache.",
		 function );

		return( -1 );
	}
	if( *destination_cache != NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_VALUE_ALREADY_SET,
		 "%s: destination cache already set.",
		 function );

		return( -1 );
	}
	if( source_cache == NULL )
	{
		*destination_cache = NULL;

		return( 1 );
	}
	internal_source_cache = (libmfdata_internal_cache_t *) source_cache;

	if( libmfdata_array_get_number_of_entries(
	     internal_source_cache->entries,
	     &number_of_cache_entries,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of cache entries from source entries array.",
		 function );

		return( -1 );
	}
	if( libmfdata_cache_initialize(
	     destination_cache,
	     number_of_cache_entries,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
		 "%s: unable to create destination cache.",
		 function );

		return( -1 );
	}
	return( 1 );
}

/* Retrieves the number of entries of the cache
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_get_number_of_entries(
     libmfdata_cache_t *cache,
     int *number_of_entries,
     liberror_error_t **error )
{
	libmfdata_internal_cache_t *internal_cache = NULL;
	static char *function                      = "libmfdata_cache_get_number_of_entries";

	if( cache == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache.",
		 function );

		return( -1 );
	}
	internal_cache = (libmfdata_internal_cache_t *) cache;

	if( libmfdata_array_get_number_of_entries(
	     internal_cache->entries,
	     number_of_entries,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve number of entries from entries array.",
		 function );

		return( -1 );
	}
	return( 1 );
}

/* Retrieves the number of cache values
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_get_number_of_cache_values(
     libmfdata_cache_t *cache,
     int *number_of_cache_values,
     liberror_error_t **error )
{
	libmfdata_internal_cache_t *internal_cache = NULL;
	static char *function                      = "libmfdata_cache_get_number_of_cache_values";

	if( cache == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache.",
		 function );

		return( -1 );
	}
	internal_cache = (libmfdata_internal_cache_t *) cache;

	if( number_of_cache_values == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid number of cache values.",
		 function );

		return( -1 );
	}
	*number_of_cache_values = internal_cache->number_of_cache_values;

	return( 1 );
}

/* Clears the cache of the cache
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_clear(
     libmfdata_cache_t *cache,
     liberror_error_t **error )
{
	libmfdata_internal_cache_t *internal_cache = NULL;
	static char *function                      = "libmfdata_cache_clear";

	if( cache == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache.",
		 function );

		return( -1 );
	}
	internal_cache = (libmfdata_internal_cache_t *) cache;

	if( libmfdata_array_clear(
	     internal_cache->entries,
	     &libmfdata_cache_value_free,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_FINALIZE_FAILED,
		 "%s: unable to clear entries array.",
		 function );

		return( -1 );
	}
	internal_cache->number_of_cache_values = 0;

	return( 1 );
}

/* Retrieves the cache value for the specific index
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_get_value_by_index(
     libmfdata_cache_t *cache,
     int cache_entry_index,
     libmfdata_cache_value_t **cache_value,
     liberror_error_t **error )
{
	libmfdata_internal_cache_t *internal_cache = NULL;
	static char *function                      = "libmfdata_cache_get_value_by_index";

	if( cache == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache.",
		 function );

		return( -1 );
	}
	internal_cache = (libmfdata_internal_cache_t *) cache;

	if( libmfdata_array_get_entry_by_index(
	     internal_cache->entries,
	     cache_entry_index,
	     (intptr_t **) cache_value,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve cache value: %d from entries array.",
		 function,
		 cache_entry_index );

		return( -1 );
	}
	return( 1 );
}

/* Sets the cache value for the specific index
 * Returns 1 if successful or -1 on error
 */
int libmfdata_cache_set_value_by_index(
     libmfdata_cache_t *cache,
     int cache_entry_index,
     int file_io_pool_entry,
     off64_t offset,
     time_t timestamp,
     intptr_t *value,
     int (*free_value)(
            intptr_t *value,
            liberror_error_t **error ),
     uint8_t flags,
     liberror_error_t **error )
{
	libmfdata_cache_value_t *cache_value       = NULL;
	libmfdata_internal_cache_t *internal_cache = NULL;
	static char *function                      = "libmfdata_cache_set_value_by_index";

	if( cache == NULL )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_ARGUMENTS,
		 LIBERROR_ARGUMENT_ERROR_INVALID_VALUE,
		 "%s: invalid cache.",
		 function );

		return( -1 );
	}
	internal_cache = (libmfdata_internal_cache_t *) cache;

	if( libmfdata_array_get_entry_by_index(
	     internal_cache->entries,
	     cache_entry_index,
	     (intptr_t **) &cache_value,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_GET_FAILED,
		 "%s: unable to retrieve cache value: %d from entries array.",
		 function,
		 cache_entry_index );

		return( -1 );
	}
	if( cache_value == NULL )
	{
		if( libmfdata_cache_value_initialize(
		     &cache_value,
		     error ) != 1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_INITIALIZE_FAILED,
			 "%s: unable to create cache value.",
			 function );

			return( -1 );
		}
		if( libmfdata_array_set_entry_by_index(
		     internal_cache->entries,
		     cache_entry_index,
		     (intptr_t *) cache_value,
		     error ) != 1 )
		{
			liberror_error_set(
			 error,
			 LIBERROR_ERROR_DOMAIN_RUNTIME,
			 LIBERROR_RUNTIME_ERROR_SET_FAILED,
			 "%s: unable to set cache value: %d in entries array.",
			 function,
			 cache_entry_index );

			libmfdata_cache_value_free(
			 (intptr_t *) cache_value,
			 NULL );

			return( -1 );
		}
		internal_cache->number_of_cache_values++;
	}
	if( libmfdata_cache_value_set_value(
	     cache_value,
	     value,
	     free_value,
	     flags,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set value in cache value.",
		 function );

		return( -1 );
	}
	if( libmfdata_cache_value_set_identifier(
	     cache_value,
	     file_io_pool_entry,
	     offset,
	     timestamp,
	     error ) != 1 )
	{
		liberror_error_set(
		 error,
		 LIBERROR_ERROR_DOMAIN_RUNTIME,
		 LIBERROR_RUNTIME_ERROR_SET_FAILED,
		 "%s: unable to set identifier in cache value.",
		 function );

		return( -1 );
	}
	return( 1 );
}

