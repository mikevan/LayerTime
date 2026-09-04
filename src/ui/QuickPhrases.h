#pragma once

#include <stddef.h>

// Canned phrases that replace the on-screen keyboard in the MeshCore and
// Meshtastic composers. Typing on a watch while moving does not work; someone
// who needs to say something needs one tap, not thirty.
//
// Meshtastic's own CannedMessageModule stores its list as a pipe-delimited
// string capped at 200 bytes. Joined with '|' this list is 199 bytes, so it
// can be pushed from the phone app later without trimming anything.
namespace QuickPhrases {

static const char *const kPhrases[] = {
    "Yes",
    "No",
    "OK",
    "On my way",
    "Be there in 10",
    "Almost there",
    "Running late",
    "Where are you?",
    "Im here",
    "Heading back",
    "Wait for me",
    "Need help",
    "All clear",
    "Copy that",
    "Standby",
    "Call me",
    "Cant talk",
    "Radio check",
    "Low battery",
    "Good night",
};

static const size_t kCount = sizeof(kPhrases) / sizeof(kPhrases[0]);

} // namespace QuickPhrases
