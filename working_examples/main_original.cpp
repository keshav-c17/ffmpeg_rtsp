#include <iostream>
#include <stdio.h>
#include <signal.h>
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
    #include <libavutil/opt.h>
}
#define RESET   "\033[0m"
#define RED     "\033[31m"      /* Red */

volatile sig_atomic_t stop;

void inthand(int signum) {
    stop = 1;
}

int encode(AVPacket* input_packet, AVFrame *input_frame, AVFormatContext *output_fmt_ctx, AVStream *input_stream, AVStream *output_stream, AVCodecContext *output_codec_ctx, int index) {
   
    AVPacket *output_packet = av_packet_alloc();
	int response = avcodec_send_frame(output_codec_ctx, input_frame);
    
    while (response >= 0) {
        response = avcodec_receive_packet(output_codec_ctx, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        } else if (response < 0) {
            return -1;
        }
        output_packet->stream_index = index;
        //output_packet->duration = output_stream->time_base.den / output_stream->time_base.num / input_stream->avg_frame_rate.num * input_stream->avg_frame_rate.den;
		output_packet->duration = av_rescale_q(output_packet->duration, output_codec_ctx->time_base, input_stream->time_base);
        
		av_packet_rescale_ts(output_packet, input_stream->time_base, output_stream->time_base);
        response = av_interleaved_write_frame(output_fmt_ctx, output_packet);
    }
    // av_packet_unref(output_packet);
    // av_packet_free(&output_packet);
    return 0;
}


