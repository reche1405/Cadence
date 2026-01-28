#ifndef AUDIODEVICE_H
#define AUDIODEVICE_H

#include "audioconfig.h"
#include <string>
#include <vector>
#include <memory>

namespace AudioEngine {

// Forward declaration
class IAudioBackend;

// Audio device abstraction
class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;

    // Device identification
    virtual std::string getId() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getVendor() const = 0;
    virtual BackendType getBackendType() const = 0;

    // Capabilities
    virtual DeviceCapabilities getCapabilities() const = 0;

    // Status
    virtual bool isAvailable() const = 0;
    virtual bool isDefaultInput() const = 0;
    virtual bool isDefaultOutput() const = 0;

    // Format queries
    virtual bool supportsSampleRate(int rate) const = 0;
    virtual bool supportsBufferSize(int size) const = 0;
    virtual bool supportsFormat(SampleFormat format) const = 0;

    // Latency information
    virtual double getDefaultInputLatencyMs() const = 0;
    virtual double getDefaultOutputLatencyMs() const = 0;

    // Comparison
    virtual bool operator==(const IAudioDevice& other) const = 0;
    virtual bool operator!=(const IAudioDevice& other) const = 0;

    // String representation
    virtual std::string toString() const = 0;
};

// Device list and management
class AudioDeviceManager {
public:
    // Singleton access
    static AudioDeviceManager& getInstance();

    // Device enumeration
    std::vector<std::unique_ptr<IAudioDevice>> enumerateDevices(BackendType backend = BackendType::Auto);

    // Default devices
    std::unique_ptr<IAudioDevice> getDefaultInputDevice(BackendType backend = BackendType::Auto);
    std::unique_ptr<IAudioDevice> getDefaultOutputDevice(BackendType backend = BackendType::Auto);

    // Device by ID/name
    std::unique_ptr<IAudioDevice> getDeviceById(const std::string& id);
    std::unique_ptr<IAudioDevice> getDeviceByName(const std::string& name);

    // Backend management
    void setPreferredBackend(BackendType backend);
    BackendType getPreferredBackend() const;

    // Refresh device list (e.g., after hardware changes)
    void refresh();

private:
    AudioDeviceManager() = default;
    ~AudioDeviceManager() = default;

    // Prevent copying
    AudioDeviceManager(const AudioDeviceManager&) = delete;
    AudioDeviceManager& operator=(const AudioDeviceManager&) = delete;

    BackendType m_preferredBackend = BackendType::Auto;
    std::vector<std::unique_ptr<IAudioDevice>> m_cachedDevices;
};


}
#endif // AUDIODEVICE_H
