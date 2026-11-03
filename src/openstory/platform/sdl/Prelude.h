// Force-included into every C++ file of the SDL build. Upstream OpenStory is
// built with MSVC, whose headers pull these in transitively; libc++ and
// libstdc++ do not, so files that use them without including them would not
// compile otherwise.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>
