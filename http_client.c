/**
 * PHP HTTP Client Extension - Implementation
 * 
 * This file contains the implementation of the HTTP client extension.
 * It provides both synchronous and asynchronous HTTP request capabilities
 * using libcurl and pthreads.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_http_client.h"
#include <curl/curl.h>
#include <pthread.h>

/* Module globals initialization */
ZEND_DECLARE_MODULE_GLOBALS(http_client)

/* Class entry */
zend_class_entry *http_client_ce;

/* Object handlers */
static zend_object_handlers http_client_object_handlers;

/* Create/Free object */
static zend_object *http_client_create_object(zend_class_entry *ce)
{
    http_client_object *intern = ecalloc(1, sizeof(http_client_object) + zend_object_properties_size(ce));
    
    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);
    
    intern->base_url = NULL;
    intern->headers = NULL;
    
    intern->std.handlers = &http_client_object_handlers;
    
    return &intern->std;
}

static void http_client_free_object(zend_object *object)
{
    http_client_object *intern = http_client_from_obj(object);
    
    if (intern->base_url) {
        efree(intern->base_url);
    }
    
    if (intern->headers) {
        curl_slist_free_all(intern->headers);
    }
    
    zend_object_std_dtor(&intern->std);
}

/**
 * Callback function for handling CURL write operations
 * Stores response data in a buffer
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    char **response = (char **)userp;
    
    *response = erealloc(*response, realsize + 1);
    if(*response == NULL) {
        return 0;
    }
    
    memcpy(*response, contents, realsize);
    (*response)[realsize] = 0;
    
    return realsize;
}

/**
 * Builds the full URL for a request
 * @param intern HTTP client object
 * @param path Request path
 * @return Full URL
 */
static char *build_full_url(http_client_object *intern, const char *path)
{
    char *full_url;
    size_t len;
    
    if (!intern->base_url) {
        return estrdup(path);
    }
    
    len = strlen(intern->base_url) + strlen(path) + 2;
    full_url = emalloc(len);
    
    snprintf(full_url, len, "%s%s%s", 
        intern->base_url,
        (intern->base_url[strlen(intern->base_url) - 1] == '/' || path[0] == '/') ? "" : "/",
        (*path == '/' ? path + 1 : path));
    
    return full_url;
}

/**
 * Performs HTTP request synchronously
 * @param curl CURL handle
 * @param url The target URL
 * @param data Request body data (for POST/PUT)
 * @param method HTTP method (GET, POST, etc.)
 * @param response Response data
 * @param status_code Response status code
 * @param error Error message
 * @param headers Custom headers
 * @return Success status
 */
static int perform_request(CURL *curl, char *url, char *data, const char *method, char **response, long *status_code, char **error, struct curl_slist *headers)
{
    CURLcode res;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        if (strcmp(method, "PUT") == 0) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        }
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        *error = estrdup(curl_easy_strerror(res));
        return 0;
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status_code);
    return 1;
}

/**
 * Thread function for asynchronous requests
 * @param arg Async context containing request details
 */
static void *async_request_thread(void *arg)
{
    http_client_async_ctx *ctx = (http_client_async_ctx *)arg;
    CURL *curl = curl_easy_init();
    
    if (curl) {
        perform_request(curl, ctx->url, ctx->data, ctx->method, &ctx->response, &ctx->status_code, &ctx->error, ctx->headers);
        curl_easy_cleanup(curl);
    }
    
    ctx->is_complete = 1;
    return NULL;
}

/* {{{ HttpClient methods */
PHP_METHOD(HttpClient, __construct)
{
    char *base_url = NULL;
    size_t base_url_len = 0;
    zval *headers = NULL;
    http_client_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING_OR_NULL(base_url, base_url_len)
        Z_PARAM_ARRAY_OR_NULL(headers)
    ZEND_PARSE_PARAMETERS_END();
    
    intern = Z_HTTP_CLIENT_P(getThis());
    
    if (base_url) {
        intern->base_url = estrndup(base_url, base_url_len);
    }
    
    if (headers) {
        zval *header_value;
        zend_string *header_key;
        
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(headers), header_key, header_value) {
            char *header_line;
            size_t header_line_len;
            
            convert_to_string(header_value);
            
            header_line_len = ZSTR_LEN(header_key) + Z_STRLEN_P(header_value) + 3;
            header_line = emalloc(header_line_len);
            
            snprintf(header_line, header_line_len, "%s: %s", ZSTR_VAL(header_key), Z_STRVAL_P(header_value));
            
            intern->headers = curl_slist_append(intern->headers, header_line);
            efree(header_line);
        } ZEND_HASH_FOREACH_END();
    }
    
    HTTP_CLIENT_G(response_body) = NULL;
    HTTP_CLIENT_G(status_code) = 0;
    HTTP_CLIENT_G(last_error) = NULL;
    HTTP_CLIENT_G(async_ctx) = NULL;
}

