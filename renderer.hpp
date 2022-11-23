#pragma once

#include <seven/base/attributes.h>
#include "camera.hpp"

namespace renderer {

void initialize();
void draw(const camera_type& camera) IWRAM_CODE ARM_CODE;
void flip();

}
