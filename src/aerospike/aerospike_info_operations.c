#include "php.h"
#include "aerospike/as_status.h"
#include "aerospike/aerospike_key.h"
#include "aerospike/as_error.h"
#include "aerospike/as_record.h"
#include "aerospike/aerospike_info.h"

#include "aerospike_common.h"
#include "aerospike_policy.h"

#define MAX_HOST_COUNT 128
#define INFO_REQUEST_RESPONSE_DELIMITER "\t"
#define INFO_RESPONSE_END "\n"
#define HOST_DELIMITER ";"
#define IP_PORT_DELIMITER ":"

/*
 *******************************************************************************************************
 * Wrapper function to perform an aerospike_info_host within the C client.
 *
 * @param as_object_p           The C client's aerospike object.
 * @param error_p               The as_error to be populated by the function
 *                              with the encountered error if any.
 * @param request_str_p         The request string for info.
 * @param response_str_p        The response zval to be initialized as a string and populated with the
 *                              actual info response by this method.
 * @param host                  The optional host array containing a single addr and port.
 * @param options_p             The user's optional policy options to be used if set, else defaults.
 *
 *******************************************************************************************************
 */
extern as_status
aerospike_info_specific_host(aerospike* as_object_p,
        as_error* error_p, char* request_str_p,
        zval* response_str_p, zval* host, zval* options_p TSRMLS_DC)
{
    as_policy_info              info_policy;
    zval**                      host_name = NULL;
    zval**                      port = NULL;
    char*                       address = (char *) as_object_p->config.hosts[0].addr;
    long                        port_no = as_object_p->config.hosts[0].port;
    char*                       response_p = NULL;

    set_policy(NULL, NULL, NULL, NULL, &info_policy, NULL, NULL, NULL,
            options_p, error_p TSRMLS_CC);

    if (AEROSPIKE_OK != (error_p->code)) {
        DEBUG_PHP_EXT_DEBUG("Unable to set policy");
        goto exit;
    }
    
    if (host) {
        if (FAILURE == zend_hash_find(Z_ARRVAL_P(host),
                    PHP_AS_KEY_DEFINE_FOR_ADDR,
                PHP_AS_KEY_DEFINE_FOR_ADDR_LEN + 1, (void**)&host_name)) {
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM,
                    "Unable to find addr");
            DEBUG_PHP_EXT_DEBUG("Unable to find addr");
            goto exit;
        }
        if (FAILURE == zend_hash_find(Z_ARRVAL_P(host),
                    PHP_AS_KEY_DEFINE_FOR_PORT,
                    PHP_AS_KEY_DEFINE_FOR_PORT_LEN + 1,  (void**)&port)) {
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM,
                    "Unable to find port");
            DEBUG_PHP_EXT_DEBUG("Unable to find port");
            goto exit;
        }
        if ((Z_TYPE_PP(host_name) != IS_STRING) || (Z_TYPE_PP(port) != IS_LONG)) {
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM,
                    "Host parameters are not correct");
            DEBUG_PHP_EXT_DEBUG("Host parameters are not correct");
            goto exit;
        }

        address = Z_STRVAL_PP(host_name);
        port_no = Z_LVAL_PP(port);
    }

    if (AEROSPIKE_OK != aerospike_info_host(as_object_p, error_p, &info_policy,
                (const char *) address, (uint16_t) port_no, request_str_p,
                &response_p)) {
            DEBUG_PHP_EXT_DEBUG(error_p->message);
            goto exit;
    }

    if (response_p != NULL) {
        ZVAL_STRINGL(response_str_p, response_p, strlen(response_p), 1);
    } else {
        ZVAL_STRINGL(response_str_p, "", 0, 1);
    }

exit:
    return error_p->code;
}

/*
 *******************************************************************************************************
 * Helper function to get all nodes within the cluster.
 *
 * @param as_object_p           The C client's aerospike object.
 * @param error_p               The as_error to be populated by the function
 *                              with the encountered error if any.
 * @param return_p              The return zval to be populated with the
 *                              node ip:port pairs by this method.
 * @param host                  The optional host array containing a single addr and port.
 * @param options_p             The user's optional policy options to be used if set, else defaults.
 *
 *******************************************************************************************************
 */
