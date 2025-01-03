<?php

function measureTime($callback) {
    $start = microtime(true);
    $result = $callback();
    $end = microtime(true);
    return [
        'result' => $result,
        'time' => ($end - $start) * 1000 // milisaniye cinsinden
    ];
}

$client = new HttpClient();

// Senkron istekler
echo "=== Senkron İstekler ===\n";
$syncTime = measureTime(function() use ($client) {
    // İlk istek
    $client->get("https://api.github.com/users/github");
    $response1 = $client->getResponseBody();
    echo "1. İstek Status: " . $client->getStatusCode() . "\n";
    
    // İkinci istek
    $client->get("https://api.github.com/users/google");
    $response2 = $client->getResponseBody();
    echo "2. İstek Status: " . $client->getStatusCode() . "\n";
    
    return [$response1, $response2];
});

echo "Senkron istekler toplam süresi: " . $syncTime['time'] . "ms\n\n";

// Asenkron istekler
echo "=== Asenkron İstekler ===\n";
$asyncTime = measureTime(function() use ($client) {
    // İstekleri başlat
    $client->getAsync("https://api.github.com/users/github");
    echo "1. İstek başlatıldı\n";
    
    // İkinci isteği başlat (ilk istek hala devam ediyor olabilir)
    $newClient = new HttpClient();
    $newClient->getAsync("https://api.github.com/users/google");
    echo "2. İstek başlatıldı\n";
    
    // İsteklerin tamamlanmasını bekle
    $client->wait();
    $response1 = $client->getResponseBody();
    echo "1. İstek Status: " . $client->getStatusCode() . "\n";
    
    $newClient->wait();
    $response2 = $newClient->getResponseBody();
    echo "2. İstek Status: " . $newClient->getStatusCode() . "\n";
    
    return [$response1, $response2];
});

echo "Asenkron istekler toplam süresi: " . $asyncTime['time'] . "ms\n";

// Fark hesaplama
$timeDiff = $syncTime['time'] - $asyncTime['time'];
echo "\nAsenkron işlemler " . $timeDiff . "ms daha hızlı tamamlandı\n";

// PUT ve DELETE örnekleri
echo "\n=== PUT ve DELETE Örnekleri ===\n";

// PUT örneği
$putData = json_encode(['name' => 'test']);
$client->put("https://postman-echo.com/put", $putData);
echo "PUT Status: " . $client->getStatusCode() . "\n";
echo "PUT Response: " . $client->getResponseBody() . "\n";

// DELETE örneği
$client->delete("https://postman-echo.com/delete");
echo "DELETE Status: " . $client->getStatusCode() . "\n";
echo "DELETE Response: " . $client->getResponseBody() . "\n";
