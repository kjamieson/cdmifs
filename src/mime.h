/* cdmifs, a filesystem interface based on the CDMI standard.
 * Koen Bollen <meneer koenbollen nl>
 * 2010 GPL
 */
#ifndef MIME_H
#define MIME_H

#define M_JSON         0
#define M_OBJECT       1
#define M_DATAOBJECT   2
#define M_CONTAINER    3
#define M_CAPABILITIES 4

static char *mime[] = {
	"application/json",
  "application/cdmi-capability,application/cdmi-container,application/cdmi-domain,application/cdmi-object,application/cdmi-queue",
  "application/cdmi-object",
  "application/cdmi-container",
  "application/cdmi-capability",
	NULL
};

#endif /* !MIME_H */

