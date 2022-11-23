#pragma once

#include "fixed.hpp"

struct camera_type {
    static constexpr auto move_speed = fixed_type { 1.0 / 16.0 };
    static constexpr auto strafe_speed = fixed_type { 1.0 / 32.0 };
    static const auto turn_speed = 0x80;

    camera_type(fixed_type startX, fixed_type startY, std::int32_t startAngle);

    void update();

    fixed_type x;
    fixed_type y;
    std::int32_t angle {};

    [[nodiscard]]
    auto& dir_x() const noexcept {
        return dirX;
    }

    [[nodiscard]]
    auto& dir_y() const noexcept {
        return dirY;
    }

private:
    fixed_type dirX;
    fixed_type dirY;
};
