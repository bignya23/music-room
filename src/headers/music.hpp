	#pragma once

	#include <string>
	#include <functional>

	namespace music {

		// Stream an MP3 file in chunks, encode to base64, and send chunks via callback
		void streamAudioFile(const std::string& filepath, std::function<void(const std::string&)> sendCallback);

		// Handle received base64-encoded audio chunk and play it
		void handleAudioChunk(const std::string& base64Chunk);

	} 


