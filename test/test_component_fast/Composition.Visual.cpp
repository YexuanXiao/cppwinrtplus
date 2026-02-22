#include "pch.h"
#include "Composition.Visual.h"
#include "Composition.Visual.g.cpp"

namespace winrt::test_component_fast::Composition::implementation
{
    Visual::Visual(Composition::Compositor const& compositor) :
        base_type(compositor)
    {
    }
    void Visual::Offset(std::int32_t value)
    {
        m_offset = value;
    }
    std::int32_t Visual::Offset()
    {
        return m_offset;
    }
    void Visual::ParentForTransform([[maybe_unused]] Composition::Visual const& value)
    {
    }
}
