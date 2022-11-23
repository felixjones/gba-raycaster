#pragma once

#include <array>

template <std::size_t Length, typename Generator>
constexpr auto lut(Generator&& f){
    using content_type = decltype(f(std::size_t{0}));
    std::array<content_type, Length> arr {};

    for(std::size_t i = 0; i < Length; i++){
        arr[i] = f(i);
    }

    return arr;
}
