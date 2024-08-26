#include "video-encoder.h"


/**
 * Initializes the encoder with the specified parameters.
 *
 * @param filepath Path to the output video file.
 * @param width Width of the output video in pixels.
 * @param height Height of the output video in pixels.
 * @param fps Frames per second of the output video.
 * @param bitrate Bitrate of the output video in bits per second.
 */
VideoEncoder::VideoEncoder(const std::string &filepath, int width, int height, double fps, int64_t bitrate)
    : m_format_context(nullptr), m_codec_context(nullptr), m_stream(nullptr), m_frame(nullptr), m_packet(nullptr),
    m_sws_context(nullptr), m_finalized(false), m_pts(0) {

    // Initialize the format context.
    avformat_alloc_output_context2(&m_format_context, nullptr, nullptr, filepath.c_str());
    if (!m_format_context) {
        throw std::runtime_error("Could not allocate output format context");
    }

    // Open the output file.
    if (!(m_format_context->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_format_context->pb, filepath.c_str(), AVIO_FLAG_WRITE) < 0) {
            throw std::runtime_error("Could not open output file");
        }
    }

    // Find the encoder.
    const AVCodec *codec = avcodec_find_encoder(m_format_context->oformat->video_codec);

    if (!codec) {
        throw std::runtime_error("Could not find encoder");
    }

    // Create a new stream.
    m_stream = avformat_new_stream(m_format_context, nullptr);
    if (!m_stream) {
        throw std::runtime_error("Could not create new stream");
    }

    // Allocate codec context.
    m_codec_context = avcodec_alloc_context3(codec);
    if (!m_codec_context) {
        throw std::runtime_error("Could not allocate video codec context");
    }

    // Set codec parameters.
    m_codec_context->width = width;
    m_codec_context->height = height;
    m_codec_context->time_base = AVRational{1000, static_cast<int>(lround(fps * 1000))};
    m_codec_context->framerate = av_d2q(fps, 100000);
    m_codec_context->gop_size = 12;
    m_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codec_context->bit_rate = bitrate;

    if (m_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
        m_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open codec.
    if (avcodec_open2(m_codec_context, codec, nullptr) < 0) {
        throw std::runtime_error("Could not open codec");
    }

    // Copy codec parameters to stream.
    if (avcodec_parameters_from_context(m_stream->codecpar, m_codec_context) < 0) {
        throw std::runtime_error("Could not copy codec parameters to stream");
    }

    m_stream->time_base = m_codec_context->time_base;

    // Write the header.
    if (avformat_write_header(m_format_context, nullptr) < 0) {
        throw std::runtime_error("Could not write format header");
    }

    // Allocate frame.
    m_frame = av_frame_alloc();
    if (!m_frame) {
        throw std::runtime_error("Could not allocate video frame");
    }

    m_frame->format = m_codec_context->pix_fmt;
    m_frame->width = width;
    m_frame->height = height;

    if (av_frame_get_buffer(m_frame, 32) < 0) {
        throw std::runtime_error("Could not allocate frame buffer");
    }

    // Allocate packet.
    m_packet = av_packet_alloc();
    if (!m_packet) {
        throw std::runtime_error("Could not allocate packet");
    }
}


/**
 * Destructor to ensure proper cleanup and finalization of the encoding process.
 * Calls finalize() to ensure that the file is correctly written.
 */
VideoEncoder::~VideoEncoder() {
    finalize();  // Ensure finalization before cleanup

    if (m_sws_context) {
        sws_freeContext(m_sws_context);
    }
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    if (m_codec_context) {
        avcodec_free_context(&m_codec_context);
    }
    if (m_format_context) {
        if (!(m_format_context->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_format_context->pb);
        }
        avformat_free_context(m_format_context);
    }
}


/**
 * Encodes an RGB frame with specific width and height (expected buffer size: width x height x 3 bytes).
 * The input frame is converted to YUV and resized to output video dimensions as needed before encoding.
 *
 * @param rgb_buffer Pointer to the RGB buffer representing the frame to be encoded.
 * @param width Width of the input frame in pixels.
 * @param height Height of the input frame in pixels.
 */
