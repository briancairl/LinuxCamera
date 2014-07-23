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
///			- OpenCV
///
///	@author Brian Cairl
/// @date 	July, 2014 [rev:1.0]
#ifndef LINUX_CAMERA_HPP
#define LINUX_CAMERA_HPP

	#ifndef LINUX
	#error 	Linux only!
	#endif

	/// C-STD
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
	

	/// SYS
	#include <sys/stat.h>  
	#include <sys/types.h>  
	#include <sys/time.h>  
	#include <sys/mman.h>  
	#include <sys/ioctl.h> 
	

	/// CXX-STD
	#include <string>
	#include <list>


	/// OpenCV Includes
	#include "opencv2/core/core_c.h"  
	#include "opencv2/highgui/highgui_c.h"   


	namespace LC
	{


		typedef enum __LC_PixelFormat__
		{
			MPEG 	= V4L2_PIX_FMT_MJPEG,
			YUYV 	= V4L2_PIX_FMT_YUYV ,
			H264 	= V4L2_PIX_FMT_H264
		} PixelFormat;



		typedef enum __LC_Flags__
		{
 			DeviceOpen 		= 0UL	,
 			DeviceInit 				,
 			MemMapInit 				, 
			Capturing 				,
			ThreadActive 			,
			ReadingFrame		
		} Flags;



		typedef struct __LC_Buffer__ 
		{
			void*   	start;
			size_t 	 	length;
		} Buffer;  


		typedef std::list<cv::IplImage*> FrameBuf;


		class LinuxCamera
		{		
		friend void 		captureLoop( LinuxCamera* cam );
		private:
			int 			fd;
			struct timeval 	tv;

			std::string 	dev_name;
			uint32_t		flags;
			uint16_t 		frame_width;
			uint16_t		frame_height;
			uint16_t 		framerate;
			uint16_t 		timeout;
		
			PixelFormat 	pixel_format;
			Buffer*			buffers;
			FrameBuf		frames;
			uint16_t 		max_size;

			boost::thread* 	proc_thread;

			void 			_InitMMap(void);
			void 			_OpenDevice(void);
			void 			_InitDevice(void);
			void 			_UnInitDevice(void);
			void 			_CloseDevice(void); 
			void 			_StartCapture(void);
			void 			_StopCapture(void);
			void 			_Dispatch();
			void 			_UnDispatch();
			
			void 			_ReadFrame();
			void 			_GrabFrame();
			void 			_StoreFrame(const void *p, int size);
			void 			_RegulateFrameBuffer();

		public:

			LinuxCamera( 
				const char* config_file 
			);
			
			LinuxCamera( 	
				uint16_t 	width 	= 640, 
				uint16_t 	height 	= 480, 
				uint16_t 	fps 	= 30, 
				PixelFormat format 	= PixelFormat::MPEG, 
				const char* dev_name="/dev/video0" 
			);
			
			~LinuxCamera();

			bool 			good();
			bool 			is_open();
			bool 			is_capturing();

			void 			start();
			void 			stop();

			void 			set_max( uint16_t _n );
		};

	}

#endif 	