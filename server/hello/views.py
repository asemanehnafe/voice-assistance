import json
import wave
from django.http import HttpResponse, JsonResponse
import requests
from django.views.decorators.csrf import csrf_exempt

# Define constants
SAMPLE_RATE = 44100
SAMPLE_WIDTH = 2  # 16-bit audio
NUM_CHANNELS = 1
THREE_SECONDS_BYTES = SAMPLE_RATE * SAMPLE_WIDTH * NUM_CHANNELS * 3  # 3 seconds of audio
API_ENDPOINT = 'https://api.wit.ai/speech'
wit_access_token = 'ZLRIM63MCC2GMWXLJZFDCSSQMOPWFJAP'


@csrf_exempt
def recognize_speech(request):
    if request.method == 'POST':
        print(request.headers)
        raw_audio_data = request.body
        if len(raw_audio_data) > THREE_SECONDS_BYTES:
            raw_audio_data = raw_audio_data[:THREE_SECONDS_BYTES]

        # Define the path where the .wav file will be saved
        wav_file_path = 'received_audio.wav'

        # Open a .wav file for writing
        with wave.open(wav_file_path, 'wb') as wav_file:
            wav_file.setnchannels(NUM_CHANNELS)
            wav_file.setsampwidth(SAMPLE_WIDTH)
            wav_file.setframerate(SAMPLE_RATE)
            wav_file.writeframes(raw_audio_data)

        print(f'Audio data saved to {wav_file_path}')

        with open(wav_file_path, 'rb') as f:
            audio = f.read()

        headers = {
            'authorization': 'Bearer ' + wit_access_token,
            'Content-Type': 'audio/wav',  # Specify raw audio content type
        }

        # Make an HTTP POST request to Wit.ai
        resp = requests.post(API_ENDPOINT, headers=headers, data=audio)

        response_text = resp.content.decode('utf-8')
        json_objects = response_text.strip().split('\r\n')

        parsed_objects = []
        for json_str in json_objects:
            if json_str:
                try:
                    data = json.loads(json_str)
                    parsed_objects.append(data)
                except json.JSONDecodeError as e:
                    return JsonResponse({'error': f"JSONDecodeError: {e} - Failed to parse: {json_str}"}, status=400)

        # Return the first FINAL_UNDERSTANDING object found
        for obj in parsed_objects:
            if 'type' in obj and obj['type'] == 'FINAL_UNDERSTANDING':
                value = str(obj['traits']['wit$on_off'][0]['value'])
                device = str(obj['entities']['device:device'][0]['value'])
                print(device)
                print(value)
                return HttpResponse(device + ',' + value)

        return JsonResponse({'error': 'No FINAL_UNDERSTANDING found'}, status=400)

    return JsonResponse({'error': 'Invalid request'}, status=400)

