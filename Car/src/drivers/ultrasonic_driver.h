#ifndef ULTRASONIC_DRIVER_H
#define ULTRASONIC_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <cstddef>
#include <array>

/**
 * @brief HC-SR04 Ultrasonic sensor driver with filtering
 */
class Ultrasonic {
public:
    Ultrasonic();

    bool init(uint8_t trig_pin, uint8_t echo_pin);
    float readDistanceCM();
    float readDistanceRaw();

private:
    uint8_t trig_pin_;
    uint8_t echo_pin_;
    bool initialized_;
    float last_distance_;

    static constexpr unsigned long TRIG_PULSE_US = 10;
    static constexpr unsigned long TRIG_SETTLE_US = 2;
    static constexpr unsigned long ECHO_TIMEOUT_US = 25000;
    static constexpr float SPEED_OF_SOUND_CM_PER_US = 0.034;
    static constexpr float DISTANCE_DIVISOR = 2.0;

    static constexpr size_t FILTER_BUFFER_SIZE = 5;
    static constexpr float MAX_DEVIATION_CM = 50.0f;
    static constexpr unsigned int MAX_CONSECUTIVE_INVALID = 5;
    static constexpr unsigned int MAX_CONSECUTIVE_SPIKE = 3;

    std::array<float, FILTER_BUFFER_SIZE> distance_buffer_;
    size_t buffer_index_;
    size_t valid_samples_count_;
    float last_filtered_distance_;
    unsigned int consecutive_invalid_count_;
    unsigned int consecutive_spike_count_;

    float readDistanceRawInternal();
    float calculateAverage() const;
    bool isValidSample(float raw, float current_avg) const;
};

#endif // ULTRASONIC_DRIVER_H

