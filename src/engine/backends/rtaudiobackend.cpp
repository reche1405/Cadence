#include "rtaudiobackend.h"
#include "../common/audioerror.h"
#include <cstring>
#include <thread>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace AudioEngine {
RtAudioBackend::RtAudioBackend(BackendType backendType)
    : m_backendType(backendType)
{
    try {
        RtAudio::Api api = convertToRtAudioApi(backendType);
        m_rtAudio = std::make_unique<RtAudio>(api);

        // If auto or invalid, let RtAudio choose
        if (backendType == BackendType::Auto || !m_rtAudio) {
            m_rtAudio = std::make_unique<RtAudio>();
            m_backendType = convertRtAudioApi(m_rtAudio->getCurrentApi());
        }
    } catch (const RtAudioError& e) {
        setError(std::string("Failed to initialize RtAudio: ") + e.getMessage());
        throw AudioException(AudioErrorCode::AudioBackendInitFailed, getLastError());
    }
}

RtAudioBackend::~RtAudioBackend() {
    try {
        if (isRunning()) {
            stop();
        }
    } catch (...) {
        // Destructor shouldn't throw
    }
}

void RtAudioBackend::initialize(const StreamConfig& config) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    if (!config.isValid()) {
        throw AudioException(AudioErrorCode::InvalidConfiguration,
                           "Invalid stream configuration");
    }

    m_config = config;

    // If backend type is Auto in config, use what we have
    if (config.preferredBackend != BackendType::Auto &&
        config.preferredBackend != m_backendType) {
        // Need to recreate RtAudio instance with different API
        m_backendType = config.preferredBackend;
        RtAudio::Api api = convertToRtAudioApi(m_backendType);
        m_rtAudio = std::make_unique<RtAudio>(api);
    }

    // Clear any previous errors
    clearError();
}

void RtAudioBackend::start(AudioCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    if (isRunning()) {
        throw AudioException(AudioErrorCode::AudioBackendStartFailed,
                           "Backend is already running");
    }

    if (!callback) {
        throw AudioException(AudioErrorCode::AudioBackendStartFailed,
                           "Invalid callback function");
    }

    m_userCallback = std::move(callback);

    try {
        // Configure RtAudio stream parameters
        RtAudio::StreamParameters inputParams;
        RtAudio::StreamParameters outputParams;

        // Input device
        if (m_config.inputChannels > 0 && m_config.inputDeviceName.has_value()) {
            inputParams.deviceId = findDeviceIdByName(
                m_config.inputDeviceName.value(), true);
            inputParams.nChannels = m_config.inputChannels;
            inputParams.firstChannel = 0;
        }

        // Output device
        if (m_config.outputChannels > 0 && m_config.outputDeviceName.has_value()) {
            outputParams.deviceId = findDeviceIdByName(
                m_config.outputDeviceName.value(), false);
            outputParams.nChannels = m_config.outputChannels;
            outputParams.firstChannel = 0;
        }

        // Get default devices if not specified
        if (inputParams.nChannels > 0 && inputParams.deviceId == -1) {
            inputParams.deviceId = m_rtAudio->getDefaultInputDevice();
        }
        if (outputParams.nChannels > 0 && outputParams.deviceId == -1) {
            outputParams.deviceId = m_rtAudio->getDefaultOutputDevice();
        }

        // Buffer size
        unsigned int bufferFrames = m_config.bufferSize;

        // Sample rate
        unsigned int sampleRate = m_config.sampleRate;

        // Format
        RtAudioFormat format = RtAudioDevice::convertToRtAudioFormat(m_config.format);

        // Open the stream
        RtAudio::StreamOptions options;
        options.flags = RTAUDIO_NONINTERLEAVED;
        options.numberOfBuffers = 2; // Double buffering
        options.streamName = "Cadence DAW";
        options.priority = 90; // High priority for audio thread

        if (m_config.exclusiveMode) {
            options.flags |= RTAUDIO_HOG_DEVICE;
        }

        m_rtAudio->openStream(
            m_config.outputChannels > 0 ? &outputParams : nullptr,
            m_config.inputChannels > 0 ? &inputParams : nullptr,
            format,
            sampleRate,
            &bufferFrames,
            &RtAudioBackend::rtAudioCallback,
            this,
            &options
        );

        // Update actual buffer size (RtAudio may adjust it)
        m_config.bufferSize = bufferFrames;

        // Reset performance counters
        resetPerformanceCounters();

        // Start the stream
        m_rtAudio->startStream();
        m_isRunning = true;
        m_isPaused = false;

        m_lastCallbackTime = std::chrono::high_resolution_clock::now();

    } catch (const RtAudioError& e) {
        setError(std::string("Failed to start audio stream: ") + e.getMessage());
        m_isRunning = false;
        throw AudioException(AudioErrorCode::AudioBackendStartFailed, getLastError());
    }
}

