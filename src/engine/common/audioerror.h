#ifndef AUDIOERROR_H
#define AUDIOERROR_H
#include <stdexcept>
#include <string>

namespace AudioEngine {
enum class AudioErrorCode {
    Success = 0,
    DeviceNotFound,
    DeviceUnavailable,
    InvalidConfiguration,
    SampleRateUnsupported,
    BufferSizeUnsupported,
    AudioBackendInitFailed,
    AudioBackendStartFailed,
    AudioBackendStopFailed,
    RealTimePriorityFailed,
    AudioCallbackError,
    AudioStreamClosed,
    PlatformSpecificError
};

class AudioException : public std::runtime_error {
public:
    AudioException(AudioErrorCode code, const std::string& message)
        : std::runtime_error(message), m_code(code) {}

    AudioErrorCode code() const { return m_code; }

    static AudioException deviceNotFound(const std::string& deviceName = "");
    static AudioException unsupportedSampleRate(int requested, int supported);
    static AudioException unsupportedBufferSize(int requested, int min, int max);

private:
    AudioErrorCode m_code;
};
}



#endif // AUDIOERROR_H
