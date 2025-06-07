#include "headers/music.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <boost/beast/core/detail/base64.hpp>
#include <portaudio.h>

constexpr size_t CHUNK_SIZE = 4096; // Reduced chunk size for better flow control
constexpr int SAMPLE_RATE = 44100;
constexpr int CHANNELS = 2;
constexpr int BYTES_PER_SAMPLE = 2;

struct AudioBuffer {
    std::queue<std::vector<short>> chunks;
    std::mutex mutex;
    std::atomic<bool> finished{ false };
    std::atomic<bool> ready_to_play{ false };
    size_t min_buffer_chunks = 3; // Reduced for faster startup
    size_t max_buffer_chunks = 10; // Prevent excessive buffering
};

static AudioBuffer g_audioBuffer;
static PaStream* g_stream = nullptr;
static std::atomic<bool> g_streamInitialized{ false };

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
        std::fill(output, output + framesPerBuffer * CHANNELS, 0);
        return paContinue;
    }

    g_audioBuffer.ready_to_play = true;

    if (!g_audioBuffer.chunks.empty()) {
        auto& chunk = g_audioBuffer.chunks.front();
        size_t samples_to_copy = std::min(static_cast<size_t>(framesPerBuffer * CHANNELS), chunk.size());

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
        if (samples_to_copy < framesPerBuffer * CHANNELS) {
            std::fill(output + samples_to_copy, output + framesPerBuffer * CHANNELS, 0);
        }

        return paContinue;
    }
    else if (g_audioBuffer.finished.load()) {
        return paComplete;
    }
    else {
        // No data available, output silence
        std::fill(output, output + framesPerBuffer * CHANNELS, 0);
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
    size_t chunks_sent = 0;

    while (file.read(buffer.data(), CHUNK_SIZE) || file.gcount() > 0) {
        std::string chunk(buffer.data(), file.gcount());
        std::string encoded = encodeBase64(chunk);

        try {
            sendCallback(encoded);
            chunks_sent++;

            // Calculate proper delay based on audio data rate
            // Each chunk represents this much audio time
            double chunk_duration_ms = static_cast<double>(file.gcount()) /
                (SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE) * 1000.0;

            // Use a more conservative delay to prevent buffer overflow
            int delay_ms = static_cast<int>(chunk_duration_ms * 0.8); // 80% of real-time

            // Minimum delay to prevent overwhelming the WebSocket
            delay_ms = std::max(delay_ms, 20); // At least 20ms between chunks

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

            // Log progress every 100 chunks
            if (chunks_sent % 100 == 0) {
                std::cout << "Sent " << chunks_sent << " audio chunks" << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error sending audio chunk: " << e.what() << std::endl;
            break;
        }
    }

    file.close();

    // Signal that streaming is finished
    g_audioBuffer.finished = true;

    std::cout << "Finished streaming WAV. Total chunks sent: " << chunks_sent << std::endl;
}

void initializeAudio() {
    if (g_streamInitialized.load()) return;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init error: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    err = Pa_OpenDefaultStream(&g_stream,
        0,          // no input channels
        CHANNELS,   // stereo output
        paInt16,    // 16-bit samples
        SAMPLE_RATE, // sample rate
        256,        // frames per buffer (increased for stability)
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
    std::cout << "Audio playback initialized successfully" << std::endl;
}

void cleanupAudio() {
    if (!g_streamInitialized.load()) return;

    // Reset buffer state
    {
        std::lock_guard<std::mutex> lock(g_audioBuffer.mutex);
        while (!g_audioBuffer.chunks.empty()) {
            g_audioBuffer.chunks.pop();
        }
        g_audioBuffer.finished = false;
        g_audioBuffer.ready_to_play = false;
    }

    if (g_stream) {
        Pa_StopStream(g_stream);
        Pa_CloseStream(g_stream);
        g_stream = nullptr;
    }
    Pa_Terminate();
    g_streamInitialized = false;

    std::cout << "Audio cleanup completed" << std::endl;
}

void music::handleAudioChunk(const std::string& base64Data) {
    if (!g_streamInitialized.load()) {
        initializeAudio();
    }

    try {
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

        // Add to playback buffer with overflow protection
        {
            std::lock_guard<std::mutex> lock(g_audioBuffer.mutex);

            // Prevent buffer overflow
            if (g_audioBuffer.chunks.size() >= g_audioBuffer.max_buffer_chunks) {
                // Drop oldest chunk to make room
                if (!g_audioBuffer.chunks.empty()) {
                    g_audioBuffer.chunks.pop();
                }
            }

            g_audioBuffer.chunks.push(std::move(audioSamples));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error handling audio chunk: " << e.what() << std::endl;
    }
}