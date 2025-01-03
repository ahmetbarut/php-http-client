<?php

// Create a test server
$host = 'localhost';
$port = 8089;
$pid = pcntl_fork();

if ($pid == 0) {
    // Child process - run test server
    $server = stream_socket_server("tcp://$host:$port", $errno, $errstr);
    if (!$server) {
        exit(1);
    }
    
    for ($i = 0; $i < 5; $i++) {
        $client = @stream_socket_accept($server);
        if ($client) {
            $response = "HTTP/1.1 200 OK\r\n";
            $response .= "Content-Type: application/json\r\n";
            $response .= "\r\n";
            $response .= json_encode(['status' => 'success', 'message' => 'Test response']);
            fwrite($client, $response);
            fclose($client);
        }
    }
    fclose($server);
    exit(0);
}

// Parent process - run tests
sleep(1); // Give server time to start

// Initialize client with test server
$headers = [
    'User-Agent' => 'PHP HTTP Client Test',
    'Accept' => 'application/json'
];

$client = new HttpClient("http://$host:$port", $headers);

// Test 1: GET request
echo "Testing GET request...\n";
$result = $client->get('/test');
$status = $client->getStatusCode();
$body = $client->getResponseBody();
echo "GET Status Code: $status\n";
echo "GET Response: $body\n\n";

// Test 2: Headers
echo "Testing headers...\n";
$client->setHeader('X-Custom-Header', 'test');
$headers = $client->getHeaders();
echo "Current Headers:\n";
print_r($headers);
echo "\n";

// Test 3: POST request
echo "Testing POST request...\n";
$postData = json_encode(['test' => 'data']);
$result = $client->post('/test', $postData);
$status = $client->getStatusCode();
$body = $client->getResponseBody();
echo "POST Status Code: $status\n";
echo "POST Response: $body\n\n";

// Test 4: PUT request
echo "Testing PUT request...\n";
$putData = json_encode(['name' => 'test']);
$result = $client->put('/test', $putData);
$status = $client->getStatusCode();
$body = $client->getResponseBody();
echo "PUT Status Code: $status\n";
echo "PUT Response: $body\n\n";

// Test 5: DELETE request
echo "Testing DELETE request...\n";
$result = $client->delete('/test');
$status = $client->getStatusCode();
$body = $client->getResponseBody();
echo "DELETE Status Code: $status\n";
echo "DELETE Response: $body\n";

// Cleanup
posix_kill($pid, SIGTERM);
pcntl_wait($status);
