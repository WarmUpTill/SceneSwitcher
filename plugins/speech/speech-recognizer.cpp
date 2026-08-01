#include "speech-recognizer.hpp"

#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <obs.h>
#include <media-io/audio-resampler.h>
#include <util/platform.h>

#include <whisper.h>

namespace advss {

// Ignore configured log level until after loading is complete to ensure we
// catch the initial whisper configuration logs
static bool ignoreLogFilter = true;

static bool setup()
{
	AddFinishedLoadingStep([]() { ignoreLogFilter = false; });
	return true;
}
const bool _ = setup();

static void whisperLogCallback(ggml_log_level level, const char *text, void *)
{
	if (!text || *text == '\0') {
		return;
	}

	int obsLevel = LOG_INFO;
	if (level == GGML_LOG_LEVEL_WARN) {
		obsLevel = LOG_WARNING;
	} else if (level == GGML_LOG_LEVEL_ERROR) {
		obsLevel = LOG_ERROR;
	}

	std::string msg(text);
	if (!msg.empty() && msg.back() == '\n') {
		msg.pop_back();
	}

	if (msg.empty()) {
		return;
	}

	if (ignoreLogFilter) {
		blog(obsLevel, "[speech] %s", msg.c_str());
	} else {
		vblog(obsLevel, "[speech] %s", msg.c_str());
	}
}

static constexpr int whisperSampleRate = 16000;

// How often to evaluate VAD and potentially trigger inference
static constexpr double stepDurationSeconds = 1.0;

// Audio kept from the previous inference run to provide word-boundary context
// for the next run.
// I guess repeating something is better than potentially missing stuff.
static constexpr double keepDurationSeconds = 0.2;

SpeechRecognizer::SpeechRecognizer()
{
	_inferenceThread = std::thread(&SpeechRecognizer::InferenceLoop, this);
}

SpeechRecognizer::~SpeechRecognizer()
{
	StopCapture();

	{
		std::unique_lock<std::mutex> lock(_inferenceMutex);
		_stopThread = true;
		_bufferReady = true;
	}
	_inferenceCV.notify_one();
	if (_inferenceThread.joinable()) {
		_inferenceThread.join();
	}

	if (_ctx) {
		whisper_free(_ctx);
	}
	if (_resampler) {
		audio_resampler_destroy(
			static_cast<audio_resampler_t *>(_resampler));
	}
}

bool SpeechRecognizer::LoadModel(const std::string &modelPath)
{
	std::lock_guard<std::mutex> lock(_ctxMutex);

	if (_ctx) {
		whisper_free(_ctx);
		_ctx = nullptr;
	}

	whisper_log_set(whisperLogCallback, nullptr);

	whisper_context_params cparams = whisper_context_default_params();
	cparams.use_gpu = _useGpu;
	_ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
	if (!_ctx) {
		blog(LOG_WARNING, "failed to load whisper model: %s",
		     modelPath.c_str());
		return false;
	}
	return true;
}

bool SpeechRecognizer::StartCapture(obs_source_t *source)
{
	StopCapture();

	if (!source) {
		return false;
	}

	const audio_t *audio = obs_get_audio();
	if (!audio) {
		return false;
	}
	const struct audio_output_info *aoi = audio_output_get_info(audio);
	_sourceSampleRate = (int)aoi->samples_per_sec;
	_sourceChannelCount = (int)get_audio_channels(aoi->speakers);

	if (_resampler) {
		audio_resampler_destroy(
			static_cast<audio_resampler_t *>(_resampler));
		_resampler = nullptr;
	}

	struct resample_info srcInfo = {};
	srcInfo.samples_per_sec = (uint32_t)_sourceSampleRate;
	srcInfo.format = AUDIO_FORMAT_FLOAT_PLANAR;
	srcInfo.speakers = aoi->speakers;

	struct resample_info dstInfo = {};
	dstInfo.samples_per_sec = whisperSampleRate;
	dstInfo.format = AUDIO_FORMAT_FLOAT;
	dstInfo.speakers = SPEAKERS_MONO;

	_resampler = audio_resampler_create(&dstInfo, &srcInfo);
	if (!_resampler) {
		blog(LOG_WARNING,
		     "failed to create audio resampler for speech condition");
		return false;
	}

	_captureSource = obs_source_get_weak_source(source);
	obs_source_add_audio_capture_callback(source, AudioCaptureCallback,
					      this);
	return true;
}

void SpeechRecognizer::StopCapture()
{
	OBSSource source = OBSGetStrongRef(_captureSource);
	if (source) {
		obs_source_remove_audio_capture_callback(
			source, AudioCaptureCallback, this);
	}
	_captureSource = OBSWeakSource{};
}

void SpeechRecognizer::SetBufferDuration(double seconds)
{
	std::lock_guard<std::mutex> lock(_audioMutex);
	_bufferDurationSeconds = seconds;
	_audioBuffer.clear();
	_framesSinceLastStep = 0;
}

void SpeechRecognizer::SetNThreads(int n)
{
	std::lock_guard<std::mutex> lock(_inferenceMutex);
	_nThreads = std::max(1, n);
}

void SpeechRecognizer::SetLanguage(const std::string &lang)
{
	std::lock_guard<std::mutex> lock(_inferenceMutex);
	_language = lang.empty() ? "auto" : lang;
}

void SpeechRecognizer::SetTranslate(bool translate)
{
	std::lock_guard<std::mutex> lock(_inferenceMutex);
	_translate = translate;
}

void SpeechRecognizer::SetVadEnergyThreshold(float threshold)
{
	std::lock_guard<std::mutex> lock(_audioMutex);
	_vadEnergyThreshold = threshold;
}

void SpeechRecognizer::SetSuppressNonSpeechTokens(bool suppress)
{
	std::lock_guard<std::mutex> lock(_inferenceMutex);
	_suppressNonSpeechTokens = suppress;
}

void SpeechRecognizer::SetNoContext(bool noContext)
{
	std::lock_guard<std::mutex> lock(_inferenceMutex);
	_noContext = noContext;
}

void SpeechRecognizer::SetListenWhenMuted(bool listen)
{
	_listenWhenMuted = listen;
}

void SpeechRecognizer::SetUseGpu(bool useGpu)
{
	std::lock_guard<std::mutex> lock(_ctxMutex);
	_useGpu = useGpu;
}

std::shared_ptr<MessageBuffer<std::string>> SpeechRecognizer::RegisterClient()
{
	return _dispatcher.RegisterClient();
}

void SpeechRecognizer::AudioCaptureCallback(void *param, obs_source_t *,
					    const struct audio_data *audio,
					    bool muted)
{
	if (!audio || !audio->data[0]) {
		return;
	}
	auto *self = static_cast<SpeechRecognizer *>(param);
	if (muted && !self->_listenWhenMuted) {
		return;
	}
	self->AppendResampledAudio(audio);
}

void SpeechRecognizer::AppendResampledAudio(const struct audio_data *audio)
{
	if (!_resampler) {
		return;
	}

	uint8_t *resampledData[MAX_AV_PLANES] = {};
	uint32_t outFrames = 0;
	uint64_t tsOffset = 0;

	bool ok = audio_resampler_resample(
		static_cast<audio_resampler_t *>(_resampler), resampledData,
		&outFrames, &tsOffset, (const uint8_t *const *)audio->data,
		audio->frames);
	if (!ok || outFrames == 0 || !resampledData[0]) {
		return;
	}

	const float *samples =
		reinterpret_cast<const float *>(resampledData[0]);

	std::unique_lock<std::mutex> lock(_audioMutex);
	_audioBuffer.insert(_audioBuffer.end(), samples, samples + outFrames);
	_framesSinceLastStep += outFrames;

	// Keep the rolling buffer capped at the configured context window.
	const size_t maxFrames =
		(size_t)(_bufferDurationSeconds * whisperSampleRate);
	if (_audioBuffer.size() > maxFrames) {
		_audioBuffer.erase(_audioBuffer.begin(),
				   _audioBuffer.begin() +
					   (_audioBuffer.size() - maxFrames));
	}

	// Only consider triggering inference once per step interval.
	const size_t stepFrames =
		(size_t)(stepDurationSeconds * whisperSampleRate);
	if (_framesSinceLastStep < stepFrames) {
		return;
	}
	_framesSinceLastStep = 0;

	// VAD: measure energy over just the most recent step window so that a
	// short utterance at the end of a longer silent buffer is not diluted.
	const size_t vadWindow = std::min(_audioBuffer.size(), stepFrames);
	const size_t vadStart = _audioBuffer.size() - vadWindow;
	float energy = 0.0f;
	for (size_t i = vadStart; i < _audioBuffer.size(); ++i) {
		energy += _audioBuffer[i] * _audioBuffer[i];
	}
	energy /= (float)vadWindow;
	const bool tooSilent = energy < _vadEnergyThreshold;
	if (tooSilent) {
		return;
	}

	{
		std::unique_lock<std::mutex> infLock(_inferenceMutex);
		if (_bufferReady) {
			return;
		}

		_inferenceBuffer = _audioBuffer;
		_bufferReady = true;
		_inferenceCV.notify_one();
	}

	// Retain a short overlap so the next inference has word-boundary context.
	const size_t keepFrames =
		(size_t)(keepDurationSeconds * whisperSampleRate);
	if (_audioBuffer.size() > keepFrames) {
		_audioBuffer.erase(_audioBuffer.begin(),
				   _audioBuffer.begin() +
					   (_audioBuffer.size() - keepFrames));
	}
}

void SpeechRecognizer::InferenceLoop()
{
	while (true) {
		std::vector<float> buffer;
		int nThreads;
		std::string language;
		bool translate;
		bool suppressNonSpeechTokens;
		bool noContext;

		{
			std::unique_lock<std::mutex> lock(_inferenceMutex);
			_inferenceCV.wait(lock,
					  [this] { return _bufferReady; });
			_bufferReady = false;
			if (_stopThread) {
				break;
			}
			buffer = std::move(_inferenceBuffer);
			nThreads = _nThreads;
			language = _language;
			translate = _translate;
			suppressNonSpeechTokens = _suppressNonSpeechTokens;
			noContext = _noContext;
		}

		if (buffer.empty()) {
			continue;
		}

		std::lock_guard<std::mutex> ctxLock(_ctxMutex);

		if (!_ctx) {
			continue;
		}

		whisper_full_params params =
			whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
		params.print_realtime = false;
		params.print_progress = false;
		params.print_timestamps = false;
		params.print_special = false;
		params.translate = translate;
		params.language = language.c_str();
		params.n_threads = nThreads;
		params.single_segment = false;
		params.suppress_nst = suppressNonSpeechTokens;
		params.no_context = noContext;

		// Limit the encoder to the actual audio length
		params.audio_ctx =
			std::min(1500, (int)((float)buffer.size() /
					     (float)whisperSampleRate * 50.0f));

		int rc = whisper_full(_ctx, params, buffer.data(),
				      (int)buffer.size());
		if (rc != 0) {
			blog(LOG_WARNING, "whisper_full returned %d", rc);
			continue;
		}

		std::string transcript;
		const int nSegments = whisper_full_n_segments(_ctx);
		for (int i = 0; i < nSegments; ++i) {
			const char *text =
				whisper_full_get_segment_text(_ctx, i);
			if (text) {
				transcript += text;
			}
		}

		if (!transcript.empty()) {
			const auto begin =
				transcript.find_first_not_of(" \t\r\n");
			if (begin != std::string::npos) {
				transcript = transcript.substr(begin);
			}
			_dispatcher.DispatchMessage(transcript);
		}
	}
}

} // namespace advss
