#include "audiobackendtest.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../common/audiobackend.h"
#include "../common/audiodevice.h"
#include "../common/audioerror.h"
#include <cmath>
#include <iostream>
#include <thread>
#include <atomic>
using namespace AudioEngine;
// Test callback that generates a sine wave
class TestSineCallback {
public:
    TestSineCallback(double frequency = 440.0, double amplitude = 0.5)
        : m_frequency(frequency), m_amplitude(amplitude), m_phase(0.0) {}

    void operator()(const float* input, float* output, size_t frames, double streamTime) {
        double phaseIncrement = 2.0 * M_PI * m_frequency / m_sampleRate;

        for (size_t i = 0; i < frames; ++i) {
            float sample = static_cast<float>(m_amplitude * sin(m_phase));

            // Fill all channels with same sine wave
            for (int ch = 0; ch < m_channels; ++ch) {
                output[i * m_channels + ch] = sample;
            }

            m_phase += phaseIncrement;
            if (m_phase >= 2.0 * M_PI) {
                m_phase -= 2.0 * M_PI;
            }
        }

        m_callbackCount++;
        m_totalFrames += frames;
    }

    void setSampleRate(double sampleRate) { m_sampleRate = sampleRate; }
    void setChannels(int channels) { m_channels = channels; }

    int getCallbackCount() const { return m_callbackCount; }
    size_t getTotalFrames() const { return m_totalFrames; }

private:
    double m_frequency;
    double m_amplitude;
    double m_phase;
    double m_sampleRate = 48000.0;
    int m_channels = 2;
    std::atomic<int> m_callbackCount{0};
    std::atomic<size_t> m_totalFrames{0};
};

// Test fixture
class AudioBackendTest {
protected:
    void SetUp() {
        // Get default configuration
        m_config.sampleRate = 48000;
        m_config.bufferSize = 512;
        m_config.inputChannels = 2;
        m_config.outputChannels = 2;
        m_config.format = SampleFormat::Float32;
        m_config.bufferStrategy = BufferStrategy::Stable;
    }

    StreamConfig m_config;
};

TEST_CASE("AudioBackendFactory creates backend", "[AudioBackend]") {
    SECTION("Create with default configuration") {
        StreamConfig config;
        auto backend = AudioBackendFactory::createBackend(config);

        REQUIRE(backend != nullptr);
        REQUIRE(backend->getBackendType() != BackendType::Auto);
    }

    SECTION("Create specific backend type") {
        auto backends = AudioBackendFactory::getAvailableBackends();

        for (auto type : backends) {
            if (type != BackendType::Auto) {
                auto backend = AudioBackendFactory::createBackend(type);
                REQUIRE(backend != nullptr);
                REQUIRE(backend->getBackendType() == type);
            }
        }
    }
}

TEST_CASE("AudioBackend basic operations", "[AudioBackend]") {
    auto backend = AudioBackendFactory::createBackend(BackendType::Auto);
    REQUIRE(backend != nullptr);

    StreamConfig config;
    config.sampleRate = 48000;
    config.bufferSize = 512;
    config.outputChannels = 2;

    SECTION("Initialize and check config") {
        backend->initialize(config);
        auto currentConfig = backend->getCurrentConfig();

        REQUIRE(currentConfig.sampleRate == config.sampleRate);
        REQUIRE(currentConfig.outputChannels == config.outputChannels);
    }

    SECTION("Start and stop stream") {
        backend->initialize(config);

        TestSineCallback sineCallback;
        sineCallback.setSampleRate(config.sampleRate);
        sineCallback.setChannels(config.outputChannels);

        // Start the stream
        backend->start([&](const float* input, float* output, size_t frames, double time) {
            sineCallback(input, output, frames, time);
        });

        REQUIRE(backend->isRunning() == true);

        // Let it run for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Stop the stream
        backend->stop();

        REQUIRE(backend->isRunning() == false);
        REQUIRE(sineCallback.getCallbackCount() > 0);
    }

    SECTION("Pause and resume") {
        backend->initialize(config);

        std::atomic<int> callbackCount{0};
        backend->start([&](const float* input, float* output, size_t frames, double time) {
            callbackCount++;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int countBeforePause = callbackCount.load();

        backend->pause();
        REQUIRE(backend->isPaused() == true);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int countAfterPause = callbackCount.load();

        backend->resume();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int countAfterResume = callbackCount.load();

        backend->stop();

        // Should have callbacks before pause and after resume, but not during pause
        REQUIRE(countBeforePause > 0);
        REQUIRE(countAfterResume > countAfterPause);
    }
}

TEST_CASE("AudioDevice enumeration", "[AudioDevice]") {
    auto& deviceManager = AudioDeviceManager::getInstance();

    SECTION("Enumerate all devices") {
        auto devices = deviceManager.enumerateDevices();

        REQUIRE_FALSE(devices.empty());

        for (const auto& device : devices) {
            REQUIRE(device != nullptr);
            REQUIRE_FALSE(device->getName().empty());
            REQUIRE(device->getId() != device->getName()); // ID should be unique
        }
    }

    SECTION("Get default devices") {
        auto defaultInput = deviceManager.getDefaultInputDevice();
        auto defaultOutput = deviceManager.getDefaultOutputDevice();

        // At least output device should exist
        REQUIRE(defaultOutput != nullptr);

        if (defaultInput) {
            REQUIRE(defaultInput->supportsFormat(SampleFormat::Float32));
        }
    }
}

TEST_CASE("AudioBackend latency measurement", "[AudioBackend][Latency]") {
    auto backend = AudioBackendFactory::createBackend(BackendType::Auto);
    REQUIRE(backend != nullptr);

    StreamConfig config;
    config.sampleRate = 48000;
    config.bufferSize = 512;
    config.outputChannels = 2;

    backend->initialize(config);

    // Simple null callback
    backend->start([](const float* input, float* output, size_t frames, double time) {
        // Do nothing - just measure overhead
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto latency = backend->measureLatency();

    // Theoretical latency should match buffer size
    double theoreticalMs = (config.bufferSize * 1000.0) / config.sampleRate;

    using Catch::Matchers::WithinAbs;
    REQUIRE_THAT(latency.theoreticalMs,
                 WithinAbs(theoreticalMs, theoreticalMs * 0.1)); // Within 10%

    // Measured latency should be reasonable
    REQUIRE(latency.measuredMs >= 0);
    REQUIRE(latency.measuredMs < 100.0); // Should be less than 100ms

    backend->stop();
}

TEST_CASE("AudioBackend error handling", "[AudioBackend][Error]") {
    SECTION("Invalid configuration") {
        auto backend = AudioBackendFactory::createBackend(BackendType::Auto);
        REQUIRE(backend != nullptr);

        StreamConfig invalidConfig;
        invalidConfig.sampleRate = 999999; // Unlikely to be supported
        invalidConfig.bufferSize = 999999;

        // Should throw or handle gracefully
        REQUIRE_THROWS_AS(backend->initialize(invalidConfig), AudioException);
    }

    SECTION("Start without initialization") {
        auto backend = AudioBackendFactory::createBackend(BackendType::Auto);
        REQUIRE(backend != nullptr);

        // Starting without initialization should fail
        REQUIRE_THROWS_AS(backend->start([](auto...){ }), AudioException);
    }
}

