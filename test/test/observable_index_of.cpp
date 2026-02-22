#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

TEST_CASE("observable_index_of")
{
    {
        auto v = single_threaded_observable_vector<int>().as<IVector<IInspectable>>();
        v.Append(box_value(123));
        std::uint32_t index = 0;
        REQUIRE(v.IndexOf(box_value(123), index));
        REQUIRE(!v.IndexOf(nullptr, index));
        REQUIRE(!v.IndexOf(box_value(456), index));
        REQUIRE(!v.IndexOf(Uri(L"http://kennykerr.ca"), index));
    }
    {
        auto value = Uri(L"http://kennykerr.ca");

        auto v = single_threaded_observable_vector<IStringable>().as<IVector<IInspectable>>();
        v.Append(value);
        std::uint32_t index = 0;
        REQUIRE(v.IndexOf(value, index));
        REQUIRE(!v.IndexOf(nullptr, index));
        REQUIRE(!v.IndexOf(box_value(456), index));
        REQUIRE(!v.IndexOf(Uri(L"http://kennykerr.ca"), index));
    }
    {
        auto v = single_threaded_observable_vector<IStringable>().as<IVector<IInspectable>>();
        v.Append(nullptr);
        std::uint32_t index = 0;
        REQUIRE(v.IndexOf(nullptr, index));
        REQUIRE(!v.IndexOf(box_value(456), index));
        REQUIRE(!v.IndexOf(Uri(L"http://kennykerr.ca"), index));
    }
}
