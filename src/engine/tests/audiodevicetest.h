#include <catch2/catch_test_macros.hpp>
#include "../common/audiodevice.h"
#include <iostream>

using namespace AudioEngine;



TEST_CASE("AudioDevice capabilities", "[AudioDevice]") {
    auto& deviceManager = AudioDeviceManager::getInstance();
    auto devices = deviceManager.enumerateDevices();

    REQUIRE_FALSE(devices.empty());

    for (const auto& device : devices) {
        SECTION("Device " + device->getName()) {
            auto caps = device->getCapabilities();

            // Basic validation
            REQUIRE(caps.supportedSampleRates.size() > 0);
            REQUIRE(caps.maxOutputChannels >= 0);

            // Sample rates should be sorted
            for (size_t i = 1; i < caps.supportedSampleRates.size(); i++) {
                REQUIRE(caps.supportedSampleRates[i] > caps.supportedSampleRates[i-1]);
            }

            // Should support common sample rates
            bool hasCommonRate = false;
            for (int rate : {44100, 48000, 96000}) {
                if (device->supportsSampleRate(rate)) {
                    hasCommonRate = true;
                    break;
                }
            }
            REQUIRE(hasCommonRate);
        }
    }
}

TEST_CASE("AudioDevice comparison", "[AudioDevice]") {
    auto& deviceManager = AudioDeviceManager::getInstance();
    auto devices = deviceManager.enumerateDevices();

    if (devices.size() >= 2) {
        auto& device1 = devices[0];
        auto& device2 = devices[1];

        REQUIRE(*device1 == *device1);  // Self-equality
        REQUIRE(*device1 != *device2);  // Different devices should not be equal

        // Copy should be equal
        auto device1Copy = deviceManager.getDeviceById(device1->getId());
        REQUIRE(*device1 == *device1Copy);
    }
}

TEST_CASE("Device filtering", "[AudioDevice][Filter]") {
    auto& deviceManager = AudioDeviceManager::getInstance();

    SECTION("Find output devices") {
        auto allDevices = deviceManager.enumerateDevices();

        std::vector<std::unique_ptr<IAudioDevice>> outputDevices;
        for (const auto& device : allDevices) {
            if (device->getCapabilities().supportsOutput) {
                outputDevices.push_back(deviceManager.getDeviceById(device->getId()));
            }
        }

        REQUIRE_FALSE(outputDevices.empty());

        for (const auto& device : outputDevices) {
            auto caps = device->getCapabilities();
            REQUIRE(caps.supportsOutput == true);
            REQUIRE(caps.maxOutputChannels > 0);
        }
    }

    SECTION("Find high-sample-rate devices") {
        auto allDevices = deviceManager.enumerateDevices();

        std::vector<std::unique_ptr<IAudioDevice>> highRateDevices;
        for (const auto& device : allDevices) {
            if (device->supportsSampleRate(96000) ||
                device->supportsSampleRate(192000)) {
                highRateDevices.push_back(deviceManager.getDeviceById(device->getId()));
            }
        }

        // At least some devices should support high rates
        if (!highRateDevices.empty()) {
            for (const auto& device : highRateDevices) {
                REQUIRE(device->supportsSampleRate(96000) ||
                        device->supportsSampleRate(192000));
            }
        }
    }
}
