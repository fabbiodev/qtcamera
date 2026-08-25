#pragma once

#include <cstdint>
#include <string>
#include <vector>

class IRSensor
{
public:
    using RawFrame = std::vector<std::uint8_t>;
    ~IRSensor();
    bool start(const std::string &device = "/dev/video0");
    bool readFrame(RawFrame &frame);
    void stop();
    int width() const { return width_; }
    int height() const { return height_; }
    const std::string &error() const { return error_; }

private:
    struct Buffer { void *data{}; std::size_t size{}; };
    bool call(unsigned long request, void *arg) const;
    bool queue(std::size_t index);
    void fail(const char *message);
    void releaseBuffers();
    int fd_ = -1;
    int width_ = 256;
    int height_ = 196;
    std::vector<Buffer> buffers_;
    std::string error_;
};