extern as_status
aerospike_info_get_cluster_nodes(aerospike* as_object_p,
        as_error* error_p, zval* return_p, zval* host, zval* options_p TSRMLS_DC)
{
    as_policy_info              info_policy;
    zval*                       response_services_p = NULL;
    zval*                       response_service_p = NULL;
    char*                       tok = NULL;
    char*                       saved = NULL;
    zval*                       current_host_p[MAX_HOST_COUNT];
    uint32_t                    host_index = 0;
    bool                        iter_first = true;
    bool                        break_flag = false;


    MAKE_STD_ZVAL(response_services_p);
    MAKE_STD_ZVAL(response_service_p);
    
    if (AEROSPIKE_OK !=
            aerospike_info_specific_host(as_object_p, error_p,
                    "services", response_services_p, host, options_p TSRMLS_CC)) {
        DEBUG_PHP_EXT_ERROR("getNodes: services call returned an error");
        goto exit;
    }

    if (AEROSPIKE_OK !=
            aerospike_info_specific_host(as_object_p, error_p,
                    "service", response_service_p, host, options_p TSRMLS_CC)) {
        DEBUG_PHP_EXT_ERROR("getNodes: service call returned an error");
        goto exit;
    }

    for (host_index = 0; host_index < MAX_HOST_COUNT; host_index++) {
        current_host_p[host_index] = NULL;
    }

    host_index = 0;

    MAKE_STD_ZVAL(current_host_p[host_index]);
    array_init(current_host_p[host_index]);

    tok = strtok_r(Z_STRVAL_P(response_service_p), INFO_REQUEST_RESPONSE_DELIMITER, &saved);
    if (tok == NULL) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
            "Unable to get addr in service");
        DEBUG_PHP_EXT_DEBUG("Unable to get addr in service");
        goto exit;
    }

    tok = strtok_r(NULL, IP_PORT_DELIMITER, &saved);
    if (tok == NULL) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
            "Unable to get addr in service");
        DEBUG_PHP_EXT_DEBUG("Unable to get addr in service");
        goto exit;
    }

    if (0 != add_assoc_stringl(current_host_p[host_index], "addr", tok, strlen(tok), 1)) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
            "Unable to get addr");
        DEBUG_PHP_EXT_DEBUG("Unable to get addr");
        goto exit;
    }

    tok = strtok_r(NULL, INFO_RESPONSE_END, &saved);
    if (tok == NULL) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
            "Unable to get port in service");
        DEBUG_PHP_EXT_DEBUG("Unable to get port in service");
        goto exit;
    }

    if (0 != add_assoc_stringl(current_host_p[host_index], "port", tok, strlen(tok), 1)) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                "Unable to get port");
        DEBUG_PHP_EXT_DEBUG("Unable to get port");
        goto exit;
    }

    if (0 != add_next_index_zval(return_p, current_host_p[host_index])) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                "Unable to get addr-port");
        DEBUG_PHP_EXT_DEBUG("Unable to get addr-port");
        goto exit;
    }

    host_index++;
    tok = strtok_r(Z_STRVAL_P(response_services_p), INFO_REQUEST_RESPONSE_DELIMITER, &saved);

    while (tok != NULL) {
        tok = strtok_r(NULL, IP_PORT_DELIMITER, &saved);
        if (tok == NULL) {
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                    "Unable to get addr");
            DEBUG_PHP_EXT_DEBUG("Unable to get addr");
            goto exit;
        }

        MAKE_STD_ZVAL(current_host_p[host_index]);
        array_init(current_host_p[host_index]);

        if (0 != add_assoc_stringl(current_host_p[host_index], "addr", tok, strlen(tok), 1)) {
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                    "Unable to get addr");
            DEBUG_PHP_EXT_DEBUG("Unable to get addr");
            goto exit;
        }

        tok = strtok_r(NULL, HOST_DELIMITER, &saved);
        if (tok == NULL) {
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                    "Unable to get port");
            DEBUG_PHP_EXT_DEBUG("Unable to get port");
            goto exit;
        }

        if (strstr(tok, INFO_RESPONSE_END)) {
            tok = strtok_r(tok, INFO_RESPONSE_END, &saved);
            break_flag = true;
        }

        if (0 != add_assoc_stringl(current_host_p[host_index], "port", tok, strlen(tok), 1)) {
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                    "Unable to get port");
            DEBUG_PHP_EXT_DEBUG("Unable to get port");
            goto exit;
        }

        if (0 != add_next_index_zval(return_p, current_host_p[host_index])) {
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                    "Unable to get addr-port");
            DEBUG_PHP_EXT_DEBUG("Unable to get addr-port");
            goto exit;
        }
        host_index++;

        if (break_flag == true) {
            goto exit;
        }
    }

