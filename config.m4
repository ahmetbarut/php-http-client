PHP_ARG_ENABLE([http_client],
  [whether to enable http_client support],
  [AS_HELP_STRING([--enable-http-client],
    [Enable http_client support])],
  [no])

if test "$PHP_HTTP_CLIENT" != "no"; then
  PHP_NEW_EXTENSION(http_client, http_client.c, $ext_shared)
fi
