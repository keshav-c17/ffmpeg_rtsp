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

int main (int argc, char **argv)
{

	signal(SIGINT, inthand);

	const char *input_filename = argv[1];
	const char *output_filename = argv[2];

	if (argc < 3 || argc > 3) {
		printf("\nERROR: Provide input_filename and output_filename as arguments.\n"
		"USAGE: ./rtsp_ffmpeg <input_filename> <output_filename>\n\n");
		exit(1);
	}

	InputUtils in_state;
	OutputUtils out_state;

	if (open_input_stream(&in_state, input_filename) != 0){
		return EXIT_FAILURE;
	}
	if (setup_decoder(&in_state) != 0) {
		return EXIT_FAILURE;
	}
	if (setup_output_stream(&out_state, output_filename) != 0) {
		return EXIT_FAILURE;
	}
	if (setup_encoder(&in_state, &out_state) != 0) {
		return EXIT_FAILURE;
	}
	if (open_output_stream(&out_state, output_filename) != 0){
		return EXIT_FAILURE;
	}
	if (transcode(&in_state, &out_state) != 0){
		return EXIT_FAILURE;
	}

	close_streams(&in_state, &out_state);
	
	// Calculating output video file size
	std::uintmax_t size = std::experimental::filesystem::file_size(output_filename);
	float new_size = size;
	if (new_size/1000000 < 1) {
		printf("\nOutput Video File size: %.2f KB\n", new_size/1000);
		std::cout << std::endl;
	}
	else {
		printf("\nOutput Video File size: %.2f MB\n", new_size/1000000);
		std::cout << std::endl;
	}
	return 0;
}


