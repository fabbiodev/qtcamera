#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class IRSensor
{
public:
    using RawFrame = std::vector<std::uint8_t>;

    IRSensor() = default;
    ~IRSensor();

    IRSensor(const IRSensor &) = delete;
    IRSensor &operator=(const IRSensor &) = delete;

    bool start(const std::string &devicePath = "/dev/video0");
    bool readFrame(RawFrame &frame);
    void stop();

    int width() const { return frameWidth_; }
    int height() const { return frameHeight_; }
    const std::string &lastError() const { return lastError_; }

private:
    struct Buffer {
        void *data = nullptr;
        std::size_t size = 0;
    };

    bool configureRawFormat();
    bool mapBuffers();
    bool queueBuffer(std::size_t index);
    bool ioctl(unsigned long request, void *argument) const;
    void unmapBuffers();
    void setError(const std::string &message);

    int fd_ = -1;
    int frameWidth_ = 256;
    int frameHeight_ = 196;
    std::vector<Buffer> buffers_;
    std::string lastError_;
};
