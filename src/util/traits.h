#ifndef KUN_UTIL_TRAITS_H
#define KUN_UTIL_TRAITS_H

#include <wchar.h>

#include <type_traits>

namespace kun {

template<typename T>
inline constexpr bool is_bool = std::is_same_v<std::remove_cv_t<T>, bool>;

template<typename T>
inline constexpr bool is_char = std::is_same_v<std::remove_cv_t<T>, char>;

template<typename T>
inline constexpr bool is_wchar = std::is_same_v<std::remove_cv_t<T>, wchar_t>;

template<typename T>
inline constexpr bool is_c_str =
    std::is_same_v<std::remove_cv_t<T>, char[sizeof(T)]> ||
    std::is_same_v<std::remove_cv_t<T>, const char*> ||
    std::is_same_v<std::remove_cv_t<T>, char*>;

template<typename T>
inline constexpr bool is_wc_str =
    std::is_same_v<std::remove_cv_t<T>, wchar_t[sizeof(T)]> ||
    std::is_same_v<std::remove_cv_t<T>, const wchar_t*> ||
    std::is_same_v<std::remove_cv_t<T>, wchar_t*>;

template<typename T>
inline constexpr bool is_number =
    !kun::is_bool<T> &&
    (std::is_integral_v<T> || std::is_floating_point_v<T>);

}

#endif
