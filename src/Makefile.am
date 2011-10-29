bin_PROGRAMS = cdmifs

AM_CFLAGS = -Wall -Wextra -Werror -D_GNU_SOURCE $(CURL_CFLAGS) $(FUSE_CFLAGS) $(JANSSON_CFLAGS)

cdmifs_SOURCES = \
		cdmifs.c \
		util.c \
		net.c \
		cdmi.c \
		fs/attr.c \
		fs/control.c \
		fs/directory.c \
		fs/read.c \
		fs/write.c \
		b64/cdecode.c \
		b64/cencode.c
cdmifs_LDADD = $(CURL_LIBS) $(FUSE_LIBS) $(JANSSON_LIBS) $(MAGIC_LIBS)

