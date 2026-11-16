// The desktop functions each platform's host file defines, for the program's own use.

#pragma once

#include <filesystem>
#include <functional>
#include <string>

// Opens a web address in the default browser. The address is never run through a shell.
void launchUrl(const std::string& url);

// The folder the program keeps its settings in.
std::filesystem::path settingsFolder();

// One copy runs at a time. A start that finds one running asks it to show its window and then ends. True when the running copy
// answered, so this start should stop; false when none did, so this start carries on as the program.
bool forwardShowRequest();

// Accepts show requests from later starts on a thread, and calls handle on that thread for each one.
void listenForShowRequests(std::function<void()> handle);

// Stops answering later starts, so a copy started after this call runs instead of being handed to this one.
void stopListeningForShowRequests();
