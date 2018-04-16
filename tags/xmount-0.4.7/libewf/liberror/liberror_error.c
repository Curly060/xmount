/*
 * Error functions
 *
 * Copyright (c) 2008-2011, Joachim Metz <jbmetz@users.sourceforge.net>
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

#include <libcstring.h>

#if defined( HAVE_STDARG_H ) || defined( WINAPI )
#include <stdarg.h>
#elif defined( HAVE_VARARGS_H )
#include <varargs.h>
#else
#error Missing headers stdarg.h and varargs.h
#endif

#include "liberror_definitions.h"
#include "liberror_error.h"
#include "liberror_types.h"

/* Free an error and its elements
 */
void liberror_error_free(
      liberror_error_t **error )
{
	liberror_internal_error_t *internal_error = NULL;
	int message_index                         = 0;

	if( error == NULL )
	{
		return;
	}
	if( *error != NULL )
	{
		internal_error = (liberror_internal_error_t *) *error;

		if( internal_error->messages != NULL )
		{
			for( message_index = 0;
			     message_index < internal_error->number_of_messages;
			     message_index++ )
			{
				if( internal_error->messages[ message_index ] != NULL )
				{
					memory_free(
					 internal_error->messages[ message_index ] );
				}
			}
			memory_free(
			 internal_error->messages );
		}
		if( internal_error->lengths != NULL )
		{
			memory_free(
			 internal_error->lengths );
		}
		memory_free(
		 *error );

		*error = NULL;
	}
}

#if defined( HAVE_STDARG_H ) || defined( WINAPI )
#define VARARGS( function, error, error_domain, error_code, type, argument ) \
        function( error, error_domain, error_code, type argument, ... )
#define VASTART( argument_list, type, name ) \
        va_start( argument_list, name )
#define VAEND( argument_list ) \
        va_end( argument_list )

#elif defined( HAVE_VARARGS_H )
#define VARARGS( function, error, error_domain, error_code, type, argument ) \
        function( error, error_domain, error_code, va_alist ) va_dcl
#define VASTART( argument_list, type, name ) \
        { type name; va_start( argument_list ); name = va_arg( argument_list, type )
#define VAEND( argument_list ) \
        va_end( argument_list ); }

#endif

/* Sets an error
 * Initializes the error if necessary
 * The error domain and code are set only the first time and the error message is appended for backtracing
 */
