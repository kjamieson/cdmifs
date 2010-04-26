/* cdmifs, a filesystem interface based on the CDMI standard.
 * Koen Bollen <meneer koenbollen nl>
 * 2010 GPL
 */
#ifdef HAVE_CONFIG
# include <config.h>
#endif

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "util.h"
#include "common.h"

#define INITIAL_SIZE 4096

struct buffer
{
	char *data;
	size_t size;
	size_t pos;
};

static size_t write_callback( void *ptr, size_t size, size_t nmemb, void *data );
static size_t read_callback( void *ptr, size_t size, size_t nmemb, void *data );

CURLcode curl_defaults( CURL *curl, int flags )
{
	CURLcode code;
	CURLcode res;
	res = CURLE_OK;

	code = curl_easy_setopt( curl, CURLOPT_USERAGENT, USERAGENT );
	if( code != CURLE_OK && res == CURLE_OK ) res = code;

	code = curl_easy_setopt( curl, CURLOPT_FORBID_REUSE, 1L );
	if( code != CURLE_OK && res == CURLE_OK ) res = code;

	if( options.curl_debug )
	{
		code = curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
		if( code != CURLE_OK && res == CURLE_OK ) res = code;
	}

	return res;
}
char *download( CURL *curl )
{
	static struct buffer block;
	CURLcode res;

	block.pos = 0;

	curl_easy_setopt( curl, CURLOPT_WRITEDATA, (void *)&block );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );

	res = curl_easy_perform( curl);
	if( res != CURLE_OK )
	{
		long err;
		curl_easy_getinfo( curl, CURLINFO_OS_ERRNO, &err );
		errno = err;
		return NULL;
	}

	block.data[block.pos] = 0;

	return block.data;
}

CURLcode upload( CURL *curl, const char *data, size_t size )
{
	CURLcode res;

	struct buffer block;
	block.data = (char*)data;
	block.size = size;
	block.pos = 0;

	curl_easy_setopt( curl, CURLOPT_UPLOAD, 1L );
	if( data != NULL && size > 0 )
	{
		curl_easy_setopt( curl, CURLOPT_READFUNCTION, read_callback );
		curl_easy_setopt( curl, CURLOPT_READDATA, &block );
		curl_easy_setopt( curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)size );
	}
	else
	{
		curl_easy_setopt( curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)0 );
	}

	res = curl_easy_perform( curl );
	if( res != CURLE_OK )
	{
		long err = 0;
		curl_easy_getinfo( curl, CURLINFO_OS_ERRNO, &err );
		errno = err == 0 ? EIO : err;
	}
	return res;
}

char *simple( const char *url )
{
	static CURL *curl = NULL;

	if( curl == NULL )
	{
		curl = curl_easy_init();
		curl_defaults( curl, 0 );
	}

	curl_easy_setopt( curl, CURLOPT_URL, url );

	char *data = download( curl );

	/* yea, I know I need to clean the curl, not gonna! */

	return data;
}

static size_t read_callback( void *ptr, size_t size, size_t nmemb, void *data )
{
	struct buffer *buf = (struct buffer*)data;

	int length = size * nmemb;
	int left = buf->size - buf->pos;

	if( left < length )
		length = left;

	memcpy( ptr, buf->data, length );
	buf->pos += length;

	return length;
}

static size_t write_callback( void *ptr, size_t size, size_t nmemb, void *stream )
{
	if( stream == NULL )
		return 0;
	size_t length = size * nmemb;
	struct buffer *buf = (struct buffer *)stream;
	//fprintf( stderr, "pos: %d\nsiz: %d\nlen: %d\n", buf->pos, buf->size, length );
	if( buf->pos + length + 1 > buf->size )
	{
		buf->size = buf->pos + length + 1;
		buf->data = alloc(buf->data, buf->size );
	}
	if( buf->data )
	{
		memcpy( buf->data+buf->pos, ptr, length );
		buf->pos += length;
	}
	return length;
}

#ifdef TEST_NET_C

#include <stdio.h>
#include <unistd.h>

#ifndef URL
# define URL "http://localhost/"
#endif

int main( int argc, char *argv[] )
{
	int i;
	char first[512];
	strncmp(first, simple( URL ), 511 );
	for( i = 0; i < 0; i++ )
	{
		if( strncmp( first, simple( URL ), 511 ) != 0 )
		{
			fprintf( stderr, "Failed at #%d!\n", i );
			break;
		}
	}
	printf( "done.\n" );
	return 0;
}

#endif /* !TEST_CURL */