int main(int argc, char **argv) {
	signal(SIGINT, inthand);
	
	int ret; // Defining a variable that will hold return value of functions
    char errorBuff[80];
	const char *input_filename = argv[1];
	const char *output_filename = argv[2];
    if (argc < 3 || argc > 3) {
        printf("ERROR: Provide input_filename and output_filename as arguments.\n"
               "USAGE: %s <input_filename> <output_filename>\n", argv[0]);
        exit(1);
    }

	//Allocate context for input stream
	AVFormatContext *input_fmt_ctx = avformat_alloc_context();

	//Open the input stream and read its header
	ret = avformat_open_input(&input_fmt_ctx, argv[1], NULL, NULL);
	if (ret < 0) {
	std::cout << "Couldn't open the stream.\n";
	return EXIT_FAILURE;
	}
	
	ret = avformat_find_stream_info(input_fmt_ctx, NULL);
	if (ret < 0){
		std::cout << "Couldn't find stream info.\n";
		return 1;
   	}

	/*
	Now we need to find the video stream out of audio-video stream.
	For this read the codec parameters for each stream and match
	codec type with AVMEDIA_TYPE VIDEO
	*/
	int video_stream_idx = -1;
	AVStream *input_stream;
	AVCodecParameters *input_codec_params;
	const AVCodec *input_codec;

	for (int i = 0; i < input_fmt_ctx->nb_streams; ++i) {
		input_codec_params = input_fmt_ctx->streams[i]->codecpar;
		input_codec = avcodec_find_decoder(input_codec_params->codec_id);

		if (input_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_idx = i;
			input_stream = input_fmt_ctx->streams[i];
			break;
		}
	}
	if (video_stream_idx == -1) {
		std::cout << "Couldn't find the video stream.\n";
		return EXIT_FAILURE;
	}

	//Setting up the codec context for the decoder
	AVCodecContext* input_codec_ctx = avcodec_alloc_context3(input_codec);
	avcodec_parameters_to_context(input_codec_ctx, input_codec_params);
	
	// Opening the codec for decoding
	if (avcodec_open2(input_codec_ctx, input_codec, NULL) < 0 ) {
		std::cout << "Couldn't open decoder\n";
		return EXIT_FAILURE;
	}

//////////////////////////////////////////////////////////////////////////////////////////////////
//					Setting up Output Stream / Output Codec (Encoder)							//
//////////////////////////////////////////////////////////////////////////////////////////////////

	/* 
	we're going to use internal options for the x265
	it disables the scene change detection and then fix
	GOP on 60 frames. 
	*/
	const char *codec_priv_key = "x265-params";
	const char *codec_priv_value = "keyint=60:min-keyint=60:scenecut=0";

    // Allocating context for output file plus guessing the output file format
	AVFormatContext *output_fmt_ctx;
	avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL, output_filename);
	if (!output_fmt_ctx) {
		printf("Could not deduce output format from file extension: using MP4.\n");
		avformat_alloc_output_context2(&output_fmt_ctx, NULL, "mp4", output_filename);
	}
    if (!output_fmt_ctx)
        return 1;

    AVRational input_framerate = av_guess_frame_rate(input_fmt_ctx, input_stream, NULL);

    AVStream *output_stream = avformat_new_stream(output_fmt_ctx, NULL);

    // Defining and setting up the encoder for output stream
	const AVCodec *output_codec = avcodec_find_encoder_by_name("libx265");   // x265 not supported always, x264 most versatile

	if (output_codec == NULL) {
        std::cout << "Unable to find Encoder. Aborting!";
        return 1;
    }
	AVCodecContext *output_codec_ctx = avcodec_alloc_context3(output_codec);
    
    output_codec_ctx->height = input_codec_ctx->height;
    output_codec_ctx->width = input_codec_ctx->width;
    output_codec_ctx->pix_fmt = output_codec->pix_fmts[0];
    // control rate
    output_codec_ctx->bit_rate = 2 * 1000 * 1000;
    output_codec_ctx->rc_buffer_size = 4 * 1000 * 1000;
    output_codec_ctx->rc_max_rate = 2 * 1000 * 1000;
    output_codec_ctx->rc_min_rate = 2.5 * 1000 * 1000;
    // time base
    output_codec_ctx->time_base = av_inv_q(input_framerate);
    output_stream->time_base = output_codec_ctx->time_base;
    
    // Setting options for encoder
	if (output_codec_ctx->codec_id == AV_CODEC_ID_H265) {
		//av_opt_set(output_codec_ctx->priv_data, codec_priv_key, codec_priv_value, 0);
		av_opt_set(output_codec_ctx->priv_data, "tune", "zerolatency", 0);
		av_opt_set(output_codec_ctx->priv_data, "preset", "veryfast", 0);
	}
	else {
		// const char *codec_priv_key = "x264-params";
		// const char *codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
		//av_opt_set(output_codec_ctx->priv_data, codec_priv_key, codec_priv_value, 0);
		av_opt_set(output_codec_ctx->priv_data, "tune", "zerolatency", 0);
		av_opt_set(output_codec_ctx->priv_data, "preset", "veryfast", 0);

	}
    avcodec_parameters_from_context(output_stream->codecpar, output_codec_ctx);


    // Open the codec (encoder)
	ret = avcodec_open2(output_codec_ctx, output_codec, NULL);
	if (ret < 0) {
		std::cout << "Could not open video codec (encoder).\n" << av_make_error_string(errorBuff,80,ret) << std::endl;
		return 1;
	}


	av_dump_format(output_fmt_ctx, 0, output_filename, 1);
    
    if (output_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
  		output_fmt_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;


	/* open the output file */
    if (!(output_fmt_ctx->flags & AVFMT_NOFILE)) {
        ret = avio_open(&output_fmt_ctx->pb, output_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cout << "Could not open outputfile" << av_make_error_string(errorBuff, 80, ret) << std::endl;
            return 1;
        }
    }

	ret = avformat_write_header(output_fmt_ctx, NULL);
	if (ret < 0) 
		std::cout << "Error occured when writing header" << av_make_error_string(errorBuff, 80, ret) << std::endl;


    AVFrame *input_frame = av_frame_alloc();
    AVPacket *input_packet = av_packet_alloc();

 	while (!stop) {

		ret = av_read_frame(input_fmt_ctx, input_packet);
		if (ret < 0)
			break;
		if (input_packet->stream_index != video_stream_idx) {

			av_packet_unref(input_packet);
			continue;
		}
		
        ret = avcodec_send_packet(input_codec_ctx, input_packet);

        while (ret >= 0) {
            ret = avcodec_receive_frame(input_codec_ctx, input_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
                break;
            else if (ret < 0)
                return ret;

            if (ret >= 0) {
                if (encode(input_packet, input_frame, output_fmt_ctx, input_stream, output_stream, output_codec_ctx, video_stream_idx)) 
					return -1;
            }
            av_frame_unref(input_frame);
        }
        av_packet_unref(input_packet);     
    
    }
	// Flush encoder by giving NULL frame to signal the end of stream
    encode(input_packet, NULL, output_fmt_ctx, input_stream, output_stream, output_codec_ctx, video_stream_idx);
    std::cout << "\nClosing input and saving the data to container\n" << std::endl;
	av_write_trailer(output_fmt_ctx);

	// Closing and freeing the memory
	avcodec_free_context(&input_codec_ctx);
    avcodec_free_context(&output_codec_ctx);
	av_frame_free(&input_frame);
	av_packet_free(&input_packet);
	avformat_close_input(&input_fmt_ctx);
	avformat_free_context(input_fmt_ctx);
	avformat_free_context(output_fmt_ctx);

    return 0;

}