# PHP HTTP Client Extension Documentation

A high-performance HTTP client extension for PHP that supports both synchronous and asynchronous requests. Built with libcurl for optimal performance and reliability.

## Features

- Synchronous and asynchronous HTTP requests
- Support for GET, POST, PUT, and DELETE methods
- Base URL configuration
- Custom header management
- JSON request/response handling
- Thread-safe implementation
- Supports PHP 8.0+

## Installation

### Requirements

- PHP 8.0 or higher
- libcurl
- pthread support

### Building from Source

```bash
phpize
./configure
make
make install
```

Add to your php.ini:
```ini
extension=http_client.so
```

## Usage

### Basic Usage

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

// PUT request
$client->put('/users/123', json_encode(['status' => 'active']));

// DELETE request
$client->delete('/users/123');
```

### Header Management

```php
// Add headers during initialization
$client = new HttpClient('https://api.example.com', [
    'Authorization' => 'Bearer token123'
]);

// Add headers individually
$client->setHeader('X-Custom-Header', 'value');

// Get all current headers
$headers = $client->getHeaders();
```

### Asynchronous Requests

```php
// Start async GET request
$client->getAsync('/users/123');

// Do other work here...

// Wait for the request to complete
$client->wait();
echo $client->getResponseBody();

// Multiple async requests
$client1 = new HttpClient('https://api1.example.com');
$client2 = new HttpClient('https://api2.example.com');

$client1->getAsync('/resource1');
$client2->getAsync('/resource2');

// Wait for both requests
$client1->wait();
$client2->wait();
```

## API Reference

### Constructor

```php
public function __construct(?string $baseUrl = null, ?array $headers = null)
```

- `$baseUrl`: Optional base URL for all requests
- `$headers`: Optional associative array of default headers

### Methods

#### HTTP Methods

```php
public function get(string $path): bool
public function post(string $path, string $data): bool
public function put(string $path, string $data): bool
public function delete(string $path): bool
```

#### Asynchronous Methods

```php
public function getAsync(string $path): bool
public function postAsync(string $path, string $data): bool
public function putAsync(string $path, string $data): bool
public function deleteAsync(string $path): bool
public function wait(): bool
```

#### Header Management

```php
public function setHeader(string $key, string $value): bool
public function getHeaders(): array
```

#### Response Handling

```php
public function getStatusCode(): int
public function getResponseBody(): ?string
```

## Error Handling

The extension returns `false` on failure and sets appropriate error messages that can be retrieved through PHP's error handling mechanisms.

## Thread Safety

The extension is thread-safe and can be used in multi-threaded environments. Each client instance maintains its own state and connection pool.

## Performance Considerations

- Asynchronous requests are processed in separate threads
- Connection pooling is handled by libcurl
- Memory management is optimized for large responses
- Headers are cached at the instance level

## Examples

### Complete Example with Error Handling

```php
try {
    $client = new HttpClient('https://api.example.com', [
        'Content-Type' => 'application/json',
        'Accept' => 'application/json'
    ]);

    // Synchronous request
    if ($client->get('/users')) {
        $statusCode = $client->getStatusCode();
        $response = $client->getResponseBody();
        // Process response...
    }

    // Asynchronous request
    if ($client->getAsync('/posts')) {
        // Do other work...
        $client->wait();
        $response = $client->getResponseBody();
        // Process response...
    }
} catch (Exception $e) {
    // Handle exceptions...
}
```

## Contributing

1. Fork the repository
2. Create your feature branch
3. Write tests for your changes
4. Ensure all tests pass
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

For bugs and feature requests, please use the GitHub issues system.
