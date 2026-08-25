#include "irsensor.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

constexpr unsigned int kBufferCount = 4;
constexpr int kUsefulWidth = 256;
constexpr int kBufferHeight = 196;

} // namespace

IRSensor::~IRSensor()
{
    stop();
}

void IRSensor::setError(const std::string &message)
{
    lastError_ = message + ": " + std::strerror(errno);
}

bool IRSensor::ioctl(unsigned long request, void *argument) const
{
    int result;
    do {
        result = ::ioctl(fd_, request, argument);
    } while (result == -1 && errno == EINTR);

    return result == 0;
}

bool IRSensor::configureRawFormat()
{
    v4l2_format format{};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = kUsefulWidth;
    format.fmt.pix.height = kBufferHeight;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_NONE;

    if (!ioctl(VIDIOC_S_FMT, &format)) {
        setError("VIDIOC_S_FMT failed");
        return false;
    }

    frameWidth_ = static_cast<int>(format.fmt.pix.width);
    frameHeight_ = static_cast<int>(format.fmt.pix.height);
    return true;
}

bool IRSensor::mapBuffers()
{
    v4l2_requestbuffers request{};
    request.count = kBufferCount;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;

    if (!ioctl(VIDIOC_REQBUFS, &request) || request.count == 0) {
        setError("VIDIOC_REQBUFS failed");
        return false;
    }

    buffers_.resize(request.count);

    for (std::size_t index = 0; index < buffers_.size(); ++index) {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = static_cast<__u32>(index);

        if (!ioctl(VIDIOC_QUERYBUF, &buffer)) {
            setError("VIDIOC_QUERYBUF failed");
            return false;
        }

        void *mapped = ::mmap(nullptr,
                              buffer.length,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED,
                              fd_,
                              static_cast<off_t>(buffer.m.offset));
        if (mapped == MAP_FAILED) {
            setError("mmap failed");
            return false;
        }

        buffers_[index] = {mapped, buffer.length};
    }

    return true;
}

bool IRSensor::queueBuffer(std::size_t index)
{
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = static_cast<__u32>(index);

    if (!ioctl(VIDIOC_QBUF, &buffer)) {
        setError("VIDIOC_QBUF failed");
        return false;
    }

    return true;
}

bool IRSensor::start(const std::string &devicePath)
{
    stop();
    lastError_.clear();

    fd_ = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        setError("Cannot open " + devicePath);
        return false;
    }

    if (!configureRawFormat() || !mapBuffers()) {
        stop();
        return false;
    }

    for (std::size_t index = 0; index < buffers_.size(); ++index) {
        if (!queueBuffer(index)) {
            stop();
            return false;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!ioctl(VIDIOC_STREAMON, &type)) {
        setError("VIDIOC_STREAMON failed");
        stop();
        return false;
    }

    return true;
}

bool IRSensor::readFrame(RawFrame &frame)
{
    if (fd_ < 0 || buffers_.empty()) {
        return false;
    }

    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (!ioctl(VIDIOC_DQBUF, &buffer)) {
        if (errno == EAGAIN) {
            return false;
        }

        setError("VIDIOC_DQBUF failed");
        return false;
    }

    if (buffer.index >= buffers_.size()) {
        setError("Camera returned an invalid buffer index");
        return false;
    }

    const auto *begin = static_cast<const std::uint8_t *>(buffers_[buffer.index].data);
    frame.assign(begin, begin + buffer.bytesused);
    return queueBuffer(buffer.index);
}

void IRSensor::unmapBuffers()
{
    for (const Buffer &buffer : buffers_) {
        if (buffer.data != nullptr && buffer.data != MAP_FAILED) {
            ::munmap(buffer.data, buffer.size);
        }
    }

    buffers_.clear();
}

void IRSensor::stop()
{
    if (fd_ < 0) {
        return;
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ::ioctl(fd_, VIDIOC_STREAMOFF, &type);
    unmapBuffers();
    ::close(fd_);
    fd_ = -1;
}
