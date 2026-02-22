#pragma once
#include "Composition.Visual.g.h"
#include "Composition.CompositionObject.h"

namespace winrt::test_component_fast::Composition::implementation
{
    struct Visual : VisualT<Visual, implementation::CompositionObject>
    {
        Visual(Composition::Compositor const& compositor);

        std::int32_t Offset();
        void Offset(std::int32_t value);
        void ParentForTransform(Composition::Visual const& value);

    private:

        std::int32_t m_offset{};
    };
}
