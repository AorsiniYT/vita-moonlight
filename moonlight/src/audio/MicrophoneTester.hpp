/*
    Copyright 2025 AorsiniYT

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include <atomic>
#include <thread>

/**
 * @brief Local microphone testing with audio loopback
 *
 * Captures audio from the PS Vita microphone and plays it back through
 * the speakers/headphones in real-time. Supports both RAW and Opus modes
 * to test if audio quality issues are from capture or compression.
 *
 * Usage:
 *   auto& tester = MicrophoneTester::getInstance();
 *   tester.setOpusMode(true);   // Test with Opus compression
 *   tester.start();  // Begin loopback
 *   // ... user can hear their voice ...
 *   tester.stop();   // End loopback
 */
class MicrophoneTester
{
  public:
    /**
     * @brief Get singleton instance
     */
    static MicrophoneTester& getInstance();

    /**
     * @brief Start microphone loopback test
     * @return true if started successfully, false on error
     */
    bool start();

    /**
     * @brief Stop microphone loopback test
     */
    void stop();

    /**
     * @brief Enable/disable Opus compression test
     * @param enabled true = test with Opus encode/decode, false = RAW PCM
     */
    void setOpusMode(bool enabled);

    /**
     * @brief Set microphone gain (1.0 - 50.0)
     * Applies immediately during loopback test
     */
    void setGain(float gain);

    /**
     * @brief Check if loopback is currently active
     * @return true if running
     */
    bool isRunning() const;

  private:
    MicrophoneTester();
    ~MicrophoneTester();

    // Disable copy/move
    MicrophoneTester(const MicrophoneTester&)            = delete;
    MicrophoneTester& operator=(const MicrophoneTester&) = delete;

    /**
     * @brief Loopback thread function
     * Continuously reads from microphone and writes to audio output
     */
    void loopbackThreadFunc();

    // State
    std::atomic<bool> running_ { false };
    std::atomic<bool> use_opus_ { false }; // Test with Opus compression?
    std::atomic<float> gain_ { 1.0f }; // Atomic for thread-safe dynamic updates
    std::thread loopback_thread_;
};
