#pragma once
#include <mutex>

namespace Gameplay {
inline std::recursive_mutex gate;
using Guard = std::lock_guard<std::recursive_mutex>;
}