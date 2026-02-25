#pragma once

#include <map>
#include <string>

namespace app {

void set_debug_logging(bool enabled);
void log_event(const std::string& level,
               const std::string& event,
               const std::string& message,
               const std::map<std::string, std::string>& fields = {});

} // namespace app
