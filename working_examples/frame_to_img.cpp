#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/log.h>
	#include <libavutil/error.h>
}


int main() {
	std::cout << "Hello World\n";
	
	//Opening the RTSP stream
	AVFormatContext* av_format_ctx = avformat_alloc_context();

	/* We dont wanna give any AV dictionary options, 
	We dont know the input format so they both will be NULL.*/ 
	if (avformat_open_input(&av_format_ctx, "rtsp://admin:admin@192.168.1.166:8554/live", NULL, NULL) != 0) {
		std::cout << "Couldn't open video file.\n";
		return EXIT_FAILURE;
	}

	//Search for video stream inside AVFormatContext
	int video_stream_idx = -1; 
	const AVCodec* av_codec;
	AVCodecParameters* av_codec_params;


	for (int i = 0; i < av_format_ctx->nb_streams; ++i) {
		av_codec_params = av_format_ctx->streams[i]->codecpar;
		av_codec = avcodec_find_decoder(av_codec_params->codec_id);

		if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_idx = i;
			break;
		}
	}

	if (video_stream_idx == -1) {
		std::cout << "Couldn't find valid video stream.\n";
		return EXIT_FAILURE;
	}

	//Setting up the codec context for the decoder
	AVCodecContext* av_codec_ctx = avcodec_alloc_context3(av_codec);
	avcodec_parameters_to_context(av_codec_ctx, av_codec_params);
	
	// Opening the codec
	if (avcodec_open2(av_codec_ctx, av_codec, NULL) < 0 ) {
		std::cout << "Couldn't open codec\n";
		return EXIT_FAILURE;
	}

	// Getting ready to read packets and frames
	AVFrame* av_frame = av_frame_alloc();
	AVPacket* av_packet = av_packet_alloc();

	//int response;
	int ret;
	while (1) {

		if (ret = av_read_frame(av_format_ctx, av_packet) < 0) {

			av_log(NULL, AV_LOG_ERROR, "cannot read frame");
			break;
		}

		if (av_packet->stream_index == video_stream_idx) {
			
			ret = avcodec_send_packet(av_codec_ctx, av_packet);
			if (ret < 0) {
				fprintf(stderr, "Error sending a packet for decoding\n");
				exit(1);
			}
		}

		while (ret >= 0) {

			ret = avcodec_receive_frame(av_codec_ctx, av_frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				continue;
			else if (ret < 0) {

				fprintf(stderr, "Error during decoding\n");
				exit(1);
			}
			printf("saving frame %3d\n", av_codec_ctx->frame_number);
			fflush(stdout);

			std::stringstream filename ;
			filename << "test" << av_codec_ctx->frame_number <<".yuv"  ;
			FILE *fout = fopen(filename.str().c_str(), "w");

			fprintf(fout, "P5\n%d %d\n%d\n", av_frame->width, av_frame->height, 255);

			for (int i = 0; i < av_frame->height; i++) {
				fwrite(av_frame->data[0] + i * av_frame->linesize[0], 1, av_frame->width, fout);
			}
			
			fclose(fout);
		}
		av_packet_unref(av_packet);
		
	}
	av_frame_free(&av_frame);
	av_packet_free(&av_packet);
	avformat_close_input(&av_format_ctx);
	avformat_free_context(av_format_ctx);
	avcodec_free_context(&av_codec_ctx);

	
	return 0;
}

