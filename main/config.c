#include "voipsteg/config.h"

vsconf_node_t config[] = {
    //       name        isvalid isstr value
    { "sip-queuenum-a"   , false, false, -1 },
    { "sip-queuenum-b"   , false, false, -1 },
    { "sip-port-a"       , false, true,  "" },
    { "sip-port-b"       , false, true,  "" },

    { "rtp-covert-from"  , false, false, -1 }, /* covert queue lower bound   */
    { "rtp-service-from" , false, false, -1 }, /* service queue lower bound  */
    { "streams-count"    , false, false, -1 }, /* number of channels         */

    { "sender"           , false, true,  "" },
    { "filename"         , false, true,  "" },
    { "dstfile"          , false, true,  "" },
    { "delta"            , false, false, -1 },
    { "d0"               , false, false, -1 },
    { "d1"               , false, false, -1 },
    { "offset"           , false, false, -1 },
    { "tokenswitch"      , false, false, -1 },
    { "streamchoose"     , false, false, -1 }
};

const int CONFIG_ELEMENTS = sizeof(config)/sizeof(vsconf_node_t);

static int configRead = 0;
static void conf_check_file();

int vsconf_readconfig(const char *filename)
{
    xmlDocPtr doc;
    xmlNodePtr cur;

    if(configRead == 1)
    {
        SYS_LOG(E_NOTICE, "Config already read");
        return 1;
    }

    if( (doc = xmlParseFile(filename)) == NULL ) {
        SYS_LOG(E_ERROR, "Unable to read config file: %s", filename)
        exit(-1);
	return -1;
    }

    if( (cur = xmlDocGetRootElement(doc)) == NULL ) {
        SYS_LOG(E_WARNING, "Dokument %s jest pusty", filename)
        return -2;
    }

    for( cur = cur->xmlChildrenNode; NULL != cur; cur = cur->next ) {
        if( cur->type == XML_ELEMENT_NODE ) {
            xmlChar *key;
            int pos, val;
            
            pos = vsconf_findentry__(cur->name);
            if( pos >= 0 ) {
                key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
                config[pos].val.isvalid = true;

                if( config[pos].val.isstr == true ) {
                    strncpy(config[pos].val.value.strval, (char *)key, CONFIG_STRVAL_SIZE);
                } else if(config[pos].val.isstr == false ) {
                    val = atoi((const char*)key);
                    config[pos].val.value.numval = val;
                }
                xmlFree(key);
            }
        }
    }

    conf_check_file();

    configRead = 1;
    return 1;
}

const vsconf_value_t* vsconf_get(const char *nodename)
{
    int pos;
    xmlChar xmlName[CONFIG_STRVAL_SIZE];
    const xmlChar *ptr = xmlCharStrndup((const char*)nodename, CONFIG_STRVAL_SIZE);

    pos = vsconf_findentry__(ptr);

    if( -1 == pos ) {
        return NULL;
    }    

    return &config[pos].val;
}

int vsconf_findentry__(const xmlChar* xmlName)
{
    int ii;

    for(ii = 0; ii < CONFIG_ELEMENTS; ii++) {
        if( xmlStrcmp(xmlName, xmlCharStrdup(config[ii].name)) == STRING_EQUAL  ) {
            return ii;
        }
    }

    SYS_LOG(E_DEBUG, "Nie znany klucz \"%s\" w pliku xml", (unsigned char*)xmlName);
    return -1;
}

static void conf_check_file()
{
    int ii;

    for(ii = 0; ii < CONFIG_ELEMENTS; ii++) {
        if( config[ii].val.isvalid == false ) {
            SYS_LOG(E_ERROR, "Value for \"%s\" was not found in configuration file", config[ii].name);
        }
    }

}
