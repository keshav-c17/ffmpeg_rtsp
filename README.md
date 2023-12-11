This project aims to save an input RTSP stream to a user specified output file using FFmpeg.  

### Software Requirments ###
* Ubuntu 20.04 (other linux based OS)
* FFmpeg 4.x (not tested with version below 4.x)
* build-essentials, cmake, pkg-config to build the project

### How to build ? ###
    git clone https://github.com/keshav-c17/ffmpeg_rtsp.git
    cd ffmpeg_rtsp 
    mkdir build && cd build && cmake .. && make -j4 && cd ..

### How to use ? ###
* Command: `./rtsp_ffmpeg <input_file> <ouput_file>`
* Example: `./rtsp_ffmpeg rtsp://admin:password@192.168.1.19:554 output.mp4`
* Press CTRL+C to stop and save the stream to a file.

### Features ###
* Command Line Arguments to specify input and output files by the user
* Ability to perform transcoding (supports H264 to H265 conversion)
* Each frame writing time measurement in milliseconds
* Displays output file size at the end of the stream

![Screenshot from 2023-12-12 00-27-03](https://github.com/keshav-c17/ffmpeg_rtsp/assets/76150218/aa6c0dac-82d9-4c6e-b7a3-58cd0cbc04f0)
