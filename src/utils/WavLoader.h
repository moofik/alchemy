#pragma once
#include <string>
#include "devices/SamplerNode.h"

// Загружает WAV в наш non-interleaved SampleBufferPtr (float32). nullptr при ошибке.
SampleBufferPtr LoadWavToSampleBuffer(const std::string& path, bool forceStereo);