void RtAudioBackend::stop() {
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    if (!isRunning()) {
        return;
    }

    try {
        m_rtAudio->stopStream();
        m_rtAudio->closeStream();
    } catch (const RtAudioError& e) {
        setError(std::string("Error stopping stream: ") + e.getMessage());
        // Continue anyway
    }

    m_isRunning = false;
    m_isPaused = false;
    m_userCallback = nullptr;
}

void RtAudioBackend::pause() {
    if (!isRunning() || isPaused()) {
        return;
    }

    try {
        m_rtAudio->stopStream();
        m_isPaused = true;
    } catch (const RtAudioError& e) {
        setError(std::string("Error pausing stream: ") + e.getMessage());
        throw AudioException(AudioErrorCode::AudioBackendStopFailed, getLastError());
    }
}

void RtAudioBackend::resume() {
    if (!isRunning() || !isPaused()) {
        return;
    }

    try {
        m_rtAudio->startStream();
        m_isPaused = false;
        m_lastCallbackTime = std::chrono::high_resolution_clock::now();
    } catch (const RtAudioError& e) {
        setError(std::string("Error resuming stream: ") + e.getMessage());
        throw AudioException(AudioErrorCode::AudioBackendStartFailed, getLastError());
    }
}

bool RtAudioBackend::isRunning() const {
    return m_isRunning.load();
}

bool RtAudioBackend::isPaused() const {
    return m_isPaused.load();
}

StreamConfig RtAudioBackend::getCurrentConfig() const {
    return m_config;
}

int RtAudioBackend::getActualSampleRate() const {
    if (!m_rtAudio || !isRunning()) {
        return m_config.sampleRate;
    }

    try {
        return static_cast<int>(m_rtAudio->getStreamSampleRate());
    } catch (...) {
        return m_config.sampleRate;
    }
}

int RtAudioBackend::getActualBufferSize() const {
    if (!m_rtAudio || !isRunning()) {
        return m_config.bufferSize;
    }

        return m_config.bufferSize;

}

double RtAudioBackend::getInputLatencyMs() const {
    if (!m_rtAudio || !isRunning()) {
        return 0.0;
    }

    try {
        return m_rtAudio->getStreamLatency() * 1000.0;
    } catch (...) {
        return (m_config.bufferSize * 1000.0) / m_config.sampleRate;
    }
}

double RtAudioBackend::getOutputLatencyMs() const {
    // Same as input latency in duplex streams
    return getInputLatencyMs();
}

double RtAudioBackend::getStreamTime() const {
    return m_streamTime.load();
}

void RtAudioBackend::updateStreamTime(unsigned int framesProcessed) {
    double increment = framesProcessed / static_cast<double>(m_config.sampleRate);
    m_streamTime.store(m_streamTime.load() + increment);
}

