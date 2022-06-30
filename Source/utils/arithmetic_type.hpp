#pragma once

#include <cstdint>

#include <limits>
#include <utility>
#include <algorithm>
#include <type_traits>


namespace devilution {
namespace utils {
namespace arithmetic_type {

class MakeExtended final {
    template <class T> struct Wrapper_ final {
        using Type = ::std::decay_t<T>;
        static_assert(::std::is_same_v<T, Type>);
        static_assert(::std::is_arithmetic_v<Type>);
    };

    template <class T> inline constexpr static auto signed_() {
        static_assert(::std::is_signed_v<T>);
        constexpr auto const limit = ::std::numeric_limits<T>::max();
        if constexpr (limit < ::std::numeric_limits<::std::int8_t>::max()) return Wrapper_<::std::int8_t>{};
        else if constexpr (limit < ::std::numeric_limits<::std::int16_t>::max()) return Wrapper_<::std::int16_t>{};
        else if constexpr (limit < ::std::numeric_limits<::std::int32_t>::max()) return Wrapper_<::std::int32_t>{};
        else if constexpr (limit < ::std::numeric_limits<::std::int64_t>::max()) return Wrapper_<::std::int64_t>{};
        else if constexpr (limit < ::std::numeric_limits<signed long long>::max()) return Wrapper_<signed long long>{};
        else {
            constexpr auto const cannot_extend_type = static_cast<T>(1) + ::std::numeric_limits<T>::max();
            return Wrapper_<::std::conditional_t<(limit > ::std::numeric_limits<signed long long>::max()), T, signed long long>>{};
        }
    }

    template <class T> inline constexpr static auto unsigned_() {
        static_assert(not ::std::is_signed_v<T>);
        constexpr auto const wrapper = signed_<::std::make_signed_t<T>>();
        using Signed = typename ::std::decay_t<decltype(wrapper)>::Type;
        return Wrapper_<::std::make_unsigned_t<Signed>>{};
    }

    template <class T> inline constexpr static auto routine_() {
        using Type = typename Wrapper_<T>::Type;
        if constexpr (::std::is_signed_v<Type>) return signed_<Type>();
        else return unsigned_<Type>();
    }

public:
    template <class T> inline constexpr static auto routine() { return routine_<T>(); }
    template <class T> using Type = typename ::std::decay_t<decltype(routine<T>())>::Type;
};

template <class T> using Extended = typename MakeExtended::Type<T>;

class MakeMixed final {
    template <class T> struct Wrapper_ final {
        using Type = ::std::decay_t<T>;
        static_assert(::std::is_same_v<T, Type>);
        static_assert(::std::is_arithmetic_v<Type>);
    };

    template <class firstT, class secondT> inline constexpr static auto widest_() {
        using First = typename Wrapper_<firstT>::Type;
        using Second = typename Wrapper_<secondT>::Type;
        if constexpr (::std::is_same_v<First, Second>) return Wrapper_<First>{};
        else if constexpr(::std::is_floating_point_v<First> or ::std::is_floating_point_v<Second>) return Wrapper_<::std::common_type_t<First, Second>>{};
        else {
            using SignedFirst = ::std::make_signed_t<First>;
            using SignedSecond = ::std::make_signed_t<Second>;
            if constexpr (::std::is_same_v<SignedFirst, SignedSecond>) return Wrapper_<Extended<SignedFirst>>{};
            else {
                using Common = ::std::common_type_t<First, Second>;
                constexpr auto const first = static_cast<Common>(::std::numeric_limits<First>::max());
                constexpr auto const second = static_cast<Common>(::std::numeric_limits<Second>::max());
                if constexpr (first < second) return Wrapper_<::std::conditional_t<
                    (::std::is_signed_v<Second> or (not ::std::is_signed_v<First>)), Second, Extended<SignedSecond>
                >>{};
                else {
                    static_assert(second < first);
                    return Wrapper_<::std::conditional_t<
                        (::std::is_signed_v<First> or (not ::std::is_signed_v<Second>)), First, Extended<SignedFirst>
                    >>{};
                }
            }
        }
    }

    template <class T, class ... tailT> inline constexpr static auto routine_() {
        if constexpr (not (0 < sizeof ... (tailT))) return Wrapper_<T>{};
        else return widest_<T, typename ::std::decay_t<decltype(routine_<tailT ...>())>::Type>();
    }

public:
    template <class ... T> inline constexpr static auto routine() { return routine_<T ...>(); }
    template <class ... T> using Type = typename ::std::decay_t<decltype(routine<T ...>())>::Type;
};

template <class ... T> using Mixed = typename MakeMixed::Type<T ...>;

template <class limitT, class valueT> inline constexpr static auto make_limited(valueT &&value) noexcept(true) {
    using Limit = ::std::decay_t<limitT>;
    static_assert(::std::is_arithmetic_v<Limit>);

    using Value = ::std::decay_t<valueT>;
    static_assert(::std::is_arithmetic_v<Value>);

    using Common = Mixed<Limit, Value>;
    constexpr auto const minimum = static_cast<Common>(::std::numeric_limits<Limit>::min());
    constexpr auto const maximum = static_cast<Common>(::std::numeric_limits<Limit>::max());

    return static_cast<Limit>(::std::max(minimum, ::std::min(maximum, static_cast<Common>(::std::forward<valueT>(value)))));
}

} // namespace arithmetic_type_utils
} // namespace utils
} // namespace devilution
