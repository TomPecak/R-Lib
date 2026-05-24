#pragma once

#include <functional>
#include <string>

#include "LEventLoop.hpp"

class LGpioPin : public LEpollHandler
{
public:
    enum Direction { Input, Output };
    enum Value { Low = 0, High = 1 };
    enum Edge { NoEdge, RisingEdge, FallingEdge, BothEdges };
    enum DriveMode { PullNone, PullUp, PullDown };

    enum class Error {
        NoError,
        PermissionError,
        OpenDeviceError,
        LineRequestError,
        ReadError,
        WriteError,
        UnknownError
    };

    LGpioPin(const std::string &chipPath, int lineOffset);
    ~LGpioPin() override;

    bool open();
    void close();
    bool isOpen() const;

    unsigned int offset() const;

    Direction direction() const;
    bool setDirection(Direction direction);

    Value value() const;
    void setValue(LGpioPin::Value value);
    void setValue(bool value);
    void toggle();

    Edge edge() const;
    bool setEdge(Edge edge);

    DriveMode driveMode() const;
    bool setDriveMode(DriveMode mode);

    bool isInverted() const;
    void setInverted(bool inverted);

    Error error() const;

    //Callbacks
    void onValueChanged(LGpioPin::Value newValue);
    void onErrorOccurred(LGpioPin::Error error);

protected:
    void handleEpollEvent(uint32_t events) override;

private:
    std::string m_chip;
    int m_lineOffset;
    int m_fd;
    Direction m_direction;
    Edge m_edge;
    std::function<void(Value)> m_valueCallback;
    std::function<void(Error)> m_errorCallback;
};
