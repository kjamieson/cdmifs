/* cdmifs, a filesystem interface based on the CDMI standard.
 * Koen Bollen <meneer koenbollen nl>
 * 2010 GPL
 */
#include "common.h"

#include <assert.h>
#include <errno.h>
#include <fuse.h>
#include <fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <curl/curl.h>
#include <jansson.h>

#include "net.h"
#include "util.h"
#include "cdmi.h"

#include "fs/attr.h"
#include "fs/control.h"
#include "fs/directory.h"
#include "fs/read.h"
#include "fs/write.h"

static int parse_uri();
static int cdmifs_opt_proc( void *data, const char *arg, int key, struct fuse_args *outargs );
static void cdmifs_capabilities();
#if FUSE_VERSION >= 26
static void *cdmifs_init(struct fuse_conn_info *conn);
#else
static void *cdmifs_init(void);
#endif


struct fuse_operations cdmifs_operations = {
	 .init     = cdmifs_init,
	 .getattr  = cdmifs_getattr,
	 .readdir  = cdmifs_readdir,
	 .mkdir    = cdmifs_mkdir,
	 .rmdir    = cdmifs_rmdir,
	 .open     = cdmifs_open,
	 .create   = cdmifs_create,
	 .release  = cdmifs_release,
	 .read     = cdmifs_read,
	 .write    = cdmifs_write,
	 .truncate = cdmifs_truncate,
	 .unlink   = cdmifs_unlink,
	 .utimens  = cdmifs_utimens,
	 .rename   = cdmifs_rename,
	 .chmod    = cdmifs_chmod
};

struct options options;
#define OPT_KEY(t, p, v) { t, offsetof(struct options, p), v }
enum
{
   KEY_VERSION,
   KEY_HELP,
};
static struct fuse_opt opts[] =
{
	OPT_KEY("-u %s", username, 0),
	OPT_KEY("--username %s", username, 0),
	OPT_KEY("-p %s", password, 0),
	OPT_KEY("--password %s", password, 0),

	OPT_KEY("cdmifs_debug", debug, 1),
	OPT_KEY("curl_debug", curl_debug, 1),
	OPT_KEY("ssl",          ssl, 1),

	FUSE_OPT_KEY("-V",             KEY_VERSION),
	FUSE_OPT_KEY("--version",      KEY_VERSION),
	FUSE_OPT_KEY("-h",             KEY_HELP),
	FUSE_OPT_KEY("--help",         KEY_HELP),
	FUSE_OPT_END
};


#if FUSE_VERSION >= 26
static void *cdmifs_init(struct fuse_conn_info *conn)
{
	(void)conn;
#else
static void *cdmifs_init(void)
{
#endif
	if( !parse_uri() )
	{
		fprintf( stderr, "error: invalid uri: %s\n", options.uri );
		exit(1);
	}
	DEBUGV( "cdmifs %s\n", APP_VERSION );
	DEBUG( "Debug information:\n" );
	DEBUGV( " uri : %s\n", options.uri );
	DEBUGV( " host: %s\n", options.host );
	DEBUGV( " port: %s\n", options.port );
	DEBUGV( " root: %s\n", options.root );
	DEBUGV( " user: %s\n", options.username );

	cdmifs_capabilities();

	curl_global_init( CURL_GLOBAL_ALL );

	return NULL;
}


int main( int argc, char *argv[] )
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	memset(&options, 0, sizeof(struct options));

	if( fuse_opt_parse(&args, &options, opts, cdmifs_opt_proc) == -1 )
		return -1;

	if( !options.uri )
	{
		fprintf( stderr, "error: no uri given\n" );
		return 1;
	}

	ret = fuse_main( args.argc, args.argv, &cdmifs_operations, NULL);

	if( ret )
		printf("\n");

	fuse_opt_free_args(&args);

	return ret;
}

/*
 * Format: "[cdmi[s]://]host[:port][/root]"
 */
static int parse_uri()
{
	char *c;
	char *uri = options.uri;
	if( strncmp( uri, "cdmi://", 7 ) == 0 )
		uri += 7;
#ifdef USE_SSL
	else if( strncmp( uri, "cdmis://", 8 ) == 0 )
	{
		options.ssl = 1;
		uri += 8;
	}
#endif

	if( (c=strchr(uri, '/')) )
		options.root = strdup(c);
	else
		options.root = "/";
	if( options.root[1] != 0 && options.root[strlen(options.root)-1] == '/' )
		options.root[strlen(options.root)-1] = 0;
	options.host = strndup(uri, c-uri);
	if( (c=strchr(options.host, ':')) )
	{
		*c = 0;
		if( strlen(c+1) > 0 )
			options.port = c+1;
	}
	if( !options.port )
		options.port = DEFAULT_PORT;
	if( strlen(options.host) < 1 )
		return 0;

	return 1;
}

static int cdmifs_opt_proc( void *data, const char *arg, int key, struct fuse_args *outargs )
{
	(void)data;
	(void)outargs;
	switch( key )
	{
		case FUSE_OPT_KEY_NONOPT:
			if( options.uri == NULL )
			{
				options.uri = strdup(arg);
				return 0;
			}
			return 1;
		default:
			return 1;
	}
}

static int cdmifs_capability_is_true( const json_t *capa )
{
  return json_is_string(capa) &&
         !strcmp( json_string_value(capa), "true" );
}

static void cdmifs_capabilities()
{
	json_t *capa;
	options.gotmeta = 1;

	capa = getcapabilities(NULL);
	if( capa == NULL )
	{
		fprintf( stderr, "error: unable to fetch capabilities: %s\n", strerror(errno) );
		exit(1);
	}
	json_decref(capa);

	capa = getcapabilities("/container/");
	if( capa == NULL )
	{
		fprintf( stderr, "error: unable to fetch capabilities: %s\n", strerror(errno) );
		exit(1);
	}
	if( !cdmifs_capability_is_true( json_object_get(capa, "cdmi_list_children") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_list_children_range") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_create_container") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_delete_container") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_read_metadata") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_move_container") ) )
	{
		fprintf( stderr, "error: server doesn't fully support: containers\n" );
		exit(1);
	}
	if( !cdmifs_capability_is_true( json_object_get(capa, "cdmi_modify_metadata") ) )
	{
		options.gotmeta = 0;
	}
	json_decref(capa);


	capa = getcapabilities("/dataobject/");
	if( capa == NULL )
	{
		fprintf( stderr, "error: unable to fetch capabilities: %s\n", strerror(errno) );
		exit(1);
	}
	if( !cdmifs_capability_is_true( json_object_get(capa, "cdmi_read_value") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_read_value_range") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_modify_value") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_modify_value_range") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_read_metadata") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_delete_dataobject") ) ||
		!cdmifs_capability_is_true( json_object_get(capa, "cdmi_size") ) )
	{
		fprintf( stderr, "error: server doesn't fully support: dataobject\n" );
		exit(1);
	}
	if( !cdmifs_capability_is_true( json_object_get(capa, "cdmi_modify_metadata") ) )
	{
		options.gotmeta = 0;
	}
	json_decref(capa);

	if( options.gotmeta != 1 )
	{
		fprintf( stderr, "warning: unable to store permissions\n" );
	}

}

