#include "LGpioPin.hpp"

LGpioPin::LGpioPin(const std::string &chipPath, int lineOffset) {}

LGpioPin::~LGpioPin() {}

bool LGpioPin::setDirection(Direction direction)
{
    return false;
}

void LGpioPin::handleEpollEvent(uint32_t events) {}
