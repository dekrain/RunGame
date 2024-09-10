#pragma once

#include <type_traits>

template <typename From, typename To>
struct copy_cv {
    using type = To;
};

template <typename From, typename To>
struct copy_cv<From const, To> {
    using type = To const;
};

template <typename From, typename To>
struct copy_cv<From volatile, To> {
    using type = To volatile;
};

template <typename From, typename To>
struct copy_cv<From const volatile, To> {
    using type = To const volatile;
};

template <typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

template <typename T>
copy_cv_t<T, std::underlying_type_t<std::remove_cv_t<T>>>& enum_ref_cast(T& en) {
    return reinterpret_cast<copy_cv_t<T, std::underlying_type_t<std::remove_cv_t<T>>>&>(en);
}
