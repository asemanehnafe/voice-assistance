#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include <stdlib.h>  // for malloc and free
#include <Arduino.h>
#include "I2SMicSampler.h"
#include "AudioProcessor.h"
#include "NeuralNetwork.h"
#include "RingBuffer.h"


// Wi-Fi credentials
const char* ssid = "Erfan";
const char* password = "erfan12345";

// Django server URL
const char* serverUrl = "http://192.168.112.60/say-hello/";

// I2S configuration
#define I2S_WS  25   // GPIO 25 (WS)
#define I2S_SD  33   // GPIO 33 (SD)
#define I2S_SCK 32   // GPIO 32 (SCK)
#define SAMPLE_RATE 44100
#define I2S_BUFFER_SIZE 1024  // Adjust based on your needs
#define MAX_RECORDING_SIZE 100000  // Maximum size of the recorded data before sending (100KB in this example)

#define WINDOW_SIZE 320
#define STEP_SIZE 160
#define POOLING_SIZE 6
#define AUDIO_LENGTH 16000

const int lightPin = 5;
const int lightpin2= 21;
int marvin_detected= 0;

i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = 20, // We don't need data out
    .data_in_num = I2S_SD
  };

i2s_config_t i2s_config =  {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 10,
    .dma_buf_len = I2S_BUFFER_SIZE,
    .use_apll = false
  };

i2s_config_t i2sMemsConfigBothChannels ={
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    //.sample_rate = 16000,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 10,
    .dma_buf_len = 1024,
    .use_apll = false
  };

float getNeuralNetworkOutput(I2SSampler *sample_provider) {
    NeuralNetwork *m_nn = new NeuralNetwork();
    Serial.println("Created Neural Net");

    AudioProcessor *audio = new AudioProcessor(AUDIO_LENGTH, WINDOW_SIZE, STEP_SIZE, POOLING_SIZE);
    Serial.println("Created audio processor");

    RingBufferAccessor *reader = sample_provider->getRingBufferReader();
    reader->rewind(16000);

    float *input_buffer = m_nn->getInputBuffer();
    audio->get_spectrogram(reader, input_buffer);
    
    delete reader;
    float output = m_nn->predict();
    Serial.println("Neural Network Output:");
    Serial.println(output);

    delete m_nn;
    m_nn = NULL;
    delete audio;
    audio = NULL;

    uint32_t free_ram = esp_get_free_heap_size();
    Serial.printf("Free RAM: %u bytes\n", free_ram);
    Serial.println(output);
    return output;
}


void captureAndSendAudio() {
  uint8_t* i2s_buffer = (uint8_t*)malloc(I2S_BUFFER_SIZE);
  uint8_t* recording_buffer = (uint8_t*)malloc(MAX_RECORDING_SIZE);
  size_t recording_size = 0;

  if (i2s_buffer == NULL || recording_buffer == NULL) {
    Serial.println("Failed to allocate memory for buffers.");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "audio/raw");

  while (true) {
    size_t bytes_read;
    i2s_read(I2S_NUM_0, i2s_buffer, I2S_BUFFER_SIZE, &bytes_read, portMAX_DELAY);

    if (bytes_read > 0) {
      if (recording_size + bytes_read <= MAX_RECORDING_SIZE) {
        memcpy(recording_buffer + recording_size, i2s_buffer, bytes_read);
        recording_size += bytes_read;
      } else {
        int httpResponseCode = http.POST(recording_buffer, recording_size);
        
        if (httpResponseCode > 0) {
        String response = http.getString();
              
        if(marvin_detected==1)
        {
          if (response=="kitchen,off") {
            Serial.println("Turning off the kitchen lights");
            digitalWrite(lightPin, LOW);
          }

          else if (response=="kitchen,on") {
              Serial.println("Turning on the kitchen lights");
              digitalWrite(lightPin, HIGH);
            }
            else if (response=="bedroom,on") {
              Serial.println("Turning on the bedroom lights");
              digitalWrite(lightpin2, HIGH);
            }
            else if (response=="bedroom,off") {
              Serial.println("Turning off the bedroom lights");
              digitalWrite(lightpin2, LOW);
            } 
          marvin_detected= 0;
        }
        else
        {
          if(response=="marvin,on")
          {
            marvin_detected= 1;
            Serial.println("marvin detected");
          }
        }
        } 
        
        
        else {
          Serial.printf("Error sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        recording_size = 0;
        delay(5000);
      }
    }
  }

  http.end();
  free(i2s_buffer);
  free(recording_buffer);
}

void setup() {
  Serial.begin(115200);
  pinMode(lightPin, OUTPUT);
  pinMode(lightpin2, OUTPUT);

  digitalWrite(lightPin, LOW);
  digitalWrite(lightpin2, LOW); 

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void loop() {
  // I2SSampler *i2s_sampler = new I2SMicSampler(pin_config, false);
  // float nn_output = getNeuralNetworkOutput(i2s_sampler);
  captureAndSendAudio();
  // if (nn_output >) {
    // Serial.println("Neural network output exceeds threshold. Capturing and sending audio...");
    // captureAndSendAudio();
  // } else {
  //   Serial.println("Neural network output below threshold. No action taken.");
  // }

  delay(5000);  // Delay before checking the neural network output again
}
