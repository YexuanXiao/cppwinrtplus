
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
#include <unordered_map>
#include <utility>
#include <vector>

#if __has_include(<version>)
#include <version>
#endif

// <windowsnumerics.impl.h> pulls in large, hard-to-control legacy headers. In header builds we keep the
// existing behavior, but in module builds it's provided by the winrt.numerics module.
#ifndef WINRT_MODULE
#if __has_include(<windowsnumerics.impl.h>)
#define WINRT_IMPL_NUMERICS
#include <directxmath.h>
#endif

#endif

#ifndef WINRT_LEAN_AND_MEAN
#include <ostream>
#endif

#ifdef __cpp_lib_span
#include <span>
#endif

#ifdef __cpp_lib_format
#include <format>
#endif

#ifdef __cpp_lib_source_location
#include <source_location>
#endif

#ifdef __cpp_lib_coroutine
#include <coroutine>
#else
#error C++/WinRT requires coroutine support, which is currently missing. Try enabling C++20 in your compiler.
#endif
