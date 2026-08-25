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
constexpr int kWidth = 256;
constexpr int kHeight = 196;
}

IRSensor::~IRSensor() { stop(); }

bool IRSensor::call(unsigned long request, void *arg) const
{
    int result;
    do { result = ::ioctl(fd_, request, arg); } while (result == -1 && errno == EINTR);
    return result == 0;
}

void IRSensor::fail(const char *message)
{
    error_ = std::string(message) + ": " + std::strerror(errno);
}

bool IRSensor::queue(std::size_t index)
{
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = static_cast<__u32>(index);
    if (!call(VIDIOC_QBUF, &buffer)) { fail("VIDIOC_QBUF failed"); return false; }
    return true;
}

bool IRSensor::start(const std::string &device)
{
    stop();
    error_.clear();
    fd_ = ::open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) { fail("Cannot open camera"); return false; }

    v4l2_format format{};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = kWidth;
    format.fmt.pix.height = kHeight;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if (!call(VIDIOC_S_FMT, &format)) { fail("VIDIOC_S_FMT failed"); stop(); return false; }
    width_ = static_cast<int>(format.fmt.pix.width);
    height_ = static_cast<int>(format.fmt.pix.height);

    v4l2_requestbuffers request{};
    request.count = kBufferCount;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (!call(VIDIOC_REQBUFS, &request) || request.count == 0) {
        fail("VIDIOC_REQBUFS failed"); stop(); return false;
    }

    buffers_.resize(request.count);
    for (std::size_t i = 0; i < buffers_.size(); ++i) {
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = static_cast<__u32>(i);
        if (!call(VIDIOC_QUERYBUF, &buffer)) { fail("VIDIOC_QUERYBUF failed"); stop(); return false; }
        void *data = ::mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd_, buffer.m.offset);
        if (data == MAP_FAILED) { fail("mmap failed"); stop(); return false; }
        buffers_[i] = {data, buffer.length};
        if (!queue(i)) { stop(); return false; }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (!call(VIDIOC_STREAMON, &type)) { fail("VIDIOC_STREAMON failed"); stop(); return false; }
    return true;
}

bool IRSensor::readFrame(RawFrame &frame)
{
    if (fd_ < 0) return false;
    v4l2_buffer buffer{};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    if (!call(VIDIOC_DQBUF, &buffer)) {
        if (errno == EAGAIN) return false;
        fail("VIDIOC_DQBUF failed");
        return false;
    }
    if (buffer.index >= buffers_.size()) { fail("Invalid camera buffer index"); return false; }
    const auto *data = static_cast<const std::uint8_t *>(buffers_[buffer.index].data);
    frame.assign(data, data + buffer.bytesused);
    return queue(buffer.index);
}

void IRSensor::releaseBuffers()
{
    for (const Buffer &buffer : buffers_)
        if (buffer.data) ::munmap(buffer.data, buffer.size);
    buffers_.clear();
}

void IRSensor::stop()
{
    if (fd_ < 0) return;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ::ioctl(fd_, VIDIOC_STREAMOFF, &type);
    releaseBuffers();
    ::close(fd_);
    fd_ = -1;
}