exit:
    if (response_services_p) {
        zval_ptr_dtor(&response_services_p);
    }
    if (response_service_p) {
        zval_ptr_dtor(&response_service_p);
    }
    if (error_p->code != AEROSPIKE_OK) {
        for (host_index = 0; host_index < MAX_HOST_COUNT; host_index++) {
            if (current_host_p[host_index]) {
                zval_ptr_dtor(&current_host_p[host_index]);
            }
        }
    }
    return error_p->code;
}

/*
 *******************************************************************************************************
 * Callback for as_scan_foreach and as_query_foreach functions.
 * It processes the as_val and translates it into an equivalent zval array.
 * It then calls the user registered callback passing the zval array as an
 * argument.
 *       
 * @param p_val             The current as_val to be passed on to the
 *                          user callback as an argument.
 * @param udata             The userland_callback instance filled
 *                          with return_value and error.
 *
 * @return true if callback is successful; else false.
 *******************************************************************************************************
 */

extern bool
aerospike_info_callback(const as_error* err, const as_node* node,
        char* request, char* response, void* udata) 
{
    TSRMLS_FETCH();
    foreach_callback_info_udata* udata_ptr = (foreach_callback_info_udata*)udata;

    if (node) {
        if (0 != add_assoc_stringl(udata_ptr->udata_p, node->name, response, strlen(response), 1)) {
            DEBUG_PHP_EXT_DEBUG("Unable to get node info");
            goto exit;
        }
    } else {
        return false;
    }
exit:
    return true;
}

/*
 *******************************************************************************************************
 * Send an info request to multiple cluster nodes.
 *
 * @param as_object_p               The C client's aerospike object.
 * @param error_p                   The C client's as_error to be set to the
 *                                  encountered error.
 * @param request_str_p             request string
 * @config_p                        Config filter array.
 * @return_value_p                  An array of response strings keyed by the
 *                                  cluster node ID.
 * @param options_p                 Options array with read timeout.
 *
 *******************************************************************************************************
 */
extern as_status
aerospike_info_request_multiple_nodes(aerospike* as_object_p,
        as_error* error_p, char* request_str_p, zval* config_p,
        zval* return_value_p, zval* options_p TSRMLS_DC)
{
    foreach_callback_info_udata info_callback_udata;
    as_policy_info              info_policy;

    if ((!request_str_p)/* || (!config_p)*/) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                "Request string is empty");
        DEBUG_PHP_EXT_DEBUG("Request string is empty");
        goto exit;
    }

    set_policy(NULL, NULL, NULL, NULL, &info_policy, NULL, NULL, NULL,
            options_p, error_p TSRMLS_CC);

    if (AEROSPIKE_OK != (error_p->code)) {
        DEBUG_PHP_EXT_DEBUG("Unable to set policy");
        goto exit;
    }

    info_callback_udata.udata_p = return_value_p;
    info_callback_udata.error_p = error_p;
    //info_callback_udata.error_p = config_p;

    if ( AEROSPIKE_OK != aerospike_info_foreach(as_object_p, error_p, &info_policy,
            request_str_p, (aerospike_info_foreach_callback) aerospike_info_callback, &info_callback_udata)) {
        DEBUG_PHP_EXT_DEBUG(error_p->message);
        goto exit;
    }
exit:
    return error_p->code;
}
