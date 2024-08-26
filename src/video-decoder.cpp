#include "video-decoder.h"


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
VideoDecoder::VideoDecoder(const std::string &path) {

    // Open input file.
    m_format_context = nullptr;
    if (avformat_open_input(&m_format_context, path.c_str(), nullptr, nullptr) != 0) {
        throw std::runtime_error("couldn't open file");
    }

    // Retrieve stream information.
    if (avformat_find_stream_info(m_format_context, nullptr) < 0) {
        avformat_close_input(&m_format_context);
        throw std::runtime_error("couldn't retrieve stream information");
    }

    // Find video stream index.
    m_video_stream_index = -1;
    for (unsigned int i = 0; i < m_format_context->nb_streams; i++) {
        if (m_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_video_stream_index = static_cast<int>(i);
        }
    }
    if (m_video_stream_index == -1) {
        avformat_close_input(&m_format_context);
        throw std::runtime_error("couldn't find a video stream");
    }

    // Get a pointer to the codec context for the video stream.
    AVCodecParameters *codec_parameters = m_format_context->streams[m_video_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
    if (!codec) {
        avformat_close_input(&m_format_context);
        throw std::runtime_error("unsupported video codec");
    }

    m_codec_context = avcodec_alloc_context3(codec);
    if (!m_codec_context) {
        avformat_close_input(&m_format_context);
        throw std::runtime_error("couldn't allocate video codec context");
    }

    if (avcodec_parameters_to_context(m_codec_context, codec_parameters) < 0) {
        avcodec_free_context(&m_codec_context);
        avformat_close_input(&m_format_context);
        throw std::runtime_error("couldn't copy video codec context");
    }

    if (avcodec_open2(m_codec_context, codec, nullptr) < 0) {
        avcodec_free_context(&m_codec_context);
        avformat_close_input(&m_format_context);
        throw std::runtime_error("couldn't open video codec");
    }

    // Allocate m_packet.
    m_packet = av_packet_alloc();
    if (!m_packet) {
        avcodec_free_context(&m_codec_context);
        avformat_close_input(&m_format_context);
        throw std::runtime_error("couldn't allocate m_packet");
    }

    // Allocate m_frame.
    m_frame = av_frame_alloc();
    if (!m_frame) {
        avcodec_free_context(&m_codec_context);
        avformat_close_input(&m_format_context);
        av_packet_free(&m_packet);
        throw std::runtime_error("couldn't allocate m_frame");
    }
}


/**
 * Destructs the VideoDecoder object, releasing all associated resources.
 */
VideoDecoder::~VideoDecoder() {
    avcodec_free_context(&m_codec_context);
    avformat_close_input(&m_format_context);
    av_packet_free(&m_packet);
    av_frame_free(&m_frame);
}

/**
 * Returns the width of the video stream in pixels.
 *
 * @return Width of the video stream.
 */
[[nodiscard]] int VideoDecoder::getWidth() const { return m_codec_context->width; }

/**
 * Returns the height of the video stream in pixels.
 *
 * @return Height of the video stream.
 */
[[nodiscard]] int VideoDecoder::getHeight() const { return m_codec_context->height; }

/**
 * Returns the total duration of the video stream in microseconds.
 *
 * @return Total duration of the video stream in microseconds.
 */
[[nodiscard]] int64_t VideoDecoder::getTotalDuration() const {
    return av_rescale_q(
        getStream()->duration,
        getStream()->time_base,
        {1, AV_TIME_BASE}
    );
}


/**
 * Returns a pointer to the demuxed video stream. This method is intended to be used
 * with getBestEffortTimestampInMicroseconds(...) method.
 */
[[nodiscard]] const AVStream* VideoDecoder::getStream() const { return m_format_context->streams[m_video_stream_index]; }


/**
 * Returns the frames per second (FPS) of the video stream.
 *
 * @return The frames per second (FPS) of the video stream.
 */
[[nodiscard]] double VideoDecoder::getFPS() const {
    AVStream* stream = m_format_context->streams[m_video_stream_index];
    AVRational framerate = av_guess_frame_rate(m_format_context, stream, nullptr);
    return av_q2d(framerate);
}


/**
 * Returns the bitrate of the video stream in bits per second (bps).
 *
 * @return The bitrate of the video stream in bits per second (bps).
 */
[[nodiscard]] int64_t VideoDecoder::getBitrate() const {
    AVStream* stream = m_format_context->streams[m_video_stream_index];
    return stream->codecpar->bit_rate;
}


/**
 * Decodes and retrieves the next video m_frame from the stream. The decoder has the ownership of the
 * decoded m_frame and the memory will be overwritten by the next m_frame when this function is called again.
 *
 * @param out_frame Pointer to the decoded AVFrame. The pointer is set to nullptr in case of an error
 * (i.e. when the function returns false)
 * @return `true` if a m_frame is successfully decoded and retrieved, `false` on end of stream or error.
 */
bool VideoDecoder::getNextFrame(AVFrame **out_frame) {
    int ret;
    static bool hasPendingFrames = false;

    while (true) {

        // If there are no pending frames, read a new m_packet.
        if (!hasPendingFrames) {
            ret = av_read_frame(m_format_context, m_packet);
            if (ret == AVERROR_EOF) {

                // End of file, send NULL m_packet to flush the decoder.
                avcodec_send_packet(m_codec_context, nullptr);
            } else if (ret < 0) {

                // Error occurred.
                return false;
            } else if (m_packet->stream_index == m_video_stream_index) {

                // Send the m_packet to the decoder.
                ret = avcodec_send_packet(m_codec_context, m_packet);
                if (ret < 0) {

                    // Error sending m_packet to decoder.
                    return false;
                }
            }
        }

        // Loop to receive all frames that may be produced from the current m_packet.
        while (true) {
            ret = avcodec_receive_frame(m_codec_context, m_frame);
            if (ret == AVERROR(EAGAIN)) {

                // No more frames available in the current m_packet.
                hasPendingFrames = false;
                break;
            } else if (ret == AVERROR_EOF) {

                // End of stream, stop processing.
                hasPendingFrames = false;
                return false;
            } else if (ret < 0) {

                // Error occurred while receiving an m_frame.
                return false;
            }

            // Return decoded m_frame through the output parameter.
            *out_frame = m_frame;

            // Mark that there might be more frames available.
            hasPendingFrames = true;

            // Return true as we have successfully decoded and processed an m_frame.
            return true;
        }
    }
}


/**
 * Decodes the next video m_frame, converts it to an RGB buffer, and retrieves its presentation timestamp.
 *
 * @param rgb_buffer Pre-allocated buffer to store the RGB data of the decoded m_frame.
 * @param pts Pointer (can be null) to store the presentation timestamp (PTS) of the decoded m_frame in microseconds.
 * @return `true` if a m_frame is successfully decoded and converted, `false` on end of stream or error.
 */
bool VideoDecoder::getNextFrame(uint8_t *rgb_buffer, int64_t *pts) {

    AVFrame *frame = nullptr;
    bool frame_received = getNextFrame(&frame);

    // If the m_frame has been successfully retrieved, convert it to RGB
    // and retrieve its presentation timestamp (PTS).
    if (frame_received) {
        try {
            convertAVFrameToRGBBuffer(frame, rgb_buffer);
        } catch (const std::runtime_error &e) {
            throw e;
        }

        if (pts) {
            *pts = getBestEffortTimestampInMicroseconds(frame, getStream());
        }

        return true;
    }

    return false;
}

/**
 * Seeks to the nearest keyframe at or before the specified timestamp, then decodes frames
 * until the exact timestamp is reached.
 *
 * @param timestamp_in_microseconds The target timestamp in microseconds to seek to.
 * @return true if the desired timestamp is reached successfully, returns false otherwise.
 */
bool VideoDecoder::seekToTimestamp(int64_t timestamp_in_microseconds) {

    // Convert timestamp into the video stream's time base unit.
    int64_t timestamp_in_time_base = av_rescale_q(
            timestamp_in_microseconds,
            AV_TIME_BASE_Q,
            m_format_context->streams[m_video_stream_index]->time_base
    );

    // Flush the codec to clear any pending frames.
    avcodec_flush_buffers(m_codec_context);

    // Seek to the nearest keyframe before or at the target timestamp_in_microseconds.
    if (av_seek_frame(m_format_context, m_video_stream_index, timestamp_in_time_base, AVSEEK_FLAG_BACKWARD) < 0) {

        // Seek failed.
        return false;
    }

    // Decode frames until we reach the desired timestamp_in_microseconds.
    AVFrame *frame;
    while (getNextFrame(&frame)) {
        int64_t currentTimestamp = getBestEffortTimestampInMicroseconds(frame, getStream());
        if (currentTimestamp >= timestamp_in_microseconds) {

            // Successfully reached the desired timestamp_in_microseconds.
            return true;
        }
    }

    // Failed to reach the desired timestamp_in_microseconds.
    return false;
}

/**
 * Converts an AVFrame into a RGB buffer.
 * @param frame AVFrame to convert into RGB buffer.
 * @param rgb_buffer pre-allocated buffer of size (m_frame->width * m_frame->height * 3) to
 * write the converted RGB data into.
 */
void VideoDecoder::convertAVFrameToRGBBuffer(const AVFrame *frame, uint8_t *rgb_buffer) {

    // Create a scaling context.
    SwsContext *sws_ctx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat) frame->format,     // source width, height, and pixel format.
            frame->width, frame->height,
            AV_PIX_FMT_RGB24,                                               // destination width, height, and pixel format.
            SWS_BICUBIC, nullptr, nullptr, nullptr                          // scaling method and additional parameters.
    );

    if (!sws_ctx) {
        throw std::runtime_error("failed to create scaling context");
    }

    // Define the RGB buffer as the destination for sws_scale.
    uint8_t *dest[1] = {rgb_buffer};
    int dest_linesize[1] = {3 * frame->width}; // RGB24 has 3 bytes per pixel.

    // Perform the conversion.
    sws_scale(
        sws_ctx,
        frame->data, frame->linesize, 0, frame->height,
        dest, dest_linesize
    );

    // Free the scaling context.
    sws_freeContext(sws_ctx);
}

/**
 * Returns the best effort timestamp of an AVFrame in microseconds.
 * @param frame m_frame to get the best effort timestamp of.
 * @param stream stream the m_frame belongs to. Can get received by calling VideoDecoder::getStream()
 * on the same decoder object you received the m_frame from.
 */
int64_t VideoDecoder::getBestEffortTimestampInMicroseconds(const AVFrame *frame, const AVStream *stream) {
    return av_rescale_q(frame->best_effort_timestamp, stream->time_base, {1, AV_TIME_BASE});
}
