import Windows.Foundation;

int main()
{
    winrt::init_apartment();

    winrt::hstring value = L"Hello";
    (void)value;

    winrt::Windows::Foundation::Uri uri{ L"https://example.com" };
    (void)uri;

    return 0;
}
