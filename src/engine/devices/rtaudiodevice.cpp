#include "rtaudiodevice.h"
#include <vector>
#include <algorithm>
namespace AudioEngine {
RtAudioDevice::RtAudioDevice(int deviceId, const RtAudio::DeviceInfo& info,
                            BackendType backendType, bool isDefaultInput,
                            bool isDefaultOutput)
    : m_deviceId(deviceId)
    , m_deviceInfo(info)
    , m_backendType(backendType)
    , m_isDefaultInput(isDefaultInput)
    , m_isDefaultOutput(isDefaultOutput)
{
    // Generate unique ID: backend-type_device-index_name
    std::stringstream ss;
    ss << static_cast<int>(backendType) << "_"
       << deviceId << "_"
       << std::hash<std::string>{}(info.name);
    m_id = ss.str();
}

std::string RtAudioDevice::getId() const {
    return m_id;
}

std::string RtAudioDevice::getName() const {
    return m_deviceInfo.name;
}

std::string RtAudioDevice::getVendor() const {
    // RtAudio doesn't provide vendor info directly
    // We can parse it from the name or return empty
    return ""; // Could be enhanced
}

BackendType RtAudioDevice::getBackendType() const {
    return m_backendType;
}

DeviceCapabilities RtAudioDevice::getCapabilities() const {
    DeviceCapabilities caps;

    // Copy supported sample rates
    caps.supportedSampleRates = m_deviceInfo.sampleRates;

    // RtAudio doesn't provide buffer size info directly
    // We'll assume typical values and allow runtime testing
    caps.supportedBufferSizes = {64, 128, 256, 512, 1024, 2048, 4096};

    // Supported formats (RtAudio uses bitwise OR for multiple formats)
    if (m_deviceInfo.nativeFormats & RTAUDIO_FLOAT32)
        caps.supportedFormats.push_back(SampleFormat::Float32);
    if (m_deviceInfo.nativeFormats & RTAUDIO_SINT16)
        caps.supportedFormats.push_back(SampleFormat::Int16);
    if (m_deviceInfo.nativeFormats & RTAUDIO_SINT24)
        caps.supportedFormats.push_back(SampleFormat::Int24);
    if (m_deviceInfo.nativeFormats & RTAUDIO_SINT32)
        caps.supportedFormats.push_back(SampleFormat::Int32);

    // Channel counts
    caps.maxInputChannels = m_deviceInfo.inputChannels;
    caps.maxOutputChannels = m_deviceInfo.outputChannels;

    // Capability flags
    caps.supportsInput = m_deviceInfo.inputChannels > 0;
    caps.supportsOutput = m_deviceInfo.outputChannels > 0;
    caps.supportsDuplex = m_deviceInfo.duplexChannels > 0;

    // Latency information
    caps.minLatencyMs = m_deviceInfo.preferredSampleRate > 0
                            ? (m_deviceInfo.preferredSampleRate / 1000.0) * 0.1 // 0.1ms per sample
                            : 1.0;
    caps.maxLatencyMs = 100.0; // Reasonable default

    // Default flags
    caps.isDefaultInput = m_isDefaultInput;
    caps.isDefaultOutput = m_isDefaultOutput;

    return caps;
}

bool RtAudioDevice::isAvailable() const {
    // We assume device is available if we could query it
    return true;
}

bool RtAudioDevice::isDefaultInput() const {
    return m_isDefaultInput;
}

bool RtAudioDevice::isDefaultOutput() const {
    return m_isDefaultOutput;
}

bool RtAudioDevice::supportsSampleRate(int rate) const {
    unsigned int sampleRate = (unsigned int)(rate);
    return std::find(m_deviceInfo.sampleRates.begin(),
                     m_deviceInfo.sampleRates.end(),
                     sampleRate) != m_deviceInfo.sampleRates.end();
}

bool RtAudioDevice::supportsBufferSize(int size) const {
    // RtAudio doesn't provide this info, so we accept common sizes
    const std::vector<int> commonSizes = {64, 128, 256, 512, 1024, 2048, 4096};
    return std::find(commonSizes.begin(), commonSizes.end(), size)
           != commonSizes.end();
}

bool RtAudioDevice::supportsFormat(SampleFormat format) const {
    RtAudioFormat rtFormat = convertToRtAudioFormat(format);
    return (m_deviceInfo.nativeFormats & rtFormat) != 0;
}

double RtAudioDevice::getDefaultInputLatencyMs() const {
    if (m_deviceInfo.preferredSampleRate > 0 && m_deviceInfo.inputChannels > 0) {
        return (512.0 / m_deviceInfo.preferredSampleRate) * 1000.0;
    }
    return 10.0; // Default 10ms
}

double RtAudioDevice::getDefaultOutputLatencyMs() const {
    if (m_deviceInfo.preferredSampleRate > 0 && m_deviceInfo.outputChannels > 0) {
        return (512.0 / m_deviceInfo.preferredSampleRate) * 1000.0;
    }
    return 10.0; // Default 10ms
}

bool RtAudioDevice::operator==(const IAudioDevice& other) const {
    const RtAudioDevice* otherRt = dynamic_cast<const RtAudioDevice*>(&other);
    if (!otherRt) return false;
    return m_id == otherRt->m_id;
}

bool RtAudioDevice::operator!=(const IAudioDevice& other) const {
    return !(*this == other);
}

std::string RtAudioDevice::toString() const {
    std::stringstream ss;
    ss << "RtAudioDevice: " << m_deviceInfo.name
       << " (ID: " << m_deviceId
       << ", In: " << m_deviceInfo.inputChannels
       << ", Out: " << m_deviceInfo.outputChannels
       << ", DefaultIn: " << (m_isDefaultInput ? "Yes" : "No")
       << ", DefaultOut: " << (m_isDefaultOutput ? "Yes" : "No")
       << ")";
    return ss.str();
}

SampleFormat RtAudioDevice::convertRtAudioFormat(RtAudioFormat format) {
    switch (format) {
    case RTAUDIO_FLOAT32: return SampleFormat::Float32;
    case RTAUDIO_SINT16:  return SampleFormat::Int16;
    case RTAUDIO_SINT24:  return SampleFormat::Int24;
    case RTAUDIO_SINT32:  return SampleFormat::Int32;
    default: return SampleFormat::Float32;
    }
}

RtAudioFormat RtAudioDevice::convertToRtAudioFormat(SampleFormat format) {
    switch (format) {
    case SampleFormat::Float32: return RTAUDIO_FLOAT32;
    case SampleFormat::Int16:   return RTAUDIO_SINT16;
    case SampleFormat::Int24:   return RTAUDIO_SINT24;
    case SampleFormat::Int32:   return RTAUDIO_SINT32;
    default: return RTAUDIO_FLOAT32;
    }
}

} // namespace AudioEngine