bool RtAudioBackend::changeSampleRate(int newRate) {
    if (!isRunning() || !m_config.allowSampleRateChange) {
        return false;
    }

    try {
        // Stop, change config, and restart
        bool wasPaused = isPaused();

        if (!wasPaused) {
            pause();
        }

        m_rtAudio->closeStream();
        m_config.sampleRate = newRate;

        // Reopen stream with new sample rate
        RtAudio::StreamParameters inputParams, outputParams;

        // Configure parameters (similar to start())


        // Buffer size
        unsigned int bufferFrames = m_config.bufferSize;


        // Sample rate
        unsigned int sampleRate = m_config.sampleRate;

        // Format
        RtAudioFormat format = RtAudioDevice::convertToRtAudioFormat(m_config.format);

        // Open the stream
        RtAudio::StreamOptions options;
        options.flags = RTAUDIO_NONINTERLEAVED;
        options.numberOfBuffers = 2; // Double buffering
        options.streamName = "Cadence DAW";
        options.priority = 90; // High priority for audio thread

        if (m_config.exclusiveMode) {
            options.flags |= RTAUDIO_HOG_DEVICE;
        }

        m_rtAudio->openStream(
            m_config.outputChannels > 0 ? &outputParams : nullptr,
            m_config.inputChannels > 0 ? &inputParams : nullptr,
            format,
            sampleRate,
            &bufferFrames,
            &RtAudioBackend::rtAudioCallback,
            this,
            &options
            );


        if (!wasPaused) {
            resume();
        }

        return true;

    } catch (const RtAudioError& e) {
        setError(std::string("Failed to change sample rate: ") + e.getMessage());
        return false;
    }
}

bool RtAudioBackend::changeBufferSize(int newSize) {
    if (!isRunning() || !m_config.allowBufferSizeChange) {
        return false;
    }

    // RtAudio doesn't support dynamic buffer size changes
    // Need to stop and restart the stream
    try {
        bool wasPaused = isPaused();

        if (!wasPaused) {
            pause();
        }

        m_rtAudio->closeStream();
        m_config.bufferSize = newSize;

        // Reopen stream with new buffer size
        RtAudio::StreamParameters inputParams, outputParams;

        // Configure parameters (similar to start())

        // Buffer size
        unsigned int bufferFrames = newSize;



        // Sample rate
        unsigned int sampleRate = m_config.sampleRate;

        // Format
        RtAudioFormat format = RtAudioDevice::convertToRtAudioFormat(m_config.format);

        // Open the stream
        RtAudio::StreamOptions options;
        options.flags = RTAUDIO_NONINTERLEAVED;
        options.numberOfBuffers = 2; // Double buffering
        options.streamName = "Cadence DAW";
        options.priority = 90; // High priority for audio thread

        if (m_config.exclusiveMode) {
            options.flags |= RTAUDIO_HOG_DEVICE;
        }

        m_rtAudio->openStream(
            m_config.outputChannels > 0 ? &outputParams : nullptr,
            m_config.inputChannels > 0 ? &inputParams : nullptr,
            format,
            sampleRate,
            &bufferFrames,
            &RtAudioBackend::rtAudioCallback,
            this,
            &options
        );

        if (!wasPaused) {
            resume();
        }

        return true;

    } catch (const RtAudioError& e) {
        setError(std::string("Failed to change buffer size: ") + e.getMessage());
        return false;
    }

}

bool RtAudioBackend::switchInputDevice(const std::string& deviceId) {
    // Implementation would need to:
    // 1. Parse deviceId to get RtAudio device index
    // 2. Stop stream
    // 3. Reopen with new device
    // 4. Restart stream

    // For now, return false (not implemented)
    setError("Device switching not implemented");
    return false;
}

bool RtAudioBackend::switchOutputDevice(const std::string& deviceId) {
    // Similar to switchInputDevice
    setError("Device switching not implemented");
    return false;
}


int RtAudioBackend::rtAudioCallback(void* outputBuffer, void* inputBuffer,
                                    unsigned int nFrames,
                                    double streamTime,
                                    RtAudioStreamStatus status,
                                    void* userData) {
    RtAudioBackend* instance = static_cast<RtAudioBackend*>(userData);
    return instance->handleAudioCallback(outputBuffer, inputBuffer,
                                         nFrames, streamTime, status);
}

