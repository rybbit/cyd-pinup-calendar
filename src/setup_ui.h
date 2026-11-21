#pragma once

// On-device editor for /calendar/config.txt (opened from the picker's SETUP button).
namespace Setup {
// Shows the settings list until the user taps CANCEL or SAVE. On SAVE the file is
// rewritten and `settings` reloaded from it; returns true. The caller applies the new
// values (time zone, layout, Wi-Fi).
bool run();
}  // namespace Setup
