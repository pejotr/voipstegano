
#ifndef _VOIPSTEG_CONFIG_H_
#define _VOIPSTEG_CONFIG_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "voipsteg/logger.h"

#define RTP_COVERT_FROM   "rtp-covert-from"
#define RTP_SERVICE_FROM  "rtp-service-from"
#define RTP_STREAMS_COUNT "streams-count"

const int CONFIG_STRVAL_SIZE = 128;
const int STRING_EQUAL  = 0;



typedef struct {
    bool isvalid;
    bool isstr;

    union {
        char strval[CONFIG_STRVAL_SIZE];
        int numval;
    } value;

} vsconf_value_t;

typedef struct {
    char name[CONFIG_STRVAL_SIZE];
    vsconf_value_t val;
} vsconf_node_t;

//! Read configuration file and fill Config structure
int vsconf_readconfig(const char *filename);

//! Get configuration value is provided nodename was found in configuration file
const vsconf_value_t* vsconf_get(const char *nodename);

//! Find entry index in config array
/*!
    @return index value greater than 0 if name as found in config array
              -1  in all other cases
 */
int vsconf_findentry__(const xmlChar* name);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif /* _VOIPSTEG_CONFIG_H_  */
