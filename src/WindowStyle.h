// WindowStyle.h - Single source of truth for the window's outer shape.
//
// Every part of the shell (the rounded root container, the anti-aliased corner
// clip, the outline overlay and Theme::metrics()) reads its radius from here, so
// changing the look of the window corners is a one-line edit.
#pragma once

/// Radius of the application's four outer corners, in logical pixels.
/// Change this value to restyle the whole window.
constexpr int WINDOW_CORNER_RADIUS = 16;

/// Transparent padding kept around the visible body for the outer glow.
/// The window is this much larger than the rounded container on every side.
constexpr int WINDOW_GLOW_MARGIN = 18;
