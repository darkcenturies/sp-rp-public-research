// The window a player actually sees when the game dies.
//
// The log file was always the easy half. The hard half is that nobody reads it:
// a player who just crashed alt-tabs, shrugs, and posts "it crashed" in
// #bugs-problems, and somebody spends an evening working out that their game
// was never 1.0. CrashInfo has shown its findings in a window for years and
// that is the half worth copying.
//
// So this puts the answer in front of them while they are still looking at the
// screen, with the log path underneath for when the answer is "post this".
#pragma once

namespace crashwindow {

// Blocks until the player closes it. Safe to call from inside an exception
// filter, and safe to call when the window cannot be created - it gives up
// quietly rather than making a dying process worse.
//
//   diagnosis - the text to show, already rendered
//   logPath   - full path of the log we just wrote, shown and copyable
// knownCause selects the red crash banner. Unknown signatures get the yellow
// "I'm not sure" banner so uncertainty is visible before the player reads a
// word of the report.
void Show(const char* diagnosis, const char* diagnosisSpanish,
          const char* logPath, bool knownCause = true,
          bool startInSpanish = false);

}  // namespace crashwindow
