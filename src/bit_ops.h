#pragma once

#include <cstdint>
#include <type_traits>

namespace GBC {

using byte = uint8_t;
using half = uint16_t;

template <typename T>
constexpr bool isBitSet(T value, byte bit) {
    static_assert(std::is_integral_v<T>);
    return (value & (static_cast<T>(1) << bit)) != 0;
}

template <typename T>
constexpr T setBit(T value, byte bit) {
    static_assert(std::is_integral_v<T>);
    return value | (static_cast<T>(1) << bit);
}

template <typename T>
constexpr T clearBit(T value, byte bit) {
    static_assert(std::is_integral_v<T>);
    return value & ~(static_cast<T>(1) << bit);
}

template <typename T>
constexpr T toggleBit(T value, byte bit) {
    static_assert(std::is_integral_v<T>);
    return value ^ (static_cast<T>(1) << bit);
}

template <typename T>
constexpr T getBitRange(T value, byte start, byte length) {
    static_assert(std::is_integral_v<T>);
    const T mask = (static_cast<T>(1) << length) - 1;
    return (value >> start) & mask;
}

template <typename T>
constexpr T extractBits(T value, byte shift, T mask) {
    static_assert(std::is_integral_v<T>);
    return (value >> shift) & mask;
}

template <typename T>
constexpr T maskBits(T value, T mask) {
    static_assert(std::is_integral_v<T>);
    return value & mask;
}

}  // namespace GBC