/* Synchronous methods */
PHP_METHOD(HttpClient, get)
{
    char *path;
    size_t path_len;
    CURL *curl;
    http_client_object *intern;
    char *full_url;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &path, &path_len) == FAILURE) {
        RETURN_FALSE;
    }

    intern = Z_HTTP_CLIENT_P(getThis());
    full_url = build_full_url(intern, path);

    curl = curl_easy_init();
    if (curl) {
        if (perform_request(curl, full_url, NULL, "GET", &HTTP_CLIENT_G(response_body), 
                          &HTTP_CLIENT_G(status_code), &HTTP_CLIENT_G(last_error), intern->headers)) {
            efree(full_url);
            curl_easy_cleanup(curl);
            RETURN_TRUE;
        }
        curl_easy_cleanup(curl);
    }
    efree(full_url);
    RETURN_FALSE;
}

PHP_METHOD(HttpClient, post)
{
    char *path, *post_data;
    size_t path_len, post_data_len;
    CURL *curl;
    http_client_object *intern;
    char *full_url;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &path, &path_len, &post_data, &post_data_len) == FAILURE) {
        RETURN_FALSE;
    }

    intern = Z_HTTP_CLIENT_P(getThis());
    full_url = build_full_url(intern, path);

    curl = curl_easy_init();
    if (curl) {
        if (perform_request(curl, full_url, post_data, "POST", &HTTP_CLIENT_G(response_body),
                          &HTTP_CLIENT_G(status_code), &HTTP_CLIENT_G(last_error), intern->headers)) {
            efree(full_url);
            curl_easy_cleanup(curl);
            RETURN_TRUE;
        }
        curl_easy_cleanup(curl);
    }
    efree(full_url);
    RETURN_FALSE;
}

PHP_METHOD(HttpClient, put)
{
    char *path, *put_data;
    size_t path_len, put_data_len;
    CURL *curl;
    http_client_object *intern;
    char *full_url;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &path, &path_len, &put_data, &put_data_len) == FAILURE) {
        RETURN_FALSE;
    }

    intern = Z_HTTP_CLIENT_P(getThis());
    full_url = build_full_url(intern, path);

    curl = curl_easy_init();
    if (curl) {
        if (perform_request(curl, full_url, put_data, "PUT", &HTTP_CLIENT_G(response_body),
                          &HTTP_CLIENT_G(status_code), &HTTP_CLIENT_G(last_error), intern->headers)) {
            efree(full_url);
            curl_easy_cleanup(curl);
            RETURN_TRUE;
        }
        curl_easy_cleanup(curl);
    }
    efree(full_url);
    RETURN_FALSE;
}

PHP_METHOD(HttpClient, delete)
{
    char *path;
    size_t path_len;
    CURL *curl;
    http_client_object *intern;
    char *full_url;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &path, &path_len) == FAILURE) {
        RETURN_FALSE;
    }

    intern = Z_HTTP_CLIENT_P(getThis());
    full_url = build_full_url(intern, path);

    curl = curl_easy_init();
    if (curl) {
        if (perform_request(curl, full_url, NULL, "DELETE", &HTTP_CLIENT_G(response_body),
                          &HTTP_CLIENT_G(status_code), &HTTP_CLIENT_G(last_error), intern->headers)) {
            efree(full_url);
            curl_easy_cleanup(curl);
            RETURN_TRUE;
        }
        curl_easy_cleanup(curl);
    }
    efree(full_url);
    RETURN_FALSE;
}

/* Asynchronous methods */
static void start_async_request(INTERNAL_FUNCTION_PARAMETERS, const char *method)
{
    char *path, *data = NULL;
    size_t path_len, data_len;
    http_client_object *intern;
    char *full_url;

    if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
        if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &path, &path_len, &data, &data_len) == FAILURE) {
            RETURN_FALSE;
        }
    } else {
        if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &path, &path_len) == FAILURE) {
            RETURN_FALSE;
        }
    }

    if (HTTP_CLIENT_G(async_ctx) != NULL) {
        php_error_docref(NULL, E_WARNING, "Another async request is already in progress");
        RETURN_FALSE;
    }

    intern = Z_HTTP_CLIENT_P(getThis());
    full_url = build_full_url(intern, path);

    HTTP_CLIENT_G(async_ctx) = emalloc(sizeof(http_client_async_ctx));
    HTTP_CLIENT_G(async_ctx)->url = full_url;
    HTTP_CLIENT_G(async_ctx)->method = estrdup(method);
    HTTP_CLIENT_G(async_ctx)->data = data ? estrdup(data) : NULL;
    HTTP_CLIENT_G(async_ctx)->is_complete = 0;
    HTTP_CLIENT_G(async_ctx)->response = NULL;
    HTTP_CLIENT_G(async_ctx)->error = NULL;
    HTTP_CLIENT_G(async_ctx)->headers = intern->headers;

    pthread_create(&HTTP_CLIENT_G(async_ctx)->thread, NULL, async_request_thread, HTTP_CLIENT_G(async_ctx));
    RETURN_TRUE;
}

PHP_METHOD(HttpClient, getAsync)
{
    start_async_request(INTERNAL_FUNCTION_PARAM_PASSTHRU, "GET");
}