int RtAudioBackend::handleAudioCallback(void* outputBuffer, void* inputBuffer,
                                        unsigned int nFrames,
                                        double streamTime,
                                        RtAudioStreamStatus status) {
    // Handle xruns (buffer over/under runs)
    if (status & RTAUDIO_INPUT_OVERFLOW) {
        m_xrunCount++;
    }
    if (status & RTAUDIO_OUTPUT_UNDERFLOW) {
        m_xrunCount++;
    }

    // Update CPU usage measurement
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_lastCallbackTime).count();
    double expectedTime = nFrames / static_cast<double>(m_config.sampleRate);

    if (elapsed > expectedTime) {
        // We're running late
        m_cpuUsage.store(100.0); // 100% CPU usage (we missed our deadline)
    } else {
        m_cpuUsage.store((elapsed / expectedTime) * 100.0);
    }
    m_lastCallbackTime = now;

    // Update stream time
    updateStreamTime(nFrames);

    // Call user callback if we have one
    if (m_userCallback) {
        try {
            // Convert buffers to float* (assuming non-interleaved)
            // Note: RtAudio callback uses non-interleaved format when RTAUDIO_NONINTERLEAVED flag is set
            float** inputChannels = static_cast<float**>(inputBuffer);
            float** outputChannels = static_cast<float**>(outputBuffer);

            // For simplicity, we'll handle interleaved format for now
            // In production, you'd want to properly handle both formats
            m_userCallback(static_cast<float*>(inputBuffer),
                           static_cast<float*>(outputBuffer),
                           nFrames,
                           m_streamTime.load());
        } catch (const std::exception& e) {
            setError(std::string("Audio callback error: ") + e.what());
            return 1; // Return non-zero to stop stream
        } catch (...) {
            setError("Unknown error in audio callback");
            return 1;
        }
    }

    // Clear output buffer if no callback
    else if (outputBuffer && m_config.outputChannels > 0) {
        size_t bufferSize = nFrames * m_config.outputChannels * sizeof(float);
        std::memset(outputBuffer, 0, bufferSize);
    }

    return 0;
}

LatencyInfo RtAudioBackend::measureLatency() {
    LatencyInfo info;

    // Theoretical latency based on buffer size
    info.theoreticalMs = (m_config.bufferSize * 1000.0) / m_config.sampleRate;

    // Measured latency (simplified - would need loopback test)
    // For now, use theoretical with some jitter simulation
    info.measuredMs = info.theoreticalMs * (1.0 + (std::rand() % 100) / 1000.0);
    info.jitterMs = info.theoreticalMs * 0.05; // 5% jitter

    // CPU usage
    info.cpuUsage = m_cpuUsage.load();

    // Xrun count
    info.xruns = m_xrunCount.load();

    return info;
}

double RtAudioBackend::getCpuUsage() const {
    return m_cpuUsage.load();
}

int RtAudioBackend::getXrunCount() const {
    return m_xrunCount.load();
}


std::vector<std::unique_ptr<IAudioDevice>> RtAudioBackend::enumerateDevices() const {
    std::vector<std::unique_ptr<IAudioDevice>> devices;

    if (!m_rtAudio) {
        return devices;
    }

    try {
        unsigned int deviceCount = m_rtAudio->getDeviceCount();
        int defaultInput = m_rtAudio->getDefaultInputDevice();
        int defaultOutput = m_rtAudio->getDefaultOutputDevice();

        for (unsigned int i = 0; i < deviceCount; i++) {
            try {
                RtAudio::DeviceInfo info = m_rtAudio->getDeviceInfo(i);

                bool isDefaultInput = (static_cast<int>(i) == defaultInput);
                bool isDefaultOutput = (static_cast<int>(i) == defaultOutput);

                devices.push_back(std::make_unique<RtAudioDevice>(
                    static_cast<int>(i),
                    info,
                    m_backendType,
                    isDefaultInput,
                    isDefaultOutput
                    ));
            } catch (const RtAudioError& e) {
                // Skip devices that can't be queried
                continue;
            }
        }
    } catch (const RtAudioError& e) {
        setError(e.getMessage());
    }

    return devices;
}

std::unique_ptr<IAudioDevice> RtAudioBackend::getCurrentInputDevice() const {
    if (!m_rtAudio || !isRunning()) {
        return nullptr;
    }

    try {
        int deviceId = 1;
        return createDeviceFromRtAudioId(deviceId);
    } catch (...) {
        return nullptr;
    }
}

