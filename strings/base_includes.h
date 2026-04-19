
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <exception>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <concepts>
#include <unordered_map>
#include <utility>
#include <vector>
#include <bit>

#ifndef WINRT_LEAN_AND_MEAN
#include <ostream>
#endif

// C++ 20 headers

#include <version>
#include <compare>
#include <span>
#include <format>
#include <source_location>
#include <coroutine>

#if __has_include(<windowsnumerics.impl.h>)
#define WINRT_IMPL_NUMERICS
#include <directxmath.h>
#define _WINDOWS_NUMERICS_NAMESPACE_ winrt::Windows::Foundation::Numerics
#define _WINDOWS_NUMERICS_BEGIN_NAMESPACE_ extern "C++" namespace _WINDOWS_NUMERICS_NAMESPACE_
#define _WINDOWS_NUMERICS_END_NAMESPACE_
#include <windowsnumerics.impl.h>
#undef _WINDOWS_NUMERICS_NAMESPACE_
#undef _WINDOWS_NUMERICS_BEGIN_NAMESPACE_
#undef _WINDOWS_NUMERICS_END_NAMESPACE_
#endif

#ifdef WINRT_ENABLE_LEGACY_COM
#include <unknwn.h>
#include <inspectable.h>
#undef GetCurrentTime
#endif
