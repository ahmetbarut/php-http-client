#ifndef PHP_HTTP_CLIENT_H
#define PHP_HTTP_CLIENT_H

/**
 * PHP HTTP Client Extension
 * 
 * A high-performance HTTP client extension that supports both synchronous
 * and asynchronous requests. Built with libcurl for optimal performance.
 * 
 * @author Your Name
 * @package http_client
 * @version 0.1.0
 */

extern zend_module_entry http_client_module_entry;
#define phpext_http_client_ptr &http_client_module_entry

#define PHP_HTTP_CLIENT_VERSION "0.1.0"

/* HTTP Status Codes */
#define HTTP_STATUS_OK 200
#define HTTP_STATUS_CREATED 201
#define HTTP_STATUS_ACCEPTED 202
#define HTTP_STATUS_BAD_REQUEST 400
#define HTTP_STATUS_UNAUTHORIZED 401
#define HTTP_STATUS_FORBIDDEN 403
#define HTTP_STATUS_NOT_FOUND 404
#define HTTP_STATUS_SERVER_ERROR 500

/* Function declarations */
PHP_MINIT_FUNCTION(http_client);
PHP_MSHUTDOWN_FUNCTION(http_client);
PHP_MINFO_FUNCTION(http_client);

/* Class method declarations */
PHP_METHOD(HttpClient, __construct);
PHP_METHOD(HttpClient, get);
PHP_METHOD(HttpClient, post);
PHP_METHOD(HttpClient, put);
PHP_METHOD(HttpClient, delete);
PHP_METHOD(HttpClient, getAsync);
PHP_METHOD(HttpClient, postAsync);
PHP_METHOD(HttpClient, putAsync);
PHP_METHOD(HttpClient, deleteAsync);
PHP_METHOD(HttpClient, wait);
PHP_METHOD(HttpClient, getStatusCode);
PHP_METHOD(HttpClient, getResponseBody);
PHP_METHOD(HttpClient, setHeader);
PHP_METHOD(HttpClient, getHeaders);

/**
 * Async context structure
 * Holds information about an ongoing asynchronous request
 */
typedef struct _http_client_async_ctx {
    char *url;              /* Full URL for the request */
    char *data;            /* POST/PUT request data */
    char *method;          /* HTTP method (GET, POST, etc.) */
    pthread_t thread;      /* Thread handle */
    int is_complete;       /* Request completion flag */
    char *response;        /* Response body */
    long status_code;      /* HTTP status code */
    char *error;          /* Error message if any */
    struct curl_slist *headers;  /* Request headers */
} http_client_async_ctx;

/**
 * Main object structure
 * Stores instance-specific data like base URL and headers
 */
typedef struct _http_client_object {
    char *base_url;        /* Base URL for all requests */
    struct curl_slist *headers;  /* Default headers */
    zend_object std;      /* Standard object handle */
} http_client_object;

/* Object handlers */
static inline http_client_object *http_client_from_obj(zend_object *obj) {
    return (http_client_object*)((char*)(obj) - XtOffsetOf(http_client_object, std));
}

#define Z_HTTP_CLIENT_P(zv) http_client_from_obj(Z_OBJ_P(zv))

/**
 * Module globals
 * Stores request-specific data
 */
ZEND_BEGIN_MODULE_GLOBALS(http_client)
    char *last_error;     /* Last error message */
    long status_code;     /* Last HTTP status code */
    char *response_body;  /* Last response body */
    http_client_async_ctx *async_ctx;  /* Current async context */
ZEND_END_MODULE_GLOBALS(http_client)

#ifdef ZTS
#define HTTP_CLIENT_G(v) TSRMG(http_client_globals_id, zend_http_client_globals *, v)
#else
#define HTTP_CLIENT_G(v) (http_client_globals.v)
#endif

#if defined(ZTS) && defined(COMPILE_DL_HTTP_CLIENT)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif /* PHP_HTTP_CLIENT_H */
