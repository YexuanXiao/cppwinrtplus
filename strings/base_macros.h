
#ifdef _DEBUG

#define WINRT_ASSERT _ASSERTE
#define WINRT_VERIFY WINRT_ASSERT
#define WINRT_VERIFY_(result, expression) WINRT_ASSERT(result == expression)

#else

#define WINRT_ASSERT(expression) ((void)0)
#define WINRT_VERIFY(expression) (void)(expression)
#define WINRT_VERIFY_(result, expression) (void)(expression)

#endif

#define WINRT_IMPL_SHIM(...) (*(abi_t<__VA_ARGS__>**)&static_cast<__VA_ARGS__ const&>(static_cast<D const&>(*this)))

#ifdef _MSC_VER // T
// Note: this is a workaround for a false-positive warning produced by the Visual C++ 15.9 compiler.
#pragma warning(disable : 5046)

// Note: this is a workaround for a false-positive warning produced by the Visual C++ 16.3 compiler.
#pragma warning(disable : 4268)
#endif

// Transition: compatibility
#ifndef WINRT_MODULE

#ifndef WINRT_EXPORT
#define WINRT_EXPORT
#endif

#endif

// <windowsnumerics.impl.h> pulls in large, hard-to-control legacy headers. In header builds we keep the
// existing behavior, but in module builds it's provided by the winrt.numerics module.
#ifndef WINRT_MODULE

#ifdef WINRT_IMPL_NUMERICS
#define _WINDOWS_NUMERICS_NAMESPACE_ winrt::Windows::Foundation::Numerics
#define _WINDOWS_NUMERICS_BEGIN_NAMESPACE_ WINRT_EXPORT namespace winrt::Windows::Foundation::Numerics
#define _WINDOWS_NUMERICS_END_NAMESPACE_
#include <windowsnumerics.impl.h>
#undef _WINDOWS_NUMERICS_NAMESPACE_
#undef _WINDOWS_NUMERICS_BEGIN_NAMESPACE_
#undef _WINDOWS_NUMERICS_END_NAMESPACE_
#endif

#endif // WINRT_MODULE

#if defined(_MSC_VER)
#define WINRT_IMPL_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define WINRT_IMPL_NOINLINE __attribute__((noinline))
#else
#define WINRT_IMPL_NOINLINE
#endif

#if defined(_MSC_VER)
#define WINRT_IMPL_EMPTY_BASES __declspec(empty_bases)
#else
#define WINRT_IMPL_EMPTY_BASES
#endif

#if defined(_MSC_VER)
#define WINRT_IMPL_NOVTABLE __declspec(novtable)
#else
#define WINRT_IMPL_NOVTABLE
#endif

#if defined(__clang__) && defined(__has_attribute)
#if __has_attribute(__lto_visibility_public__)
#define WINRT_IMPL_PUBLIC __attribute__((lto_visibility_public))
#else
#define WINRT_IMPL_PUBLIC
#endif // __has_attribute(__lto_visibility_public__)
#else
#define WINRT_IMPL_PUBLIC
#endif

#define WINRT_IMPL_ABI_DECL WINRT_IMPL_NOVTABLE WINRT_IMPL_PUBLIC

#if defined(__clang__)
#define WINRT_IMPL_HAS_DECLSPEC_UUID __has_declspec_attribute(uuid)
#elif defined(_MSC_VER)
#define WINRT_IMPL_HAS_DECLSPEC_UUID 1
#else
#define WINRT_IMPL_HAS_DECLSPEC_UUID 0
#endif

#ifdef __IUnknown_INTERFACE_DEFINED__
#define WINRT_IMPL_IUNKNOWN_DEFINED
#else
// Forward declare so we can talk about it.
struct IUnknown;
typedef struct _GUID GUID;
#endif

#if defined(__cpp_consteval)
#define WINRT_IMPL_CONSTEVAL consteval
#else
#define WINRT_IMPL_CONSTEVAL constexpr
#endif

