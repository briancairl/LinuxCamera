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
///
///	@todo	Time-sequenced constant-capture mode?
/// @todo	Video streaming mode? (With OpenCV VideoWriter)
///
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
		friend void 		s_capture_loop( LinuxCamera* cam );
		private:
			int 			fd;
			struct timeval 	tv;

			std::string 	dev_name;
			uint32_t		flags;
			uint16_t 		frame_width;
			uint16_t		frame_height;
			uint16_t 		framerate;
			uint16_t 		timeout;
			uint32_t 		usleep_len;
		
			PixelFormat 	pixel_format;
			Buffer*			buffers;
			FrameBuf		frames;
			uint16_t 		max_size;
			uint32_t		capture_count;

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
			bool 			_ScrollFrameBuffer();
		public:

			/// @brief 		Default
			LinuxCamera();


			/// @brief 		Constructor from XML-style configuration-file
			///	@param 		fname 		config file name (*.conf)
			LinuxCamera( 
				const char* fname 
			);
			

			/// @brief 		Primary 	Constructor
			///	@param 		width 		image frame width 						(default is 640 pixels)
			///	@param 		height		image frame height 						(default is 480 pixels)
			///	@param 		fps 		image capture rate from camera device 	(default is 30  fps)
			///	@param 		ormat 		pixel formate @see LC::PixelFormat 		(default is MPEG)
			///	@param 		dev_name	device name 							(default is '/dev/video0')
			LinuxCamera( 	
				uint16_t 	width 	= 640, 
				uint16_t 	height 	= 480, 
				uint16_t 	fps 	= 30, 
				PixelFormat format 	= PixelFormat::MPEG, 
				const char* dev_name= "/dev/video0"
			);
			

			/// @brief Deconstructor
			~LinuxCamera();


			/////////////////////////////////////////////////////////////////////////////////////////////////////////////
			/// Statuses
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////

			/// @brief 		Returns TRUE if camera device was opened successfully and is current capturing (in-thread)
			bool 			good();

			/// @brief 		Returns TRUE if camera device was opened successfully
			bool 			is_open();

			// @brief 		Returns TRUE if camera is current capturing
			bool 			is_capturing();


			/////////////////////////////////////////////////////////////////////////////////////////////////////////////
			/// Capture Controls
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////

			// @brief 		Starts frame capturing
			void 			start();

			/// @brief 		Stops frame capturing
			void 			stop();


			/////////////////////////////////////////////////////////////////////////////////////////////////////////////
			/// Interfaces
			/////////////////////////////////////////////////////////////////////////////////////////////////////////////

			///	@brief 		Sets the micro-sleep length (in micro-seconds) when the camera thread is idling
			void 			set_usleep( uint32_t _n );

			/// @brief 		Sets the max length of the frame-capture buffer
			void 			set_max( uint16_t 	_n );

			///	@brief		Returns the total number of captured frames
			uint32_t 		get_capture_count();

			/// @brief 		Returns the first un-released frame in the frame buffer
			IplImage*		get_frame();

			///	@brief 		Saves the first un-released frame to a file
			bool 			save_frame( const char* fname );

			/// @brief 		Releases first un-released frame and removes it from the frame buffer
			///	@return 	TRUE 	if the frame buffer is not empty
			bool 			operator++(void);
		};

	}

#endif 	