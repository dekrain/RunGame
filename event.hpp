#pragma once

#include <cstdint>

enum class EventType : uint32_t {
    Quit,
    KeyDown,
    KeyUp,
};

enum class LogicalKey : uint16_t {
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    N1, N2, N3, N4, N5, N6, N7, N8, N9, N0,
    Return, Escape, Backspace, Tab, Space,
    ArrowLeft, ArrowRight, ArrowUp, ArrowDown,
    Plus, Minus, Equals,
};

enum class PhysicalKey : uint16_t {
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    N1, N2, N3, N4, N5, N6, N7, N8, N9, N0,
    Return, Escape, Backspace, Tab, Space,
    ArrowLeft, ArrowRight, ArrowUp, ArrowDown,
    Minus, Equals,
};

union WinEvent {
    EventType type;
    struct {
        EventType type;
        LogicalKey lkey;
        PhysicalKey pkey;
    } key;
};
