#pragma once

#include <seven/base/attributes.h>
#include <concepts>
#include <cstdint>
#include <agbabi.h>

struct fixed_type {
    using data_type = std::int32_t;

    static const auto exponent = 16;

    [[gnu::always_inline]]
    constexpr fixed_type() noexcept : data {} {}

    [[gnu::always_inline]]
    constexpr fixed_type(std::integral auto x) noexcept : data { x << exponent } {}

    constexpr fixed_type(std::floating_point auto x) noexcept : data { static_cast<data_type>(x * (1 << exponent)) } {}

    [[gnu::always_inline]]
    static constexpr auto from_raw(const data_type x) noexcept  {
        fixed_type y;
        y.data = x;
        return y;
    }

    explicit operator float() const noexcept {
        return static_cast<float>(data) / (1 << exponent);
    }

    [[gnu::always_inline]]
    constexpr explicit operator int() const noexcept {
        return data >> exponent;
    }

    [[gnu::always_inline]]
    constexpr auto operator +(std::integral auto x) const noexcept {
        return from_raw(data + (x << exponent));
    }

    [[gnu::always_inline]]
    constexpr auto operator +(const fixed_type o) const noexcept {
        return from_raw(data + o.data);
    }

    [[gnu::always_inline]]
    constexpr auto& operator +=(const fixed_type o) noexcept {
        data += o.data;
        return *this;
    }

    [[gnu::always_inline]]
    constexpr auto& operator -=(const fixed_type o) noexcept {
        data -= o.data;
        return *this;
    }

    [[gnu::always_inline]]
    constexpr auto operator -(std::integral auto x) const noexcept {
        return from_raw(data - (x << exponent));
    }

    [[gnu::always_inline]]
    constexpr auto operator -(const fixed_type o) const noexcept {
        return from_raw(data - o.data);
    }

    [[gnu::always_inline]]
    constexpr auto& operator -=(std::integral auto x) noexcept {
        data -= (x << exponent);
        return *this;
    }

    [[gnu::always_inline]]
    constexpr auto operator *(std::integral auto x) const noexcept {
        return from_raw(static_cast<std::int64_t>(data) * x);
    }

    [[gnu::always_inline]]
    constexpr auto operator *(const fixed_type o) const noexcept {
        return from_raw(static_cast<data_type>((static_cast<std::int64_t>(data) * o.data) >> exponent));
    }

    [[gnu::always_inline]]
    constexpr auto operator /(const fixed_type o) const noexcept {
        const auto dataLL = static_cast<std::int64_t>(data) << exponent;
        if (std::is_constant_evaluated()) {
            return from_raw(static_cast<data_type>(dataLL / o.data));
        }
        return from_raw(static_cast<data_type>(__agbabi_uluidiv(dataLL, o.data)));
    }

    [[gnu::always_inline]]
    constexpr auto operator -() const noexcept {
        return from_raw(-data);
    }

    [[gnu::always_inline]]
    constexpr auto operator <<(std::integral auto x) const noexcept {
        return from_raw(data << x);
    }

    [[gnu::always_inline]]
    constexpr auto operator <(std::integral auto x) const noexcept {
        return (data >> exponent) < x;
    }

    [[gnu::always_inline]]
    constexpr auto operator >(std::integral auto x) const noexcept {
        return data > (x << exponent);
    }

    [[gnu::always_inline]]
    constexpr auto operator !() const noexcept {
        return data == 0;
    }

    [[gnu::always_inline]]
    constexpr auto operator <(const fixed_type o) const noexcept {
        return data < o.data;
    }

    [[gnu::always_inline]]
    constexpr auto operator >(const fixed_type o) const noexcept {
        return data > o.data;
    }

    [[gnu::always_inline]]
    constexpr auto operator <=(const fixed_type o) const noexcept {
        return data <= o.data;
    }

    [[gnu::always_inline]]
    constexpr auto operator >=(const fixed_type o) const noexcept {
        return data >= o.data;
    }

    data_type data;
};

[[gnu::always_inline]]
constexpr auto operator +(std::integral auto x, const fixed_type y) noexcept {
    return fixed_type::from_raw((x << fixed_type::exponent) + y.data);
}

[[gnu::always_inline]]
constexpr auto operator -(std::integral auto x, const fixed_type y) noexcept {
    return fixed_type::from_raw((x << fixed_type::exponent) - y.data);
}

[[gnu::always_inline]]
constexpr auto operator *(std::integral auto x, const fixed_type y) noexcept {
    return fixed_type::from_raw(x * y.data);
}

[[gnu::always_inline]]
constexpr auto operator /(std::integral auto x, const fixed_type y) noexcept {
    const auto dataLL = static_cast<std::int64_t>(x) << (fixed_type::exponent * 2);
    if (std::is_signed_v<decltype(x)> || std::is_constant_evaluated()) {
        return fixed_type::from_raw(static_cast<fixed_type::data_type>(dataLL / y.data));
    }
    return fixed_type::from_raw(static_cast<fixed_type::data_type>(__agbabi_uluidiv(dataLL, y.data)));
}

[[gnu::always_inline]]
inline fixed_type fixed_floor(const fixed_type x) noexcept {
    return fixed_type::from_raw(x.data & static_cast<fixed_type::data_type>(0xffffffffu << fixed_type::exponent));
}

[[gnu::always_inline]]
inline fixed_type fixed_abs(const fixed_type x) noexcept {
    return fixed_type::from_raw(x.data < 0 ? -x.data : x.data);
}

[[gnu::always_inline]]
inline fixed_type fixed_frac(const fixed_type x) noexcept {
    return fixed_type::from_raw(x.data & ((1 << fixed_type::exponent) - 1));
}

[[gnu::always_inline]]
inline auto fixed_negative(const fixed_type x) noexcept {
    return x.data < 0;
}
