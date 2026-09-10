// Faster, smoother zooming on the full map.
//
// San Andreas moves the map's zoom in fixed steps and applies each one the
// instant it happens. On a map this size that is two complaints at once: it
// takes a lot of turns of the wheel to cross the state, and each turn jumps
// rather than moves, which makes it hard to keep track of where you were
// looking.
//
// This owns wheel zoom because Eagle's scaled native step can transiently send
// the overview outside ProperRadar's valid range. Zoom-in holds the world point
// under the cursor; zoom-out moves toward the whole-continent centre. Hovering
// at a screen edge pans with smooth acceleration once zoom has settled.
#pragma once

namespace mapzoom {

// Validate and install the narrowly scoped native map-control patches. If any
// expected instruction differs, zoom/pan remain disabled for the session.
bool Install();

// Paired around CGame::Process. The first hands ProperRadar the next valid view;
// the second reads the newly updated wheel state for the following frame.
void BeforeFrame();
void AfterFrame();

}  // namespace mapzoom
