#pragma once
#include "message-buffer.hpp"
#include "message-dispatcher.hpp"

#include <obs.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct whisper_context;
struct audio_data;

namespace advss {

// Captures audio from one OBS source, resamples to 16 kHz mono, runs
// whisper.cpp inference on a background thread, and dispatches the resulting
// transcript text to registered MessageBuffers.
class SpeechRecognizer {
public:
	SpeechRecognizer();
	~SpeechRecognizer();

	bool LoadModel(const std::string &modelPath);

	bool StartCapture(obs_source_t *source);
	void StopCapture();

	void SetBufferDuration(double seconds);
	void SetNThreads(int n);
	void SetLanguage(const std::string &lang);
	void SetTranslate(bool translate);
	void SetVadEnergyThreshold(float threshold);
	void SetSuppressNonSpeechTokens(bool suppress);
	void SetNoContext(bool noContext);
	void SetListenWhenMuted(bool listen);
	void SetUseGpu(bool useGpu);

	[[nodiscard]] std::shared_ptr<MessageBuffer<std::string>>
	RegisterClient();

private:
	static void AudioCaptureCallback(void *param, obs_source_t *source,
					 const struct audio_data *audio,
					 bool muted);
	void AppendResampledAudio(const struct audio_data *audio);
	void InferenceLoop();

	// Held during whisper_full and when freeing/replacing _ctx.
	std::mutex _ctxMutex;
	whisper_context *_ctx = nullptr;
	// Stored as void* to avoid pulling <media-io/audio-resampler.h> into
	// this header. Cast to audio_resampler_t* in the .cpp.
	void *_resampler = nullptr;

	std::vector<float> _audioBuffer;
	std::mutex _audioMutex;
	double _bufferDurationSeconds = 5.0;
	float _vadEnergyThreshold = 1e-4f;
	size_t _framesSinceLastStep = 0;

	std::vector<float> _inferenceBuffer;
	std::thread _inferenceThread;
	std::atomic_bool _stopThread{false};
	std::condition_variable _inferenceCV;
	std::mutex _inferenceMutex;
	bool _bufferReady = false;
	int _nThreads = 4;
	std::string _language = "auto";
	bool _translate = false;
	bool _suppressNonSpeechTokens = true;
	bool _noContext = true;
	bool _listenWhenMuted = false;
	bool _useGpu = true;

	OBSWeakSource _captureSource;
	int _sourceSampleRate = 44100;
	int _sourceChannelCount = 2;

	MessageDispatcher<std::string> _dispatcher;
};

} // namespace advss
