#pragma once

namespace demo {

// Routes KHR_debug messages to the log. No-op if the context was not created with the debug flag.
void installGlDebugOutput();

}  // namespace demo