PHP_METHOD(HttpClient, postAsync)
{
    start_async_request(INTERNAL_FUNCTION_PARAM_PASSTHRU, "POST");
}

PHP_METHOD(HttpClient, putAsync)
{
    start_async_request(INTERNAL_FUNCTION_PARAM_PASSTHRU, "PUT");
}

PHP_METHOD(HttpClient, deleteAsync)
{
    start_async_request(INTERNAL_FUNCTION_PARAM_PASSTHRU, "DELETE");
}

PHP_METHOD(HttpClient, wait)
{
    if (HTTP_CLIENT_G(async_ctx) == NULL) {
        RETURN_FALSE;
    }

    pthread_join(HTTP_CLIENT_G(async_ctx)->thread, NULL);

    if (HTTP_CLIENT_G(async_ctx)->error) {
        HTTP_CLIENT_G(last_error) = HTTP_CLIENT_G(async_ctx)->error;
        RETVAL_FALSE;
    } else {
        HTTP_CLIENT_G(response_body) = HTTP_CLIENT_G(async_ctx)->response;
        HTTP_CLIENT_G(status_code) = HTTP_CLIENT_G(async_ctx)->status_code;
        RETVAL_TRUE;
    }

    efree(HTTP_CLIENT_G(async_ctx)->url);
    efree(HTTP_CLIENT_G(async_ctx)->method);
    if (HTTP_CLIENT_G(async_ctx)->data) {
        efree(HTTP_CLIENT_G(async_ctx)->data);
    }
    efree(HTTP_CLIENT_G(async_ctx));
    HTTP_CLIENT_G(async_ctx) = NULL;
}

PHP_METHOD(HttpClient, setHeader)
{
    char *key, *value;
    size_t key_len, value_len;
    http_client_object *intern;
    char *header_line;
    size_t header_line_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &key, &key_len, &value, &value_len) == FAILURE) {
        RETURN_FALSE;
    }

    intern = Z_HTTP_CLIENT_P(getThis());
    
    header_line_len = key_len + value_len + 3;
    header_line = emalloc(header_line_len);
    
    snprintf(header_line, header_line_len, "%s: %s", key, value);
    
    intern->headers = curl_slist_append(intern->headers, header_line);
    efree(header_line);
    
    RETURN_TRUE;
}

PHP_METHOD(HttpClient, getHeaders)
{
    http_client_object *intern;
    struct curl_slist *list;
    array_init(return_value);

    intern = Z_HTTP_CLIENT_P(getThis());
    
    if (intern->headers) {
        for (list = intern->headers; list; list = list->next) {
            add_next_index_string(return_value, list->data);
        }
    }
}

PHP_METHOD(HttpClient, getStatusCode)
{
    RETURN_LONG(HTTP_CLIENT_G(status_code));
}

PHP_METHOD(HttpClient, getResponseBody)
{
    if (HTTP_CLIENT_G(response_body)) {
        RETURN_STRING(HTTP_CLIENT_G(response_body));
    }
    RETURN_NULL();
}
/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo_http_client_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_http_client_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, base_url)
    ZEND_ARG_INFO(0, headers)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_http_client_request, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_http_client_header, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ http_client_functions[] */
static const zend_function_entry http_client_methods[] = {
    PHP_ME(HttpClient, __construct,    arginfo_http_client_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(HttpClient, get,            arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, post,           arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, put,            arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, delete,         arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, getAsync,       arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, postAsync,      arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, putAsync,       arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, deleteAsync,    arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, wait,           arginfo_http_client_void,     ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, setHeader,      arginfo_http_client_header,   ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, getHeaders,     arginfo_http_client_void,     ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, getStatusCode,  arginfo_http_client_void,     ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, getResponseBody, arginfo_http_client_void,    ZEND_ACC_PUBLIC)
    PHP_FE_END
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(http_client)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "HttpClient", http_client_methods);
    
    http_client_ce = zend_register_internal_class(&ce);
    http_client_ce->create_object = http_client_create_object;
    
    memcpy(&http_client_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    http_client_object_handlers.offset = XtOffsetOf(http_client_object, std);
    http_client_object_handlers.free_obj = http_client_free_object;
    
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(http_client)
{
    if (HTTP_CLIENT_G(response_body)) {
        efree(HTTP_CLIENT_G(response_body));
    }
    if (HTTP_CLIENT_G(last_error)) {
        efree(HTTP_CLIENT_G(last_error));
    }
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(http_client)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "http_client support", "enabled");
    php_info_print_table_row(2, "Version", PHP_HTTP_CLIENT_VERSION);
    php_info_print_table_end();
}
/* }}} */

/* {{{ http_client_module_entry */
zend_module_entry http_client_module_entry = {
    STANDARD_MODULE_HEADER,
    "http_client",
    NULL,
    PHP_MINIT(http_client),
    PHP_MSHUTDOWN(http_client),
    NULL,
    NULL,
    PHP_MINFO(http_client),
    PHP_HTTP_CLIENT_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_HTTP_CLIENT
ZEND_GET_MODULE(http_client)
#endif
