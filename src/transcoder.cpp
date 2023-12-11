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

#include "../include/transcoder.hpp"


volatile sig_atomic_t stop; // signal.h variable
int ret;                    // return values of the functions
char errorBuff[80];         // error string buffer  
int video_stream_idx;       // video stream index

// Time calculation based variables
struct timespec start_time, end_time; 
double total_elapsed;
double avg_elapsed;

void inthand(int signum) {
    stop = 1;
}

void start_timer(){
    clock_gettime(CLOCK_REALTIME, &start_time);
}

void stop_timer(bool error=false)
{
	// Stop measuring time and calculate the elapsed time
    clock_gettime(CLOCK_REALTIME, &end_time);
	double elapsed;
    long seconds;
	long nanoseconds;

	seconds = end_time.tv_sec - start_time.tv_sec;
	nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
	
	if (error) {
		elapsed = 0;
		total_elapsed += 0;
		printf(", Error occured.. Time measured: %.3f ms.\n", elapsed);
	}
	else {
		elapsed = seconds + nanoseconds*1e-9;
		total_elapsed += elapsed*1000;
		printf(", Time measured: %.3f ms.\n", elapsed*1000);
	}
}	


int open_input_stream(InputUtils *in_state, const char *input_filename) 
{
	// Liniking variables
	auto &input_fmt_ctx = in_state->input_fmt_ctx;

    //Allocate context for input stream
    input_fmt_ctx = avformat_alloc_context();

	AVDictionary *open_opts = NULL;
    av_dict_set(&open_opts, "rtsp_transport", "tcp", 0);

	//Open the input stream and read its header
	ret = avformat_open_input(&input_fmt_ctx, input_filename, NULL, &open_opts);
	if (ret < 0) {
	std::cout << "Couldn't open the stream.\n";
	return EXIT_FAILURE;
	}
	
	ret = avformat_find_stream_info(input_fmt_ctx, NULL);
	if (ret < 0){
		std::cout << "Couldn't find stream info.\n";
		return 1;
   	}
	av_dump_format(input_fmt_ctx, 0, input_filename, 0);

    return 0;
}


int setup_decoder(InputUtils *in_state) 
{
	// Linking Variables
	auto &input_fmt_ctx = in_state->input_fmt_ctx;
	auto &input_codec_params = in_state->input_codec_params;
	auto &input_codec = in_state->input_codec;
	auto &input_stream = in_state->input_stream;
	auto &input_codec_ctx = in_state->input_codec_ctx;
    
	// Finding and selecting the Video Stream
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

	// Setting up the codec context for the decoder
	input_codec_ctx = avcodec_alloc_context3(input_codec);
	avcodec_parameters_to_context(input_codec_ctx, input_codec_params);
	
	// Opening the codec for decoding
	if (avcodec_open2(input_codec_ctx, input_codec, NULL) < 0 ) {
		std::cout << "Couldn't open decoder\n";
		return EXIT_FAILURE;
	}

    return 0;
}

int setup_output_stream(OutputUtils *out_state, const char *output_filename)
{
	//Linking Variables
	auto &output_fmt_ctx = out_state->output_fmt_ctx;
	auto &output_stream = out_state->output_stream;

    // Allocating context for output file plus guessing the output file format
	avformat_alloc_output_context2(&output_fmt_ctx, NULL, NULL, output_filename);
	if (!output_fmt_ctx) {
		printf("Could not deduce output format from file extension: using MP4.\n");
		avformat_alloc_output_context2(&output_fmt_ctx, NULL, "mp4", output_filename);
	}
    if (!output_fmt_ctx)
        return 1;

    output_stream = avformat_new_stream(output_fmt_ctx, NULL);

    return 0;
}

