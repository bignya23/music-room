	#pragma once

	#include <string>
	#include <functional>

	namespace music {

		void streamAudioFile(const std::string& filepath, std::function<void(const std::string&)> sendCallback);

		// Handle received base64-encoded audio chunk and play it
		void handleAudioChunk(const std::string& base64Chunk);

	} 


