#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <cstdint>
#include <string>


/**
 * A class for decoding video frames from a video file using FFmpeg.
 *
 * The VideoDecoder class provides functionality to open a video file, decode video frames,
 * convert frames to RGB format, retrieve timestamps, and perform precise seeking within the
 * video stream. It manages the lifecycle of the underlying FFmpeg structures and provides a
 * simple interface for frame retrieval.
 */
class VideoDecoder {
    AVFormatContext *m_format_context;
    AVCodecContext *m_codec_context;

    int m_video_stream_index;
    AVPacket *m_packet;
    AVFrame *m_frame;

public:

    /**
     * Constructs a VideoDecoder object and initializes the decoding context for a specified video file.
     *
     * The `VideoDecoder` constructor opens the specified video file, retrieves stream information, and
     * initializes the necessary FFmpeg structures for video decoding. It identifies the video stream
     * within the file and prepares the decoder for subsequent frame retrieval operations.
     *
     * @param path The path to the video file to be decoded.
     *
     * @throws std::runtime_error If the video file cannot be opened.
     * @throws std::runtime_error If the stream information cannot be retrieved.
     * @throws std::runtime_error If no video stream is found in the file.
     * @throws std::runtime_error If the video codec is unsupported.
     * @throws std::runtime_error If the video codec context cannot be allocated.
     * @throws std::runtime_error If the codec parameters cannot be copied to the codec context.
     * @throws std::runtime_error If the video codec cannot be opened.
     * @throws std::runtime_error If the packet allocation fails.
     * @throws std::runtime_error If the frame allocation fails.
     */
    explicit VideoDecoder(const std::string &path);

    /**
     * Destructs the VideoDecoder object, releasing all associated resources.
     */
    ~VideoDecoder();

    /**
     * Returns the width of the video stream in pixels.
     *
     * @return Width of the video stream.
     */
    [[nodiscard]] int getWidth() const;

    /**
     * Returns the height of the video stream in pixels.
     *
     * @return Height of the video stream.
     */
    [[nodiscard]] int getHeight() const;

    /**
     * Returns the total duration of the video stream in microseconds.
     *
     * @return Total duration of the video stream in microseconds.
     */
    [[nodiscard]] int64_t getTotalDuration() const;

    /**
     * Returns the frames per second (FPS) of the video stream.
     *
     * @return The frames per second (FPS) of the video stream.
     */
    [[nodiscard]] double getFPS() const;

    /**
     * Returns the bitrate of the video stream in bits per second (bps).
     *
     * @return The bitrate of the video stream in bits per second (bps).
     */
    [[nodiscard]] int64_t getBitrate() const;

    /**
     * Decodes the next video m_frame, converts it to an RGB buffer, and retrieves its presentation timestamp.
     *
     * @param rgb_buffer Pre-allocated buffer to store the RGB data of the decoded m_frame.
     * @param pts Pointer (can be null) to store the presentation timestamp (PTS) of the decoded m_frame in microseconds.
     * @return `true` if a m_frame is successfully decoded and converted, `false` on end of stream or error.
     */
    bool getNextFrame(uint8_t *rgb_buffer, int64_t *pts = nullptr);

    /**
     * Seeks to the nearest keyframe at or before the specified timestamp, then decodes frames
     * until the exact timestamp is reached.
     *
     * @param timestamp_in_microseconds The target timestamp in microseconds to seek to.
     * @return true if the desired timestamp is reached successfully, returns false otherwise.
     */
    bool seekToTimestamp(int64_t timestamp_in_microseconds);
   

private:

    /**
     * Returns a pointer to the demuxed video stream. This method is intended to be used
     * with getBestEffortTimestampInMicroseconds(...) method.
     */
    [[nodiscard]] const AVStream *getStream() const;

    /**
     * Decodes and retrieves the next video m_frame from the stream. The decoder has the ownership of the
     * decoded m_frame and the memory will be overwritten by the next m_frame when this function is called again.
     *
     * @param out_frame Pointer to the decoded AVFrame. The pointer is set to nullptr in case of an error
     * (i.e. when the function returns false)
     * @return `true` if a m_frame is successfully decoded and retrieved, `false` on end of stream or error.
     */
    bool getNextFrame(AVFrame **out_frame);

     /**
     * Converts an AVFrame into a RGB buffer.
     * @param frame AVFrame to convert into RGB buffer.
     * @param rgb_buffer pre-allocated buffer of size (m_frame->width * m_frame->height * 3) to
     * write the converted RGB data into.
     */
    static void convertAVFrameToRGBBuffer(const AVFrame *frame, uint8_t *rgb_buffer);

    /**
     * Returns the best effort timestamp of an AVFrame in microseconds.
     * @param frame m_frame to get the best effort timestamp of.
     * @param stream stream the m_frame belongs to. Can get received by calling VideoDecoder::getStream()
     * on the same decoder object you received the m_frame from.
     */
    static int64_t getBestEffortTimestampInMicroseconds(const AVFrame *frame, const AVStream *stream);
};
