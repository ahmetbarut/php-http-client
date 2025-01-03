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
    
    if (path[0] == '/') {
        snprintf(full_url, len, "%s%s", intern->base_url, path);
    } else {
        snprintf(full_url, len, "%s/%s", intern->base_url, path);
    }
    
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

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(http_client)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "HttpClient", http_client_methods);
    http_client_ce = zend_register_internal_class(&ce);
    
    memcpy(&http_client_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    http_client_object_handlers.offset = XtOffsetOf(http_client_object, std);
    http_client_object_handlers.free_obj = http_client_free_obj;
    http_client_ce->create_object = http_client_create_object;
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(http_client)
{
    curl_global_cleanup();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(http_client)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "http_client support", "enabled");
    php_info_print_table_row(2, "Version", PHP_HTTP_CLIENT_VERSION);
    php_info_print_table_row(2, "libcurl Version", curl_version_info(CURLVERSION_NOW)->version);
    php_info_print_table_end();
}
/* }}} */

/* {{{ Object creation/destruction */
static void http_client_free_obj(zend_object *obj)
{
    http_client_object *intern = http_client_from_obj(obj);
    if (intern->base_url) {
        efree(intern->base_url);
    }
    if (intern->headers) {
        curl_slist_free_all(intern->headers);
    }
    zend_object_std_dtor(&intern->std);
}

zend_object *http_client_create_object(zend_class_entry *ce)
{
    http_client_object *intern = ecalloc(1, sizeof(http_client_object) + zend_object_properties_size(ce));
    
    zend_object_std_init(&intern->std, ce);
    object_properties_init(&intern->std, ce);
    
    intern->std.handlers = &http_client_object_handlers;
    
    return &intern->std;
}

/**
 * Callback function for handling CURL write operations
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
 * Build full URL from base URL and path
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
    
    if (path[0] == '/') {
        snprintf(full_url, len, "%s%s", intern->base_url, path);
    } else {
        snprintf(full_url, len, "%s/%s", intern->base_url, path);
    }
    
    return full_url;
}

/**
 * Performs HTTP request synchronously
 */
static int perform_request(char *url, char *method, char *data, struct curl_slist *headers)
{
    CURL *curl;
    CURLcode res;
    char *response = NULL;
    
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        if(headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
        
        if(strcmp(method, "POST") == 0) {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if(data) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            }
        }
        else if(strcmp(method, "PUT") == 0) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if(data) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            }
        }
        else if(strcmp(method, "DELETE") == 0) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            HTTP_CLIENT_G(last_error) = estrdup(curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            if(response) efree(response);
            return 0;
        }
        
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &HTTP_CLIENT_G(status_code));
        HTTP_CLIENT_G(response_body) = response;
        
        curl_easy_cleanup(curl);
        return 1;
    }
    
    return 0;
}

/* Class method implementations */

/**
 * HttpClient constructor
 */
PHP_METHOD(HttpClient, __construct)
{
    char *base_url = NULL;
    size_t base_url_len = 0;
    zval *headers = NULL;
    
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING_EX(base_url, base_url_len, 1, 0)
        Z_PARAM_ARRAY_EX(headers, 1, 0)
    ZEND_PARSE_PARAMETERS_END();
    
    http_client_object *obj = Z_HTTP_CLIENT_P(getThis());
    
    if(base_url && base_url_len > 0) {
        obj->base_url = estrndup(base_url, base_url_len);
    }
    
    if(headers) {
        zval *entry;
        zend_string *key;
        
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(headers), key, entry) {
            if(key) {
                char header_line[1024];
                convert_to_string(entry);
                snprintf(header_line, sizeof(header_line), "%s: %s", ZSTR_VAL(key), Z_STRVAL_P(entry));
                obj->headers = curl_slist_append(obj->headers, header_line);
            }
        } ZEND_HASH_FOREACH_END();
    }
}

/* HTTP Methods */

