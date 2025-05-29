// music.cpp
#include "music.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <boost/beast/core/detail/base64.hpp>
#include <portaudio.h>

constexpr size_t CHUNK_SIZE = 4096;

// Audio buffer management
struct AudioBuffer {
    std::queue<std::vector<short>> chunks;
    std::mutex mutex;
    bool finished = false;
    bool ready_to_play = false;
    size_t min_buffer_chunks = 3; // Buffer at least 3 chunks before starting
};

static AudioBuffer g_audioBuffer;

std::string encodeBase64(const std::string& input) {
    namespace b64 = boost::beast::detail::base64;
    std::string output;
    output.resize(b64::encoded_size(input.size()));
    b64::encode(&output[0], input.data(), input.size());
    return output;
}

// PortAudio callback function
int audioCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {

    short* output = static_cast<short*>(outputBuffer);
    std::lock_guard<std::mutex> lock(g_audioBuffer.mutex);

    // Wait for minimum buffer before starting playback
    if (!g_audioBuffer.ready_to_play && g_audioBuffer.chunks.size() < g_audioBuffer.min_buffer_chunks) {
        std::fill(output, output + framesPerBuffer * 2, 0);
        return paContinue;
    }

    g_audioBuffer.ready_to_play = true;

    if (!g_audioBuffer.chunks.empty()) {
        auto& chunk = g_audioBuffer.chunks.front();
        size_t samples_to_copy = std::min(static_cast<size_t>(framesPerBuffer * 2), chunk.size()); // 2 for stereo

        std::copy(chunk.begin(), chunk.begin() + samples_to_copy, output);

        // If we used the entire chunk, remove it
        if (samples_to_copy == chunk.size()) {
            g_audioBuffer.chunks.pop();
        }
        else {
            // Remove used samples from the beginning
            chunk.erase(chunk.begin(), chunk.begin() + samples_to_copy);
        }

        // Fill remaining buffer with silence if needed
        if (samples_to_copy < framesPerBuffer * 2) {
            std::fill(output + samples_to_copy, output + framesPerBuffer * 2, 0);
        }

        return paContinue;
    }
    else if (g_audioBuffer.finished) {
        return paComplete;
    }
    else {
        // No data available, output silence
        std::fill(output, output + framesPerBuffer * 2, 0);
        return paContinue;
    }
}

void music::streamAudioFile(const std::string& filepath, std::function<void(const std::string&)> sendCallback) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return;
    }

    // Skip WAV header (usually 44 bytes)
    file.seekg(44, std::ios::beg);

    std::vector<char> buffer(CHUNK_SIZE);
    while (file.read(buffer.data(), CHUNK_SIZE) || file.gcount() > 0) {
        std::string chunk(buffer.data(), file.gcount());
        std::string encoded = encodeBase64(chunk);
        sendCallback(encoded);

        // Reduce sleep time for initial buffering
        {
            std::lock_guard<std::mutex> lock(g_audioBuffer.mutex);
            if (g_audioBuffer.chunks.size() < g_audioBuffer.min_buffer_chunks) {
                // Fast initial fill - check buffer size safely
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simple delay for streaming
    }

    file.close();

    // Signal that streaming is finished
    {
        std::lock_guard<std::mutex> lock(g_audioBuffer.mutex);
        g_audioBuffer.finished = true;
    }

    std::cout << "Finished streaming WAV." << std::endl;
}

static PaStream* g_stream = nullptr;
static bool g_streamInitialized = false;

void music::initializeAudio() {
    if (g_streamInitialized) return;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init error: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    err = Pa_OpenDefaultStream(&g_stream,
        0,          // no input channels
        2,          // stereo output
        paInt16,    // 16-bit samples
        44100,      // sample rate
        128,        // smaller frames per buffer for lower latency
        audioCallback,
        nullptr);   // no callback userData

    if (err != paNoError) {
        std::cerr << "PortAudio open stream error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return;
    }

    err = Pa_StartStream(g_stream);
    if (err != paNoError) {
        std::cerr << "PortAudio start stream error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(g_stream);
        Pa_Terminate();
        return;
    }

    g_streamInitialized = true;
    std::cout << "Audio system initialized successfully." << std::endl;
}

void music::cleanupAudio() {
    if (!g_streamInitialized) return;

    // Reset buffer state
    {
        std::lock_guard<std::mutex> lock(g_audioBuffer.mutex);
        while (!g_audioBuffer.chunks.empty()) {
            g_audioBuffer.chunks.pop();
        }
        g_audioBuffer.finished = false;
        g_audioBuffer.ready_to_play = false;
    }

    Pa_StopStream(g_stream);
    Pa_CloseStream(g_stream);
    Pa_Terminate();
    g_streamInitialized = false;
    std::cout << "Audio system cleaned up." << std::endl;
}

void music::handleAudioChunk(const std::string& base64Data) {
    if (!g_streamInitialized) {
        initializeAudio();
    }

    std::string decoded;
    decoded.resize(boost::beast::detail::base64::decoded_size(base64Data.size()));
    auto result = boost::beast::detail::base64::decode(
        &decoded[0],
        base64Data.data(),
        base64Data.size()
    );
    decoded.resize(result.first);

    // Convert to 16-bit samples
    std::vector<short> audioSamples;
    audioSamples.resize(decoded.size() / 2);
    std::memcpy(audioSamples.data(), decoded.data(), decoded.size());

    // Add to playback buffer
    {
        std::lock_guard<std::mutex> lock(g_audioBuffer.mutex);
        g_audioBuffer.chunks.push(std::move(audioSamples));
    }
}