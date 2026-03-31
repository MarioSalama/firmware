#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>

#ifdef T_EMBED_1101

enum EncoderGesture {
    GESTURE_NONE = 0,
    GESTURE_SINGLE_CLICK,
    GESTURE_DOUBLE_CLICK,
    GESTURE_LONG_PRESS,
    GESTURE_TWIST_CLICK, // Rotate then click within 300ms
};

// Callback type for gesture handlers
using GestureCallback = std::function<void()>;

// Initialize gesture detection system
void encoderGesturesInit();

// Poll for gestures - call from main input loop
// Returns detected gesture (GESTURE_NONE if no gesture)
EncoderGesture encoderGesturePoll();

// Register callbacks for gestures (used in specific contexts)
void onDoubleClick(GestureCallback cb);
void onLongPress(GestureCallback cb);
void onTwistClick(GestureCallback cb);

// Quick Favorites Wheel - shown on long press of encoder
void showFavoritesWheel();

// Add an item to recent tools (auto-managed, max 8)
void addRecentTool(const char* name, GestureCallback action);

// Quick tool switcher - shown on double click
void showQuickSwitcher();

#else
inline void encoderGesturesInit() {}
inline int encoderGesturePoll() { return 0; }
inline void showFavoritesWheel() {}
inline void showQuickSwitcher() {}
#endif