void VARARGS(
      liberror_error_set,
      liberror_error_t **error,
      int error_domain,
      int error_code,
      const char *,
      format )
{
	va_list argument_list;

	liberror_internal_error_t *internal_error           = NULL;
	libcstring_system_character_t *system_string_format = NULL;
	void *reallocation                                  = NULL;
	size_t format_length                                = 0;
	size_t message_size                                 = LIBERROR_MESSAGE_INCREMENT_SIZE;
	int message_index                                   = 0;
	int print_count                                     = 0;

#if defined( WINAPI )
	size_t string_index                                 = 0;
#endif

	if( error == NULL )
	{
		return;
	}
	if( format == NULL )
	{
		return;
	}
	format_length = libcstring_narrow_string_length(
	                 format );

	if( format_length > message_size )
	{
		message_size = ( ( format_length / LIBERROR_MESSAGE_INCREMENT_SIZE ) + 1 )
		             * LIBERROR_MESSAGE_INCREMENT_SIZE;
	}
#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
	do
	{
		reallocation = memory_reallocate(
		                system_string_format,
		                sizeof( libcstring_system_character_t ) * ( format_length + 1 ) );

		if( reallocation == NULL )
		{
			goto on_error;
		}
		system_string_format = (libcstring_system_character_t *) reallocation;

#if defined( WINAPI )
		print_count = libcstring_wide_string_snwprintf(
		               system_string_format,
		               format_length + 1,
		               L"%S",
		               format );
#else
		print_count = libcstring_wide_string_snwprintf(
		               system_string_format,
		               format_length + 1,
		               L"%s",
		               format );
#endif

		if( print_count <= -1 )
		{
			format_length += LIBERROR_MESSAGE_INCREMENT_SIZE;
		}
		else if( ( (size_t) print_count > format_length )
		      || ( system_string_format[ print_count ] != 0 ) )
		{
			format_length = (size_t) print_count;
			print_count  = -1;
		}
		if( format_length >= LIBERROR_MESSAGE_MAXIMUM_SIZE )
		{
			goto on_error;
		}
	}
	while( print_count <= -1 );
#else
	system_string_format = (libcstring_system_character_t *) format;
#endif

#if defined( WINAPI )
	/* Rewrite %s to %S
	 */
	string_index  = 0;

	while( string_index < format_length )
	{
		if( system_string_format[ string_index ] == 0 )
		{
			break;
		}
		else if( system_string_format[ string_index ] == (libcstring_system_character_t) '%' )
		{
			string_index++;

			if( system_string_format[ string_index ] == (libcstring_system_character_t) 's' )
			{
				 system_string_format[ string_index ] = (libcstring_system_character_t) 'S';
			}
		}
		string_index++;
	}
#endif
	if( *error == NULL )
	{
		internal_error = memory_allocate_structure(
		                  liberror_internal_error_t );

		if( internal_error == NULL )
		{
			goto on_error;
		}
		internal_error->domain             = error_domain;
		internal_error->code               = error_code;
		internal_error->number_of_messages = 0;
		internal_error->messages           = NULL;
		internal_error->lengths            = NULL;

		*error = (liberror_error_t *) internal_error;
	}
	else
	{
		internal_error = (liberror_internal_error_t *) *error;
	}
	reallocation = memory_reallocate(
	                internal_error->messages,
	                sizeof( libcstring_system_character_t * ) * ( internal_error->number_of_messages + 1 ) );

	if( reallocation == NULL )
	{
		goto on_error;
	}
	internal_error->messages = (libcstring_system_character_t **) reallocation;

	reallocation = memory_reallocate(
	                internal_error->lengths,
	                sizeof( size_t ) * ( internal_error->number_of_messages + 1 ) );

	if( reallocation == NULL )
	{
		goto on_error;
	}
	internal_error->lengths = (size_t *) reallocation;

	message_index                             = internal_error->number_of_messages;
	internal_error->messages[ message_index ] = NULL;
	internal_error->lengths[ message_index ]  = 0;
	internal_error->number_of_messages       += 1;

	do
	{
		reallocation = memory_reallocate(
		                internal_error->messages[ message_index ],
		                sizeof( libcstring_system_character_t ) * message_size );

		if( reallocation == NULL )
		{
			memory_free(
			 internal_error->messages[ message_index ] );

			internal_error->messages[ message_index ] = NULL;

			break;
		}
		internal_error->messages[ message_index ] = (libcstring_system_character_t *) reallocation;

		VASTART(
		 argument_list,
		 const char *,
		 format );

		print_count = libcstring_system_string_vsprintf(
		               internal_error->messages[ message_index ],
		               message_size,
		               system_string_format,
		               argument_list );

		VAEND(
		 argument_list );

		if( print_count <= -1 )
		{
			message_size += LIBERROR_MESSAGE_INCREMENT_SIZE;
		}
		else if( ( (size_t) print_count > message_size )
		      || ( ( internal_error->messages[ message_index ] )[ print_count ] != 0 ) )
		{
			message_size = (size_t) ( print_count + 1 );
			print_count  = -1;
		}
		if( message_size >= LIBERROR_MESSAGE_MAXIMUM_SIZE )
		{
			memory_free(
			 internal_error->messages[ message_index ] );

			internal_error->messages[ message_index ] = NULL;
			internal_error->lengths[ message_index ]  = 0;

			break;
		}
		internal_error->lengths[ message_index ] = (size_t) print_count;
	}
	while( print_count <= -1 );

#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
	memory_free(
	 system_string_format );
#endif
	return;

on_error:
#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
	memory_free(
	 system_string_format );
#endif

	if( ( *error == NULL )
	 && ( internal_error != NULL ) )
	{
		memory_free(
		 internal_error );
	}
	return;
}

#undef VARARGS
#undef VASTART
#undef VAEND

/* Determines if an error equals a certain error code of a domain
 * Returns 1 if error matches or 0 if not
 */
int liberror_error_matches(
     liberror_error_t *error,
     int error_domain,
     int error_code )
{
	if( error == NULL )
	{
		return( 0 );
	}
	if( ( ( (liberror_internal_error_t *) error )->domain == error_domain )
	 && ( ( (liberror_internal_error_t *) error )->code == error_code ) )
	{
		return( 1 );
	}
	return( 0 );
}

/* Prints a descriptive string of the error to the stream
 * Returns the number of printed characters if successful or -1 on error
 */
int liberror_error_fprint(
     liberror_error_t *error,
     FILE *stream )
{
	liberror_internal_error_t *internal_error = NULL;
	int message_index                         = 0;
	int print_count                           = 0;

	if( error == NULL )
	{
		return( -1 );
	}
	internal_error = (liberror_internal_error_t *) error;

	if( internal_error->messages == NULL )
	{
		return( -1 );
	}
	if( stream == NULL )
	{
		return( -1 );
	}
	message_index = internal_error->number_of_messages - 1;

	if( internal_error->messages[ message_index ] != NULL )
	{
		print_count = fprintf(
		               stream,
		               "%" PRIs_LIBCSTRING_SYSTEM "\n",
		               internal_error->messages[ message_index ] );

		if( print_count <= -1 )
		{
			return( -1 );
		}
	}
	return( print_count );
}

/* Prints a descriptive string of the error to the string
 * The end-of-string character is not included in the return value
 * Returns the number of printed characters if successful or -1 on error
 */