std::unique_ptr<IAudioDevice> RtAudioBackend::getCurrentOutputDevice() const {
    if (!m_rtAudio || !isRunning()) {
        return nullptr;
    }

    try {
        int deviceId = 2;
        return createDeviceFromRtAudioId(deviceId);
    } catch (...) {
        return nullptr;
    }
}

std::unique_ptr<IAudioDevice> RtAudioBackend::createDeviceFromRtAudioId(int deviceId) const {
    if (deviceId < 0 || !m_rtAudio) {
        return nullptr;
    }

    try {
        RtAudio::DeviceInfo info = m_rtAudio->getDeviceInfo(deviceId);
        int defaultInput = m_rtAudio->getDefaultInputDevice();
        int defaultOutput = m_rtAudio->getDefaultOutputDevice();

        return std::make_unique<RtAudioDevice>(
            deviceId,
            info,
            m_backendType,
            deviceId == defaultInput,
            deviceId == defaultOutput
            );
    } catch (const RtAudioError& e) {
        return nullptr;
    }
}

int RtAudioBackend::findDeviceIdByName(const std::string& name, bool isInput) const {
    auto devices = enumerateDevices();

    for (const auto& device : devices) {
        if (device->getName() == name) {
            const RtAudioDevice* rtDevice =
                dynamic_cast<const RtAudioDevice*>(device.get());
            if (rtDevice) {
                if ((isInput && rtDevice->getRtDeviceInfo().inputChannels > 0) ||
                    (!isInput && rtDevice->getRtDeviceInfo().outputChannels > 0)) {
                    return rtDevice->getRtAudioDeviceId();
                }
            }
        }
    }

    return -1; // Not found
}

BackendType RtAudioBackend::convertRtAudioApi(RtAudio::Api api) {
    switch (api) {
    case RtAudio::WINDOWS_ASIO:    return BackendType::ASIO;
    case RtAudio::WINDOWS_WASAPI:  return BackendType::WASAPI;
    case RtAudio::WINDOWS_DS:      return BackendType::DirectSound;
    case RtAudio::MACOSX_CORE:     return BackendType::CoreAudio;
    case RtAudio::LINUX_ALSA:      return BackendType::ALSA;
    case RtAudio::LINUX_PULSE:     return BackendType::Pulse;
    case RtAudio::UNIX_JACK:       return BackendType::JACK;
    case RtAudio::RTAUDIO_DUMMY:   return BackendType::RtAudio;
    default:                       return BackendType::RtAudio;
    }
}

RtAudio::Api RtAudioBackend::convertToRtAudioApi(BackendType backendType) {
    switch (backendType) {
    case BackendType::ASIO:        return RtAudio::WINDOWS_ASIO;
    case BackendType::WASAPI:      return RtAudio::WINDOWS_WASAPI;
    case BackendType::DirectSound: return RtAudio::WINDOWS_DS;
    case BackendType::CoreAudio:   return RtAudio::MACOSX_CORE;
    case BackendType::ALSA:        return RtAudio::LINUX_ALSA;
    case BackendType::Pulse:       return RtAudio::LINUX_PULSE;
    case BackendType::JACK:        return RtAudio::UNIX_JACK;
    case BackendType::RtAudio:     return RtAudio::UNSPECIFIED;
    default:                       return RtAudio::UNSPECIFIED;
    }
}

BackendType RtAudioBackend::getBackendType() const {
    return m_backendType;
}

void* RtAudioBackend::getPlatformHandle() const {
    // RtAudio doesn't expose its platform handle easily
    return nullptr;
}

std::string RtAudioBackend::getLastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

void RtAudioBackend::clearError() {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError.clear();
}

void RtAudioBackend::setError(const std::string& error) const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = error;
}

void RtAudioBackend::checkAndThrowRtAudioError(const std::string& context) const {
    std::string error = getLastError();
    if (!error.empty()) {
        throw AudioException(AudioErrorCode::PlatformSpecificError,
                             context + ": " + error);
    }
}

void RtAudioBackend::resetPerformanceCounters() {
    m_xrunCount = 0;
    m_cpuUsage = 0.0;
    m_streamTime = 0.0;
}

}
