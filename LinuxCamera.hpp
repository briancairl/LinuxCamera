/// @file  	LinuxCamera.hpp
/// @brief 	An optimized linux camera handler for cpp based on FrameGrabberCV by Matthew Witherwax.		
///
///			Dependencies:
///			=============
///			- v4l1
///			- v4l2
///			- linux/videodev2.h
///			- sys/
///			- unistd.h
///			- boost	
///			- OpenCV 	[-D WITH_OPENCV]
///
///	@author Brian Cairl
/// @date 	July, 2014 [rev:1.0]
#ifndef LINUX_CAMERA_HPP
#define LINUX_CAMERA_HPP

	#ifndef LINUX
	#error 	Linux only!
	#endif

	#include <stdio.h>
	#include <stdlib.h>
	#include <stdint.h>
	#include <signal.h>
	#include <string.h>
	#include <assert.h>
	#include <fcntl.h>
	#include <unistd.h>  
	#include <errno.h>  
	#include <time.h>  

	#include <sys/stat.h>  
	#include <sys/types.h>  
	#include <sys/time.h>  
	#include <sys/mman.h>  
	#include <sys/ioctl.h> 

	#include <string>

	namespace LC
	{

		typedef enum __LC_PixelFormat__
		{
			MPEG = V4L2_PIX_FMT_MJPEG,
			YUYV = V4L2_PIX_FMT_YUYV ,
			H264 = V4L2_PIX_FMT_H264
		} PixelFormat;


		class LinuxCamera
		{
		#define LC_GET_BIT(reg,n) 		(reg&(1<<n))
		#define LC_SET_BIT(reg,n) 		reg|= (1<<n)
		#define LC_CLR_BIT(reg,n) 		reg&=~(1<<n)
		#define LC_TOG_BIT(reg,n,t)		(t)?(SET_BIT(reg,n)):(CLR_BIT(reg,n))
		private:
			std::string 	dev_name;
			uint16_t		flags;
			uint16_t 		frame_width;
			uint16_t		frame_height;
			uint16_t 		framerate;
			uint16_t 		timeout;
			PixelFormat 	pixel_format;

			void 			_OpenDevice();
		public:

			LinuxCamera( const char* config_file );
			LinuxCamera( uint16_t width=640, uint16_t height=480, uint16_t fps=30, PixelFormat format=MPEG, const char* dev_name="/dev/video0" );
			~LinuxCamera();
		};

	}

#endif 	