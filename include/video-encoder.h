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
 * A class for encoding video frames into a video file using FFmpeg.
 *
 * The `VideoEncoder` class provides functionality to create and configure a video
 * encoder, encode RGB frames into a video stream, and finalize the video file. It
 * manages the lifecycle of the underlying FFmpeg structures and provides a simple
 * interface for encoding video frames.
 */
class VideoEncoder {
    AVFormatContext *m_format_context;
    AVCodecContext *m_codec_context;
    AVStream *m_stream;
    AVFrame *m_frame;
    AVPacket *m_packet;
    SwsContext *m_sws_context;
    bool m_finalized;
    int64_t m_pts;

public:
    /**
     * Initializes the encoder with the specified parameters.
     *
     * @param filepath Path to the output video file.
     * @param width Width of the output video in pixels.
     * @param height Height of the output video in pixels.
     * @param fps Frames per second of the output video.
     * @param bitrate Bitrate of the output video in bits per second.
     *
     * @throws std::runtime_error If the output format context cannot be allocated.
     * @throws std::runtime_error If the output file cannot be opened.
     * @throws std::runtime_error If the encoder cannot be found.
     * @throws std::runtime_error If a new stream cannot be created.
     * @throws std::runtime_error If the codec context cannot be allocated.
     * @throws std::runtime_error If the codec cannot be opened.
     * @throws std::runtime_error If the frame or packet allocation fails.
     */
    explicit VideoEncoder(const std::string &filepath, int width, int height, double fps, int64_t bitrate);

    /**
     * Destructor to ensure proper cleanup and finalization of the encoding process.
     * Calls finalize() to ensure that the file is correctly written before cleanup.
     */
    ~VideoEncoder();

    /**
     * Encodes an RGB frame with specific width and height (expected buffer size: width x height x 3 bytes).
     * The input frame is converted to YUV and resized to output video dimensions as needed before encoding.
     *
     * @param rgb_buffer Pointer to the RGB buffer representing the frame to be encoded.
     * @param width Width of the input frame in pixels.
     * @param height Height of the input frame in pixels.
     *
     * @throws std::runtime_error If the frame cannot be encoded or if any error occurs during conversion.
     */
    void encodeFrame(const uint8_t *rgb_buffer, int width, int height);

    /**
     * Finalizes the encoding process, ensuring that the output file is written correctly.
     * This method flushes the encoder, writes the trailer, and cleans up FFmpeg structures.
     *
     * @throws std::runtime_error If an error occurs while finalizing the encoding process.
     */
    void finalize();

private:

    /**
     * Encodes a YUV frame (as an AVFrame) without any colorspace conversion. However,
     * the frame is resized to output video dimensions as needed before encoding.
     *
     * @param frame Pointer to the AVFrame representing the YUV frame to be encoded.
     */
    void encodeFrame(AVFrame* frame);
};
