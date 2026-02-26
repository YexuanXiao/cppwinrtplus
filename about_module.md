# C++/WinRT Plus: Bringing C++ Standard Modules to Windows Development

## Implementation Overview

Here's how this was implemented.

This week, I implemented C++ standard modules on my own C++/WinRT fork ([C++/WinRT Plus](https://github.com/YexuanXiao/cppwinrtplus)). I spent approximately 36-40 hours of effective time implementing it (including this article). While I don't consider my understanding of modules to be as deep as those who implemented them personally, I possess sufficient knowledge to understand compiler complaints. Therefore, I initially viewed it as a challenge, uncertain whether it would work. The results turned out to be excellent.

During the implementation process, I used AI to help me understand how C++/WinRT works. The C++/WinRT code generator was simpler than I estimated, so AI interpreted it well. Due to the high code quality of C++/WinRT itself, I was able to make gradual progress even during the most difficult moments.

The current implementation status is that all header files can be built as modules. I'm uncertain whether anything is missing, but I haven't seen any bad news. I am seeking more people interested in the project to help improve C++/WinRT Plus and reduce my burden. My fork maintains full compatibility and test coverage, except it no longer supports C++17 and C++/CX. I even discovered [a bug](https://github.com/microsoft/cppwinrt/issues/1540) in the C++/WinRT tests.

If you are also interested in C++ modules/C++/WinRT, or if you are currently using C++/WinRT, I hope you can try my fork. If you are satisfied with it, please share it with more people.

### MSVC Compiler Status

The MSVC team has fixed a large number of bugs over the past few years, and it can be said that modules are now highly usable. One remaining bug is that cross-module using declarations don't work, requiring alias declarations as a replacement. Actually, apart from this bug I knew about in advance, I haven't encountered any other MSVC bugs. The bug was [marked as fixed a few days ago](https://developercommunity.visualstudio.com/t/C-modules-compile-error-when-using-a-u/10981263#T-ND11047313) and will be released with the next version.

### C++/WinRT Characteristics

C++/WinRT is a header-only library, which brings many conveniences. This means any header files can be combined without conflicts. More valuable is that C++/WinRT doesn't use internal linkage, avoiding various potential issues.

However, C++/WinRT still needed significant improvements to achieve modularization.

- First, C++/WinRT doesn't strictly use `std::` qualification for things existing in the C standard library, which meant I had to manually fix all missing prefixes.
- Second, C++/WinRT mistakenly placed a declaration in a file that should only define macros. I believe this was an oversight in the collaboration process, since C++/WinRT intentionally separates macros (which also brings many conveniences), but missed this one. My solution was to split that declaration into a separate header file.

Finally, C++/WinRT actually chose the wrong path when attempting to support modules by having all header files share a single module, which leads to terrible results. This would make the BMI (or .ifc) file 260MB in size—9 times larger than the STL's 29MB. It's foreseeable that with such a design, compilation will slow down.

C++/WinRT also attempts to implement modules without using std modules, which also causes issues, as will be explained further later. However, this is not C++/WinRT's fault, as STL's module support was completed in 2024.

### The Solution

Therefore, I implemented a more complicated approach.

First, write all header files in the following pattern:

```cpp
#pragma once
#ifndef WINRT_XXX_H
#define WINRT_XXX_H
#pragma push_macro("WINRT_EXPORT")
#undef WINRT_EXPORT
#if !defined(WINRT_MODULE) // legacy header path
#define WINRT_EXPORT
#include <winrt/base.h>
#include <dep headers>
#else
#define WINRT_EXPORT export
#endif
// declarations/definitions
#pragma pop_macro("WINRT_EXPORT")
#endif
```

Then, implement module interface units (.ixx) in the following pattern for these header files:

```cpp
module;
#define WINRT_MODULE
#include <intrin.h>
#include <cstddef>
#include <version>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
// This file only defines macros and does not contain any declarations
#include "winrt/module.h"
export module Windows.XX;
import deps;

#include "impls"
```

Since macros in the global module fragment are visible to subsequent `#includes` in the current file, defining the `WINRT_MODULE` macro on the second line will convert subsequent header files into module implementation files.

This design allows you to import exactly what you want, not unnecessary junk. This is especially significant for the `Windows.UI.Xaml` namespace, which generates over 90MB of BMI, accounting for one-third of the total. (I advocate for using WinUI3.)

### Build Configuration

Unfortunately, due to special reasons (which won't change in the future), when using MSBuild with C++/WinRT Plus, once interface units are added, they are compiled. Therefore, by default, even if you don't use `Windows.UI.Xaml`, it still consumes valuable time.

Thus, C++/WinRT Plus provides the ability to disable certain namespaces via a configuration file. You can create a `CppWinRT.config` file in your solution directory with the following content:

```xml
<?xml version="1.0" encoding="utf-8"?>
<configuration>
    <exclude>
        <prefix>Windows.UI.Xaml</prefix>
        <!-- Windows.ApplicationModel.Store depends on Windows.UI.Xaml -->
        <prefix>Windows.ApplicationModel.Store</prefix>
    </exclude>
</configuration>
```

This can significantly reduce compilation time. Note that namespace exclusion occurs at a very early stage of the build process, so a clean build needs to be performed for it to take effect.

If you're using MSBuild, you can still use C++/WinRT modules even with C++20. You just need to enable module support in the C++/WinRT options.

### CMake Integration

Starting from CMake 4.3 (the CMake version in current Visual Studio 2026 Insider is 4.2), CMake supports compiling standard library modules. Therefore, you only need to write the following `CMakeLists.txt` to use both `std` modules and C++/WinRT modules:

```cmake
cmake_minimum_required(VERSION 4.3)
project(winrt_module LANGUAGES CXX)

set(CMAKE_CXX_MODULE_STD 1)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CPPWINRT_EXE "cppwinrt" CACHE FILEPATH "Path to cppwinrt executable")
set(CPPWINRT_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cppwinrt")

execute_process(
    COMMAND "${CPPWINRT_EXE}" -input local -output "${CPPWINRT_OUT_DIR}" -modules -verbose
    RESULT_VARIABLE CPPWINRT_RESULT
)
if(NOT CPPWINRT_RESULT EQUAL 0)
    message(FATAL_ERROR "cppwinrt failed with exit code ${CPPWINRT_RESULT}")
endif()

file(GLOB CPPWINRT_MODULES
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CPPWINRT_OUT_DIR}/winrt/*.ixx"
)
list(SORT CPPWINRT_MODULES)

add_executable(main main.cpp)
target_sources(main
    PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES BASE_DIRS "${CPPWINRT_OUT_DIR}/winrt" FILES ${CPPWINRT_MODULES}
)
target_include_directories(main PRIVATE "${CPPWINRT_OUT_DIR}")
target_link_libraries(main PRIVATE runtimeobject synchronization)
```

## Best Practices for C++ Modules

What I want to tell users about implementing C++ modules:

**Use Standard Library Modules:**

If you want to use modules, you should use `std` modules (or `std.compact`). If you write `#include <standard headers>` in the global module fragment, all declarations from the standard library will exist both in that module and in other modules using the same approach. This not only increases module size but also greatly affects compilation efficiency.

When compiling modules, the C++ compiler merges all definitions from the global module fragment, which is extremely time-consuming as it needs to verify they're structurally identical. This is why it's called the global module fragment.

Three major standard libraries actually [support compiling standard library modules in C++20 mode](https://github.com/microsoft/STL/issues/3945), so it's not very reasonable for CMake to restrict this to C++23.

**Avoid modules without names:**

I don't recommend using `import <standard headers>` because it's not part of the Module TS and actually has numerous ambiguous issues. Additionally, I don't recommend using the cl compiler option `/translateIncludes` because I don't believe it can convert existing header projects to modules, and it's certainly not modules itself.

## Performance Analysis

I tested the performance difference between using precompiled headers (PCH) and modules. The test method involved including all header files and importing all modules.

**Build Time and File Size:**

- Building the PCH took 1 minute and 40 seconds
- Building the modules took 2 minutes
- PCH file size: 2.4GB
- Module intermediate files: 480MB

This result is not surprising, as in my C++/WinRT module implementation, the module implementation files require more complex preprocessing, and also need to analyze dependencies. Clearly, this test favors PCH. Moreover, since the PCH includes all declarations, it is expected that the PCH approach will become slower as the number of source files increases. Once the module is precompiled, using it will be super fast.

**Memory Usage:**

When using modules, memory fluctuated between several tens of MB and 300MB, with a final spike reaching 1.1GB. When using PCH, memory gradually grew to 2.5GB during the first minute, then stabilized at 300MB.

### Conclusion

Modules also solve an important problem: they do not export any macros, providing a clean interface. Therefore, even though PCH is slightly faster than modules in the most silly tests, its disadvantages in other areas are enough to outweigh its advantages.
