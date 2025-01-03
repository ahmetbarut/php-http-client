<?php

// Base URL ve header'lar ile başlatma
$headers = [
    'User-Agent' => 'PHP HTTP Client',
    'Accept' => 'application/json'
];

$client = new HttpClient('https://api.github.com', $headers);

// GET request test
$result = $client->get('/users/github');
echo "GET Status Code: " . $client->getStatusCode() . "\n";
echo "GET Response: " . $client->getResponseBody() . "\n\n";

// Yeni header ekleme
$client->setHeader('X-Custom-Header', 'test');
echo "Current Headers:\n";
print_r($client->getHeaders());

// POST request test with base URL
$postData = json_encode(['test' => 'data']);
$result = $client->post('/user', $postData);
echo "\nPOST Status Code: " . $client->getStatusCode() . "\n";
echo "POST Response: " . $client->getResponseBody() . "\n\n";

// Farklı bir base URL ile yeni client
$postmanClient = new HttpClient('https://postman-echo.com');
$postmanClient->setHeader('Content-Type', 'application/json');

// PUT test
$putData = json_encode(['name' => 'test']);
$result = $postmanClient->put('/put', $putData);
echo "PUT Status Code: " . $postmanClient->getStatusCode() . "\n";
echo "PUT Response: " . $postmanClient->getResponseBody() . "\n\n";

// DELETE test
$result = $postmanClient->delete('/delete');
echo "DELETE Status Code: " . $postmanClient->getStatusCode() . "\n";
echo "DELETE Response: " . $postmanClient->getResponseBody() . "\n";
