#include <iostream>
#include <stdio.h>
#include <signal.h>
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libavformat/avio.h>
}
#define RESET   "\033[0m"
#define RED     "\033[31m"      /* Red */


volatile sig_atomic_t stop;

void inthand(int signum) {
    stop = 1;
}


int main(int argc, char **argv) {

	signal(SIGINT, inthand);
	
	int ret; // Defining a variable that will hold return value of functions
	const char *input_filename = argv[1];
	const char *output_filename = argv[2];

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
		std::cout << "Couldn't open codec\n";
		return EXIT_FAILURE;
	}

	///////////////////////////////
	// Setting up the output file//
	///////////////////////////////
	
	// Allocating context for output file plus guessing the output file format
	AVFormatContext *output_fmt_ctx;
	avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL, output_filename);
	if (!output_fmt_ctx) {
		printf("Could not deduce output format from file extension: using MP4.\n");
		avformat_alloc_output_context2(&output_fmt_ctx, NULL, "mp4", output_filename);
	}
    	if (!output_fmt_ctx)
        	return 1;


	// Creating and setting up the output stream
	AVRational input_framerate = av_guess_frame_rate(input_fmt_ctx, input_stream, NULL);
	AVStream *output_stream;
	output_stream = avformat_new_stream(output_fmt_ctx, NULL);
	avcodec_parameters_copy(output_stream->codecpar, input_codec_params);
	output_stream->time_base = av_inv_q(input_framerate);


	// Allocating memory to read input packets and frames
	AVFrame *input_frame = av_frame_alloc();
	AVPacket *input_packet = av_packet_alloc();
	
	av_dump_format( input_fmt_ctx, 0, input_filename, 0 );
	std::cout << std::endl;
	av_dump_format(output_fmt_ctx, 0, output_filename, 1);
	char errorBuff[80];

	if (output_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER){
  		output_fmt_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	// Opening the output file
	if (!(output_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		int ret = avio_open(&output_fmt_ctx->pb, output_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			std::cout << "Could not open output file.\n" << av_make_error_string(errorBuff,80,ret) << std::endl;
			return 1;
		}
	}
	// Writing the output stream header
	ret = avformat_write_header(output_fmt_ctx, NULL);
	if (ret < 0) {
        std::cout << "Error occurred when opening output file.\n" << av_make_error_string(errorBuff,80,ret) << std::endl;
        return 1;
    	}

	std::cout << "\nStart reading packets and saving to the output container.." << std::endl;
	int64_t old_pts = 0;
	while (!stop) {

		ret = av_read_frame(input_fmt_ctx, input_packet);
		if (ret < 0)
			break;
		if (input_packet->stream_index != video_stream_idx) {
			av_packet_unref(input_packet);
			continue;
		}
		
		input_packet->pts = av_rescale_q_rnd(input_packet->pts, input_stream->time_base, output_stream->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		input_packet->dts = input_packet->pts;
		// std::cout<<input_packet->pts<<" "<<input_packet->dts<<"\n";
		if (old_pts > input_packet->pts) {
			std::cout << RED << "ERROR: Invalid packet PTS found. " << old_pts << " > " << input_packet->pts<< RESET <<" Trying to correct..\n";
			
			input_packet->pts = input_packet->dts = old_pts + 1;
		}
		old_pts = input_packet->pts;

		input_packet->duration = av_rescale_q(input_packet->duration, input_stream->time_base, output_stream->time_base);
		input_packet->pos = -1;
		
		ret = av_interleaved_write_frame(output_fmt_ctx, input_packet);
		if (ret < 0) {
			fprintf(stderr, "Error muxing packet\n");
			break;
		}
		av_packet_unref(input_packet);
	}
	
	std::cout << "Closing input and saving the data to container" << std::endl;
	av_write_trailer(output_fmt_ctx);


	// Closing and freeing the memory
	avcodec_free_context(&input_codec_ctx);
	av_frame_free(&input_frame);
	av_packet_free(&input_packet);
	avformat_close_input(&input_fmt_ctx);
	avformat_free_context(input_fmt_ctx);
	avformat_free_context(output_fmt_ctx);


	return 0;
}