int setup_encoder(InputUtils *in_state, OutputUtils *out_state)
{
	// Linking variables
	auto &input_framerate = in_state->input_framerate;
	auto &input_fmt_ctx = in_state->input_fmt_ctx;
	auto &input_codec_ctx = in_state->input_codec_ctx;
	auto &input_stream = in_state->input_stream;
	auto &input_codec = in_state->input_codec;
	auto &output_codec = out_state->output_codec;
	auto &output_codec_ctx = out_state->output_codec_ctx;
	auto &output_stream = out_state->output_stream;
	auto &output_fmt_ctx = out_state->output_fmt_ctx;
    
    // Defining and setting up the encoder for output stream
	// output_codec = avcodec_find_encoder(input_codec_ctx->codec_id);
	output_codec = avcodec_find_encoder_by_name("libx264"); // x265 not supported always, x264 most versatile

	if (output_codec == NULL) {
		std::cout << "Could not find encoder the input stream using codec: " << avcodec_get_name(input_codec_ctx->codec_id) << std::endl;
        return 1;
    }
	output_codec_ctx = avcodec_alloc_context3(output_codec);
    
    output_codec_ctx->height = input_codec_ctx->height;
    output_codec_ctx->width = input_codec_ctx->width;
	if (input_codec->pix_fmts)
        output_codec_ctx->pix_fmt = output_codec->pix_fmts[0];
    else
        output_codec_ctx->pix_fmt = input_codec_ctx->pix_fmt;

	// time base
	input_framerate = av_guess_frame_rate(input_fmt_ctx, input_stream, NULL);
    output_codec_ctx->time_base = av_inv_q(input_framerate);
    output_stream->time_base = output_codec_ctx->time_base;
	

    // Setting options for encoder
	if (output_codec_ctx->codec_id == AV_CODEC_ID_H265) {
		const char *codec_priv_key = "x265-params";
		// disables the scene change detection and fix GOP on 60 frames
		const char *codec_priv_value = "keyint=60:min-keyint=60:scenecut=0";
		av_opt_set(output_codec_ctx->priv_data, codec_priv_key, codec_priv_value, 0);
		// crf range 0-51, 0 for lossless best quality, 51 for lossy worst quality, 28 is default
		av_opt_set(output_codec_ctx->priv_data, "crf", "28", AV_OPT_SEARCH_CHILDREN);
		av_opt_set(output_codec_ctx->priv_data, "tune", "zerolatency", 0);
		av_opt_set(output_codec_ctx->priv_data, "preset", "veryfast", 0);
	}
	else if (output_codec_ctx->codec_id == AV_CODEC_ID_H264){
		const char *codec_priv_key = "x264-params";
		const char *codec_priv_value = "keyint=60:min-keyint=60:scenecut=0";
		av_opt_set(output_codec_ctx->priv_data, codec_priv_key, codec_priv_value, 0);
		// crf range 0-51, 0 for lossless best quality, 51 for lossy worst quality, 23 is default
		av_opt_set(output_codec_ctx->priv_data, "crf", "23", AV_OPT_SEARCH_CHILDREN);
		av_opt_set(output_codec_ctx->priv_data, "tune", "zerolatency", 0);
		av_opt_set(output_codec_ctx->priv_data, "preset", "veryfast", 0);
	}
	// Some formats want stream headers to be separate.
	if (output_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
  		output_fmt_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // filling codec parameters of the output stream based on output codec context
	avcodec_parameters_from_context(output_stream->codecpar, output_codec_ctx);

    // Open the output_codec for encoding (encoder)
	ret = avcodec_open2(output_codec_ctx, output_codec, NULL);
	if (ret < 0) {
		std::cout << "Could not open video codec (encoder).\n" << av_make_error_string(errorBuff,80,ret) << std::endl;
		return 1;
	}

    return 0;
}

int open_output_stream(OutputUtils *out_state, const char *output_filename) 
{
	// Linking Variables
	auto &output_fmt_ctx = out_state->output_fmt_ctx;

	// print the output stream information in termina;
    av_dump_format(output_fmt_ctx, 0, output_filename, 1);
    	
	// open the output file
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

    return 0;
}

int encode(InputUtils *in_state, OutputUtils *out_state, bool flush=false) 
{
	// Linking variable
	auto &input_frame = in_state->input_frame;
	auto &input_stream = in_state->input_stream;
	auto &output_fmt_ctx = out_state->output_fmt_ctx;
	auto &output_codec_ctx = out_state->output_codec_ctx;
	auto &output_stream = out_state->output_stream;
	
	if (flush) {
		input_frame = NULL;
	}

    AVPacket *output_packet = av_packet_alloc();
	ret = avcodec_send_frame(output_codec_ctx, input_frame);
    
    while (ret >= 0) {

        ret = avcodec_receive_packet(output_codec_ctx, output_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
			stop_timer(true);
			std::cout << "Error in encoding process.";
            return EXIT_FAILURE;
        }
		
		std::cout <<"\nWriting frame number: " << output_codec_ctx->frame_number ;

        output_packet->stream_index = video_stream_idx;
		output_packet->duration = av_rescale_q(output_packet->duration, output_codec_ctx->time_base, input_stream->time_base);
        
		av_packet_rescale_ts(output_packet, input_stream->time_base, output_stream->time_base);
        ret = av_interleaved_write_frame(output_fmt_ctx, output_packet);
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

int transcode(InputUtils *in_state, OutputUtils *out_state)
{
	auto &input_frame = in_state->input_frame;
	auto &input_fmt_ctx = in_state->input_fmt_ctx;
	auto &input_codec_ctx = in_state->input_codec_ctx;
	auto &input_packet = in_state->input_packet;
	
	input_frame = av_frame_alloc();
	input_packet = av_packet_alloc();

	while (!stop) {
		start_timer();
		
		ret = av_read_frame(input_fmt_ctx, input_packet);
		if (ret < 0)
			break;
		if (input_packet->stream_index != video_stream_idx) {

			av_packet_unref(input_packet);
			continue;
		}
        ret = avcodec_send_packet(input_codec_ctx, input_packet);
        if (ret < 0) {
            std::cerr << "Error sending packet to decoder: " << av_make_error_string(errorBuff, 80, ret) << std::endl;
            return ret;
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(input_codec_ctx, input_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				stop_timer(true);
                break;
            } 
            else if (ret < 0) {
                return ret;
            }

            if (ret >= 0) {
				input_frame->pict_type = AV_PICTURE_TYPE_NONE;   // let encoder set the picture type by its own.
                if (encode(in_state, out_state, false) != 0) 
					return EXIT_FAILURE;
            }
			stop_timer();
            av_frame_unref(input_frame);
        }
        av_packet_unref(input_packet);     
    
    }

	return 0;
}

void close_streams(InputUtils *in_state, OutputUtils *out_state) 
{
	// Linking Variables	
	auto &input_frame = in_state->input_frame;
	auto &input_fmt_ctx = in_state->input_fmt_ctx;
	auto &input_codec_ctx = in_state->input_codec_ctx;
	auto &input_packet = in_state->input_packet;
	auto &output_fmt_ctx = out_state->output_fmt_ctx;
	auto &output_codec_ctx = out_state->output_codec_ctx;

	printf("\nAverage Frame Write Time: %.3f ms.\n", total_elapsed/((output_codec_ctx->frame_number)-1));
	
	// Flush encoder by giving NULL frame to signal the end of stream
    encode(in_state, out_state, true);
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
}