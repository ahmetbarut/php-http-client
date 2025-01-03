# PHP HTTP Client Extension

[![Build and Test](https://github.com/username/php-http-client/actions/workflows/build-test.yml/badge.svg)](https://github.com/username/php-http-client/actions/workflows/build-test.yml)

A high-performance HTTP client extension for PHP that supports both synchronous and asynchronous requests. Built with libcurl for optimal performance and reliability.

## Features

- Synchronous and asynchronous HTTP requests
- Support for GET, POST, PUT, and DELETE methods
- Base URL configuration
- Custom header management
- JSON request/response handling
- Thread-safe implementation

## Building

```bash
phpize
./configure
make
make install
```

## Configuration
Add the following line to your php.ini:
```ini
extension=http_client.so
```

## Basic Usage

```php
// Initialize with base URL and headers
$client = new HttpClient('https://api.example.com', [
    'User-Agent' => 'PHP HTTP Client',
    'Accept' => 'application/json'
]);

// GET request
$client->get('/users/123');
echo $client->getStatusCode();
echo $client->getResponseBody();

// POST request with JSON data
$data = json_encode(['name' => 'John Doe']);
$client->post('/users', $data);
```

## Documentation

For detailed documentation including:
- Complete API reference
- Advanced usage examples
- Error handling
- Thread safety
- Performance considerations
- Contributing guidelines

Please see [docs.md](docs.md).

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

For bugs and feature requests, please use the GitHub issues system.
