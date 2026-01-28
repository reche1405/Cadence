#ifndef AUDIOBACKEND_H
#define AUDIOBACKEND_H

#include "audiodevice.h"
#include "audioconfig.h"
#include <functional>
#include <memory>
#include <vector>

namespace AudioEngine {

// Audio callback function type
// Args: inputBuffer, outputBuffer, framesPerBuffer, streamTime (seconds)
using AudioCallback = std::function<void(
    const float* input,
    float* output,
    size_t frames,
    double streamTime
    )>;



// Audio backend abstraction
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    // ===== Core Audio Operations =====

    // Initialize with configuration
    virtual void initialize(const StreamConfig& config) = 0;

    // Start audio processing with callback
    virtual void start(AudioCallback callback) = 0;

    // Stop audio processing
    virtual void stop() = 0;

    // ===== Stream Control =====

    // Pause/resume
    virtual void pause() = 0;
    virtual void resume() = 0;

    // Check state
    virtual bool isRunning() const = 0;
    virtual bool isPaused() const = 0;

    // ===== Stream Information =====

    // Get current configuration
    virtual StreamConfig getCurrentConfig() const = 0;

    // Get actual stream parameters (may differ from requested)
    virtual int getActualSampleRate() const = 0;
    virtual int getActualBufferSize() const = 0;
    virtual double getInputLatencyMs() const = 0;
    virtual double getOutputLatencyMs() const = 0;

    // Get current stream time in seconds
    virtual double getStreamTime() const = 0;

    // ===== Dynamic Configuration =====

    // Change configuration while running (may restart stream)
    virtual bool changeSampleRate(int newRate) = 0;
    virtual bool changeBufferSize(int newSize) = 0;

    // Device switching (may restart stream)
    virtual bool switchInputDevice(const std::string& deviceId) = 0;
    virtual bool switchOutputDevice(const std::string& deviceId) = 0;

    // ===== Performance Monitoring =====

    virtual LatencyInfo measureLatency() = 0;
    virtual double getCpuUsage() const = 0;
    virtual int getXrunCount() const = 0;  // Buffer over/under runs

    // ===== Error Handling =====

    virtual std::string getLastError() const = 0;
    virtual void clearError() = 0;

    // ===== Device Management =====

    virtual std::vector<std::unique_ptr<IAudioDevice>>
    enumerateDevices() const = 0;

    virtual std::unique_ptr<IAudioDevice>
    getCurrentInputDevice() const = 0;

    virtual std::unique_ptr<IAudioDevice>
    getCurrentOutputDevice() const = 0;

    // ===== Platform Specific =====

    virtual BackendType getBackendType() const = 0;
    virtual void* getPlatformHandle() const = 0;  // For advanced use

protected:
    // Protected constructor for factory pattern
    IAudioBackend() = default;

private:
    // Prevent copying
    IAudioBackend(const IAudioBackend&) = delete;
    IAudioBackend& operator=(const IAudioBackend&) = delete;
};

// Factory for creating backends
class AudioBackendFactory {
public:
    // Create backend based on configuration
    static std::unique_ptr<IAudioBackend> createBackend(
        const StreamConfig& config
        );

    // Create specific backend type
    static std::unique_ptr<IAudioBackend> createBackend(
        BackendType type
        );

    // Get available backend types for current platform
    static std::vector<BackendType> getAvailableBackends();

    // Get default backend for platform
    static BackendType getDefaultBackend();

    // Test backend compatibility
    static bool isBackendAvailable(BackendType type);
};
}

#endif // AUDIOBACKEND_H