void VideoEncoder::encodeFrame(const uint8_t *rgb_buffer, int width, int height) {

    // Initialize the scaling context if necessary.
    if (!m_sws_context || width != m_frame->width || height != m_frame->height) {
        sws_freeContext(m_sws_context);
        m_sws_context = sws_getContext(
            width, height, AV_PIX_FMT_RGB24,                                // Source dimensions and format.
            m_frame->width, m_frame->height, m_codec_context->pix_fmt,      // Destination dimensions and format.
            SWS_BICUBIC, nullptr, nullptr, nullptr
        );
        if (!m_sws_context) {
            throw std::runtime_error("Failed to create scaling context");
        }
    }

    // Convert RGB to YUV and scale.
    uint8_t *src_slices[1] = {const_cast<uint8_t *>(rgb_buffer)};
    int src_stride[1] = {3 * width};                                        // RGB24 has 3 bytes per pixel.

    sws_scale(
        m_sws_context,

        src_slices,
        src_stride,
        0,
        height,

        m_frame->data,
        m_frame->linesize
    );

    // Encode the frame.
    encodeFrame(m_frame);
}


/**
 * Finalizes the encoding process, ensuring that the output file is written correctly.
 * This method flushes the encoder, writes the trailer, and cleans up FFmpeg structures.
 */
void VideoEncoder::finalize() {
    if (!m_finalized) {

        // Flush the encoder.
        avcodec_send_frame(m_codec_context, nullptr);
        while (true) {
            int ret = avcodec_receive_packet(m_codec_context, m_packet);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                break;
            }
            av_interleaved_write_frame(m_format_context, m_packet);
            av_packet_unref(m_packet);
        }

        // Write the trailer.
        av_write_trailer(m_format_context);

        m_finalized = true;
    }
}


/**
 * Encodes a YUV frame (as an AVFrame) without any colorspace conversion. However,
 * the frame is resized to output video dimensions as needed before encoding.
 *
 * @param frame Pointer to the AVFrame representing the YUV frame to be encoded.
 */
void VideoEncoder::encodeFrame(AVFrame* frame) {

    // Calculate the correct PTS for the frame based on the frame index.
    frame->pts = av_rescale_q(m_pts++, m_codec_context->time_base, m_stream->time_base);

    int ret;

    // Send the frame to the encoder.
    while (true) {
        ret = avcodec_send_frame(m_codec_context, frame);
        if (ret == AVERROR(EAGAIN)) {

            // The encoder cannot accept new frames until we receive more packets.
            while (true) {
                ret = avcodec_receive_packet(m_codec_context, m_packet);
                if (ret == AVERROR(EAGAIN)) {

                    // No more packets to receive, break out of the loop.
                    break;
                } else if (ret == AVERROR_EOF) {

                    // Encoder has been fully flushed, break out.
                    return;
                } else if (ret < 0) {
                    throw std::runtime_error("Error receiving packet from encoder");
                }

                // Write the encoded packet to the output file.
                av_interleaved_write_frame(m_format_context, m_packet);
                av_packet_unref(m_packet);
            }
        } else if (ret == AVERROR_EOF) {

            // End of the stream, nothing more to encode.
            return;
        } else if (ret < 0) {
            throw std::runtime_error("Error sending frame to encoder");
        } else {

            // Frame sent successfully, now break out of this loop to receive packets.
            break;
        }
    }

    // Receive all available packets from the encoder after sending a frame.
    while (true) {
        ret = avcodec_receive_packet(m_codec_context, m_packet);
        if (ret == AVERROR(EAGAIN)) {

            // No more packets available right now, return.
            break;
        } else if (ret == AVERROR_EOF) {

            // Encoder has been fully flushed, return.
            return;
        } else if (ret < 0) {
            throw std::runtime_error("Error receiving packet from encoder");
        }

        // Write the encoded packet to the output file.
        av_interleaved_write_frame(m_format_context, m_packet);
        av_packet_unref(m_packet);
    }
}