int liberror_error_sprint(
     liberror_error_t *error,
     char *string,
     size_t size )
{
	liberror_internal_error_t *internal_error = NULL;
	size_t string_index                       = 0;
	int message_index                         = 0;

#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
	size_t print_count                        = 0;
#endif

	if( error == NULL )
	{
		return( -1 );
	}
	internal_error = (liberror_internal_error_t *) error;

	if( internal_error->messages == NULL )
	{
		return( -1 );
	}
	if( string == NULL )
	{
		return( -1 );
	}
	message_index = internal_error->number_of_messages - 1;

	if( internal_error->messages[ message_index ] != NULL )
	{
#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
#if defined( _MSC_VER )
		if( wcstombs_s(
		     &print_count,
		     &( string[ string_index ] ),
		     size - string_index,
		     internal_error->messages[ message_index ],
		     _TRUNCATE ) != 0 )
		{
			return( -1 );
		}
#else
		print_count = wcstombs(
			       &( string[ string_index ] ),
			       internal_error->messages[ message_index ],
			       size - string_index );

		if( print_count == (size_t) -1 )
		{
			return( -1 );
		}
#endif
		string_index += print_count;

		if( string_index >= size )
		{
			return( -1 );
		}
#else
		if( ( string_index + internal_error->lengths[ message_index ] ) > size )
		{
			return( -1 );
		}
		if( libcstring_narrow_string_copy(
		     &( string[ string_index ] ),
		     internal_error->messages[ message_index ],
		     internal_error->lengths[ message_index ] ) == NULL )
		{
			string[ string_index ] = 0;

			return( -1 );
		}
		string_index += internal_error->lengths[ message_index ];

		string[ string_index ] = 0;
#endif /* defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER ) */
	}
	if( string_index > (size_t) INT_MAX )
	{
		return( -1 );
	}
	return( (int) string_index );
}

/* Prints a backtrace of the error to the stream
 * Returns the number of printed characters if successful or -1 on error
 */
int liberror_error_backtrace_fprint(
     liberror_error_t *error,
     FILE *stream )
{
	liberror_internal_error_t *internal_error = NULL;
	int message_index                         = 0;
	int print_count                           = 0;
	int total_print_count                     = 0;

	if( error == NULL )
	{
		return( -1 );
	}
	internal_error = (liberror_internal_error_t *) error;

	if( internal_error->messages == NULL )
	{
		return( -1 );
	}
	if( stream == NULL )
	{
		return( -1 );
	}
	for( message_index = 0;
	     message_index < internal_error->number_of_messages;
	     message_index++ )
	{
		if( internal_error->messages[ message_index ] != NULL )
		{
			print_count = fprintf(
			               stream,
			               "%" PRIs_LIBCSTRING_SYSTEM "\n",
			               internal_error->messages[ message_index ] );

			if( print_count <= -1 )
			{
				return( -1 );
			}
			total_print_count += print_count;
		}
	}
	return( total_print_count );
}

/* Prints a backtrace of the error to the string
 * The end-of-string character is not included in the return value
 * Returns the number of printed characters if successful or -1 on error
 */
int liberror_error_backtrace_sprint(
     liberror_error_t *error,
     char *string,
     size_t size )
{
	liberror_internal_error_t *internal_error = NULL;
	size_t string_index                       = 0;
	int message_index                         = 0;

#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
	size_t print_count                        = 0;
#endif

	if( error == NULL )
	{
		return( -1 );
	}
	internal_error = (liberror_internal_error_t *) error;

	if( internal_error->messages == NULL )
	{
		return( -1 );
	}
	if( internal_error->lengths == NULL )
	{
		return( -1 );
	}
	if( string == NULL )
	{
		return( -1 );
	}
	for( message_index = 0;
	     message_index < internal_error->number_of_messages;
	     message_index++ )
	{
		if( internal_error->messages[ message_index ] != NULL )
		{
#if defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER )
#if defined( _MSC_VER )
			if( wcstombs_s(
			     &print_count,
			     &( string[ string_index ] ),
			     size - string_index,
			     internal_error->messages[ message_index ],
			     _TRUNCATE ) != 0 )
			{
				return( -1 );
			}
#else
			print_count = wcstombs(
			               &( string[ string_index ] ),
			               internal_error->messages[ message_index ],
			               size - string_index );

			if( print_count == (size_t) -1 )
			{
				return( -1 );
			}
#endif
			string_index += print_count;

			if( string_index >= size )
			{
				return( -1 );
			}
#else
			if( ( string_index + internal_error->lengths[ message_index ] ) > size )
			{
				return( -1 );
			}
			if( libcstring_narrow_string_copy(
			     &( string[ string_index ] ),
			     internal_error->messages[ message_index ],
			     internal_error->lengths[ message_index ] ) == NULL )
			{
				string[ string_index ] = 0;

				return( -1 );
			}
			string_index += internal_error->lengths[ message_index ];

			string[ string_index ] = 0;
#endif /* defined( LIBCSTRING_HAVE_WIDE_SYSTEM_CHARACTER ) */
		}
	}
	if( string_index > (size_t) INT_MAX )
	{
		return( -1 );
	}
	return( (int) string_index );
}
