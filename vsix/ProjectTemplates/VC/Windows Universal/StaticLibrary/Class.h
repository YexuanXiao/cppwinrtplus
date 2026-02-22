#pragma once

#include "Class.g.h"

namespace winrt::$safeprojectname$::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        std::int32_t MyProperty();
        void MyProperty(std::int32_t value);
    };
}

namespace winrt::$safeprojectname$::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