/* {{{ proto bool HttpClient::get(string $path) */
PHP_METHOD(HttpClient, get)
{
    char *path;
    size_t path_len;
    char *url;
    http_client_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(path, path_len)
    ZEND_PARSE_PARAMETERS_END();
    
    intern = Z_HTTP_CLIENT_P(getThis());
    url = build_full_url(intern, path);
    
    if (perform_request(url, "GET", NULL, intern->headers)) {
        efree(url);
        RETURN_TRUE;
    }
    
    efree(url);
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool HttpClient::post(string $path, string $data) */
PHP_METHOD(HttpClient, post)
{
    char *path, *data = NULL;
    size_t path_len, data_len;
    char *url;
    http_client_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(path, path_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(data, data_len)
    ZEND_PARSE_PARAMETERS_END();
    
    intern = Z_HTTP_CLIENT_P(getThis());
    url = build_full_url(intern, path);
    
    if (perform_request(url, "POST", data, intern->headers)) {
        efree(url);
        RETURN_TRUE;
    }
    
    efree(url);
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool HttpClient::put(string $path, string $data) */
PHP_METHOD(HttpClient, put)
{
    char *path, *data = NULL;
    size_t path_len, data_len;
    char *url;
    http_client_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(path, path_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(data, data_len)
    ZEND_PARSE_PARAMETERS_END();
    
    intern = Z_HTTP_CLIENT_P(getThis());
    url = build_full_url(intern, path);
    
    if (perform_request(url, "PUT", data, intern->headers)) {
        efree(url);
        RETURN_TRUE;
    }
    
    efree(url);
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool HttpClient::delete(string $path) */
PHP_METHOD(HttpClient, delete)
{
    char *path;
    size_t path_len;
    char *url;
    http_client_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(path, path_len)
    ZEND_PARSE_PARAMETERS_END();
    
    intern = Z_HTTP_CLIENT_P(getThis());
    url = build_full_url(intern, path);
    
    if (perform_request(url, "DELETE", NULL, intern->headers)) {
        efree(url);
        RETURN_TRUE;
    }
    
    efree(url);
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto array HttpClient::getHeaders() */
PHP_METHOD(HttpClient, getHeaders)
{
    http_client_object *intern;
    struct curl_slist *list;
    char *header, *value;
    
    intern = Z_HTTP_CLIENT_P(getThis());
    
    array_init(return_value);
    
    list = intern->headers;
    while(list) {
        add_next_index_string(return_value, list->data);
        list = list->next;
    }
}
/* }}} */

/* {{{ proto bool HttpClient::setHeader(string $key, string $value) */
PHP_METHOD(HttpClient, setHeader)
{
    char *key, *value;
    size_t key_len, value_len;
    char header_line[1024];
    http_client_object *intern;
    
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(key, key_len)
        Z_PARAM_STRING(value, value_len)
    ZEND_PARSE_PARAMETERS_END();
    
    intern = Z_HTTP_CLIENT_P(getThis());
    
    snprintf(header_line, sizeof(header_line), "%s: %s", key, value);
    intern->headers = curl_slist_append(intern->headers, header_line);
    
    RETURN_TRUE;
}
/* }}} */

/* {{{ proto int HttpClient::getStatusCode() */
PHP_METHOD(HttpClient, getStatusCode)
{
    RETURN_LONG(HTTP_CLIENT_G(status_code));
}
/* }}} */

/* {{{ proto string HttpClient::getResponseBody() */
PHP_METHOD(HttpClient, getResponseBody)
{
    if (HTTP_CLIENT_G(response_body)) {
        RETURN_STRING(HTTP_CLIENT_G(response_body));
    }
    RETURN_NULL();
}
/* }}} */

/* {{{ http_client_functions[] */
static const zend_function_entry http_client_methods[] = {
    PHP_ME(HttpClient, __construct,    arginfo_http_client_construct, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, get,            arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, post,           arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, put,            arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, delete,         arginfo_http_client_request, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, setHeader,      arginfo_http_client_header, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, getHeaders,     arginfo_http_client_void, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, getStatusCode,  arginfo_http_client_void, ZEND_ACC_PUBLIC)
    PHP_ME(HttpClient, getResponseBody, arginfo_http_client_void, ZEND_ACC_PUBLIC)
    PHP_FE_END
};
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
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(http_client)
#endif
