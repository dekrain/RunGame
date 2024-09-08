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

#ifdef DEFINE_KEY_TO_STRING
#define CASE(enum) case TYPE::enum: return #enum;
inline char const* LogicalKeyAsString(LogicalKey key) {
    #define TYPE LogicalKey
    switch (key) {
        CASE(Unknown)
        CASE(A) CASE(B) CASE(C) CASE(D) CASE(E) CASE(F) CASE(G) CASE(H) CASE(I) CASE(J) CASE(K) CASE(L) CASE(M) CASE(N) CASE(O) CASE(P) CASE(Q) CASE(R) CASE(S) CASE(T) CASE(U) CASE(V) CASE(W) CASE(X) CASE(Y) CASE(Z)
        CASE(N1) CASE(N2) CASE(N3) CASE(N4) CASE(N5) CASE(N6) CASE(N7) CASE(N8) CASE(N9) CASE(N0)
        CASE(Return) CASE(Escape) CASE(Backspace) CASE(Tab) CASE(Space)
        CASE(ArrowLeft) CASE(ArrowRight) CASE(ArrowUp) CASE(ArrowDown)
        CASE(Plus) CASE(Minus) CASE(Equals)
    }
    return "<invalid>";
    #undef TYPE
}
inline char const* PhysicalKeyAsString(PhysicalKey key) {
    #define TYPE PhysicalKey
    switch (key) {
        CASE(Unknown)
        CASE(A) CASE(B) CASE(C) CASE(D) CASE(E) CASE(F) CASE(G) CASE(H) CASE(I) CASE(J) CASE(K) CASE(L) CASE(M) CASE(N) CASE(O) CASE(P) CASE(Q) CASE(R) CASE(S) CASE(T) CASE(U) CASE(V) CASE(W) CASE(X) CASE(Y) CASE(Z)
        CASE(N1) CASE(N2) CASE(N3) CASE(N4) CASE(N5) CASE(N6) CASE(N7) CASE(N8) CASE(N9) CASE(N0)
        CASE(Return) CASE(Escape) CASE(Backspace) CASE(Tab) CASE(Space)
        CASE(ArrowLeft) CASE(ArrowRight) CASE(ArrowUp) CASE(ArrowDown)
        CASE(Minus) CASE(Equals)
    }
    return "<invalid>";
    #undef TYPE
}
#undef CASE
#endif

union WinEvent {
    EventType type;
    struct {
        EventType type;
        LogicalKey lkey;
        PhysicalKey pkey;
    } key;
};
