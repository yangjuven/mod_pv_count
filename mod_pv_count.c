/* 
 *  mod_pv_count.c -- Apache pv_count module
 */ 

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"
#include "apr_strings.h"
#include "libmemcached/memcached.h"

#define PV_COUNT_DEFAULT_SERVERS "localhost:11211"

typedef struct {
    char *servers;    /* memcached servers */
    memcached_st *pc_memcache;
} pc_cfg;

module AP_MODULE_DECLARE_DATA pv_count_module;

/*
 * create the per server configuration
 */
static void *pc_create_server_config(apr_pool_t *p, server_rec *s)
{
    pc_cfg *svr = (pc_cfg *)apr_pcalloc(p, sizeof(pc_cfg));

    /* init new server config here */
    svr->servers = apr_pstrcat(p, PV_COUNT_DEFAULT_SERVERS, NULL);
    svr->pc_memcache = (memcached_st *)apr_pcalloc(p, sizeof(memcached_st));

    return (void *)svr;
}


/*
 * List of directives specific to pv count module.
 */
static const char *set_servers(cmd_parms *cmds, void *mconfig, const char *s)
{
    pc_cfg *svr = (pc_cfg *)ap_get_module_config(cmds->server->module_config, &pv_count_module);
    svr->servers = (char *)s;

    return NULL;
}

static const command_rec pv_count_cmds[] =
{
    /* String options */
    AP_INIT_TAKE1("PCServers", set_servers, NULL, RSRC_CONF, 
                "List of memcached servers to use (in server:port format, seperated by commas)"),
    {NULL},
};


/*
 * List of hook function
 */
static int post_config_handler(apr_pool_t *p, apr_pool_t *plog,
            apr_pool_t *ptemp, server_rec *s)
{
    pc_cfg *svr = (pc_cfg *)ap_get_module_config(s->module_config, &pv_count_module);

    if (svr->servers != NULL) {
        memcached_create(svr->pc_memcache);

        memcached_server_st *pc_servers = memcached_servers_parse(svr->servers);
        memcached_return_t rc = memcached_server_push(svr->pc_memcache, pc_servers);
        memcached_server_list_free(pc_servers);
    }

    return OK;
}

static int log_transaction_handler(request_rec *r)
{
    pc_cfg *svr = (pc_cfg *)ap_get_module_config(r->server->module_config, &pv_count_module);
    memcached_return_t rc;
    char key[] = "name";
    char *one_s = "1";
    rc = memcached_set(svr->pc_memcache, key, strlen(key), one_s, strlen(one_s), 0, 0);
    if (rc != MEMCACHED_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Memcached Set Error");
    }

    return OK;
}

static void pv_count_register_hooks(apr_pool_t *p)
{
    ap_hook_post_config(post_config_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_log_transaction(log_transaction_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA pv_count_module = {
    STANDARD20_MODULE_STUFF, 
    NULL,                          /* create per-dir    config structures */
    NULL,                          /* merge  per-dir    config structures */
    pc_create_server_config, /* create per-server config structures */
    NULL,                          /* merge  per-server config structures */
    pv_count_cmds,                 /* table of config file commands       */
    pv_count_register_hooks        /* register hooks                      */
};

