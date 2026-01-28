#ifndef RTAUDIODEVICE_H
#define RTAUDIODEVICE_H
#include "../common/audiodevice.h"
#include "RtAudio.h"
namespace AudioEngine {


class RtAudioDevice : public IAudioDevice {
public:
    // Constructor from RtAudio device info
    RtAudioDevice(int deviceId, const RtAudio::DeviceInfo& info,
                  BackendType backendType, bool isDefaultInput,
                  bool isDefaultOutput);

    // Device identification
    std::string getId() const override;
    std::string getName() const override;
    std::string getVendor() const override;
    BackendType getBackendType() const override;

    // Capabilities
    DeviceCapabilities getCapabilities() const override;

    // Status
    bool isAvailable() const override;
    bool isDefaultInput() const override;
    bool isDefaultOutput() const override;

    // Format queries
    bool supportsSampleRate(int rate) const override;
    bool supportsBufferSize(int size) const override;
    bool supportsFormat(SampleFormat format) const override;

    // Latency information
    double getDefaultInputLatencyMs() const override;
    double getDefaultOutputLatencyMs() const override;

    // Comparison
    bool operator==(const IAudioDevice& other) const override;
    bool operator!=(const IAudioDevice& other) const override;

    // String representation
    std::string toString() const override;

    // RtAudio-specific accessors
    int getRtAudioDeviceId() const { return m_deviceId; }
    RtAudio::DeviceInfo getRtDeviceInfo() const { return m_deviceInfo; }
    static SampleFormat convertRtAudioFormat(RtAudioFormat format);
    static RtAudioFormat convertToRtAudioFormat(SampleFormat format);

private:
    int m_deviceId;
    RtAudio::DeviceInfo m_deviceInfo;
    BackendType m_backendType;
    bool m_isDefaultInput;
    bool m_isDefaultOutput;
    std::string m_id; // Generated unique ID

    // Convert RtAudio sample format to our format
};

} // namespace AudioEngine

#endif // RTAUDIODEVICE_H
