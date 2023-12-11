/**
 * @Project : RTSP stream Transcoding and Storing using FFmpeg
 * 
 * @Brief   : Transcode and store video to a file from live RTSP stream using FFmpeg
 * 
 * @Created : 21-Feb-2022
 * 
 * @Updated : 11-Dec-2023
 * 
 * @Author  : Keshav Choudhary <keshav.choudhary0@gmail.com>
 */

#ifndef transcoder_hpp
#define transcoder_hpp

#include <iostream>
#include <signal.h>
#include <experimental/filesystem>  // used for calculating the output file size
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
    #include <libavutil/opt.h>
}


// Input Utilities
struct InputUtils {
    AVFormatContext *input_fmt_ctx;
    AVStream *input_stream;
    AVCodecParameters *input_codec_params;
    const AVCodec *input_codec;
    AVCodecContext* input_codec_ctx;
    AVFrame *input_frame;
    AVPacket *input_packet;
    AVRational input_framerate;
};

// Output Utilities
struct OutputUtils {
    AVFormatContext *output_fmt_ctx;
    AVStream *output_stream;
    const AVCodec *output_codec;
    AVCodecContext *output_codec_ctx;
};

void inthand(int signum);
int open_input_stream(InputUtils *in_state, const char *input_filename);
int setup_decoder(InputUtils *in_state);
int setup_output_stream(OutputUtils *out_state, const char *output_filename);
int setup_encoder(InputUtils *in_state, OutputUtils *out_state);
int open_output_stream(OutputUtils *out_state, const char *output_filename);
int encode(InputUtils *in_state, OutputUtils *out_state);
int transcode(InputUtils *in_state, OutputUtils *out_state);
void close_streams(InputUtils *in_state, OutputUtils *out_state);

#endif