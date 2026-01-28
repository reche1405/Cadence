#ifndef RTAUDIOBACKEND_H
#define RTAUDIOBACKEND_H
#include "../common/audiobackend.h"
#include "../devices/rtaudiodevice.h"
#include <RtAudio.h>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace AudioEngine {
class RtAudioBackend : public IAudioBackend {
public:
    explicit RtAudioBackend(BackendType backendType = BackendType::RtAudio);
    ~RtAudioBackend() override;

    // Core Audio Operations
    void initialize(const StreamConfig& config) override;
    void start(AudioCallback callback) override;
    void stop() override;

    // Stream Control
    void pause() override;
    void resume() override;

    // Check state
    bool isRunning() const override;
    bool isPaused() const override;

    // Stream Information
    StreamConfig getCurrentConfig() const override;
    int getActualSampleRate() const override;
    int getActualBufferSize() const override;
    double getInputLatencyMs() const override;
    double getOutputLatencyMs() const override;
    double getStreamTime() const override;

    // Dynamic Configuration
    bool changeSampleRate(int newRate) override;
    bool changeBufferSize(int newSize) override;
    bool switchInputDevice(const std::string& deviceId) override;
    bool switchOutputDevice(const std::string& deviceId) override;

    // Performance Monitoring
    LatencyInfo measureLatency() override;
    double getCpuUsage() const override;
    int getXrunCount() const override;

    // Error Handling
    std::string getLastError() const override;
    void clearError() override;

    // Device Management
    std::vector<std::unique_ptr<IAudioDevice>> enumerateDevices() const override;
    std::unique_ptr<IAudioDevice> getCurrentInputDevice() const override;
    std::unique_ptr<IAudioDevice> getCurrentOutputDevice() const override;

    // Platform Specific
    BackendType getBackendType() const override;
    void* getPlatformHandle() const override;

    // Static helper to convert between RtAudio API type and BackendType
    static BackendType convertRtAudioApi(RtAudio::Api api);
    static RtAudio::Api convertToRtAudioApi(BackendType backendType);

private:
    // RtAudio callback (static method that routes to instance)
    static int rtAudioCallback(void* outputBuffer, void* inputBuffer,
                               unsigned int nFrames,
                               double streamTime,
                               RtAudioStreamStatus status,
                               void* userData);

    // Instance callback handler
    int handleAudioCallback(void* outputBuffer, void* inputBuffer,
                            unsigned int nFrames,
                            double streamTime,
                            RtAudioStreamStatus status);

    // Device management
    std::unique_ptr<IAudioDevice> createDeviceFromRtAudioId(int deviceId) const;
    int findDeviceIdByName(const std::string& name, bool isInput) const;

    // Error handling
    void setError(const std::string& error) const;
    void checkAndThrowRtAudioError(const std::string& context) const;

    // State management
    void updateStreamTime(unsigned int framesProcessed);
    void resetPerformanceCounters();

private:
    std::unique_ptr<RtAudio> m_rtAudio;
    BackendType m_backendType;
    StreamConfig m_config;

    // Callback and state
    AudioCallback m_userCallback;
    std::atomic<bool> m_isRunning{false};
    std::atomic<bool> m_isPaused{false};
    std::atomic<int> m_xrunCount{0};

    // Timing and performance
    std::atomic<double> m_streamTime{0.0};
    std::chrono::high_resolution_clock::time_point m_lastCallbackTime;
    std::atomic<double> m_cpuUsage{0.0};
    mutable std::mutex m_callbackMutex;

    // Error handling
    mutable std::mutex m_errorMutex;
    mutable std::string m_lastError;

    // Latency measurement
    std::vector<float> m_latencyTestBuffer;
    std::atomic<bool> m_measuringLatency{false};
};

}
#endif // RTAUDIOBACKEND_H
