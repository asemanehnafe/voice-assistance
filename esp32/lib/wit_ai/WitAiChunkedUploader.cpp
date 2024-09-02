#include "WitAiChunkedUploader.h"
#include "WiFiClientSecure.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

HTTPClient http;
WitAiChunkedUploader::WitAiChunkedUploader(const char *access_key)
{
    // HTTPClient http;  // Create an HTTPClient 

    // // Construct the full URL
    
    String url = "http://192.168.1.38/say-hello/";

    // // Begin the HTTP connection
    http.begin(url);
    http.addHeader("Content-Type", "audio/raw; encoding=signed-integer; bits=16; rate=16000; endian=little");
    http.addHeader("Transfer-Encoding", "chunked");


    // // Set the headers
   //  http.addHeader("Content-Type", "audio/raw; encoding=signed-integer; bits=16; rate=16000; endian=little");  // Set the content-type header

    // // Send the POST request
    int httpResponseCode = http.GET();
    Serial.println(http.getString());
    // // Check the response code


    // // End the HTTP connection
    http.end();
    // m_wifi_client = new WiFiClientSecure();
    // m_wifi_client->setInsecure();
    // m_wifi_client->connect("192.168.112.60", 80);
    // // char authorization_header[100];
    // // snprintf(authorization_header, 100, "authorization: Bearer %s", access_key);
    // m_wifi_client->println("POST /say-hello/ HTTP/1.1");
    // m_wifi_client->println("host: 192.168.112.60");
    // // m_wifi_client->println(authorization_header);
    // m_wifi_client->println("content-type: audio/raw; encoding=signed-integer; bits=16; rate=16000; endian=little");
    // m_wifi_client->println("transfer-encoding: chunked");
    // m_wifi_client->println();
}

bool WitAiChunkedUploader::connected()
{
    return http.getSize() > 0;
}

void WitAiChunkedUploader::startChunk(int size_in_bytes)
{
     http.getStream().print(String(size_in_bytes, HEX) + "\r\n");
}


void WitAiChunkedUploader::sendChunkData(const uint8_t *data, int size_in_bytes)
{
    http.getStream().write(data, size_in_bytes);
}

void WitAiChunkedUploader::finishChunk()
{
    // End the current chunk
    http.getStream().print("\r\n");
}
Intent WitAiChunkedUploader::getResults()
{
    // Finish the chunked request by sending a zero-length chunk
    http.getStream().print("0\r\n\r\n");

    // Get the response and headers
    int status = -1;
    int content_length = 0;

    while (http.connected())
    {
        String line = http.getStream().readStringUntil('\n');
        line.trim();  // Remove any leading or trailing whitespace
        if (line.length() == 0)
        {
            // Blank line indicates the end of headers
            break;
        }
        if (line.startsWith("HTTP"))
        {
            sscanf(line.c_str(), "HTTP/1.1 %d", &status);
        }
        else if (line.startsWith("Content-Length:"))
        {
            sscanf(line.c_str(), "Content-Length: %d", &content_length);
        }
    }

    Serial.printf("HTTP status is %d with content length of %d\n", status, content_length);

    if (status == 200)
    {
        StaticJsonDocument<500> filter;
        filter["entities"]["device:device"][0]["value"] = true;
        filter["entities"]["device:device"][0]["confidence"] = true;
        filter["text"] = true;
        filter["intents"][0]["name"] = true;
        filter["intents"][0]["confidence"] = true;
        filter["traits"]["wit$on_off"][0]["value"] = true;
        filter["traits"]["wit$on_off"][0]["confidence"] = true;

        StaticJsonDocument<500> doc;
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

        const char *text = doc["text"];
        const char *intent_name = doc["intents"][0]["name"];
        float intent_confidence = doc["intents"][0]["confidence"];
        const char *device_name = doc["entities"]["device:device"][0]["value"];
        float device_confidence = doc["entities"]["device:device"][0]["confidence"];
        const char *trait_value = doc["traits"]["wit$on_off"][0]["value"];
        float trait_confidence = doc["traits"]["wit$on_off"][0]["confidence"];

        return Intent{
            .text = (text ? text : ""),
            .intent_name = (intent_name ? intent_name : ""),
            .intent_confidence = intent_confidence,
            .device_name = (device_name ? device_name : ""),
            .device_confidence = device_confidence,
            .trait_value = (trait_value ? trait_value : ""),
            .trait_confidence = trait_confidence};
    }
    return Intent{};
}

WitAiChunkedUploader::~WitAiChunkedUploader()
{
    delete m_wifi_client;
}
