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
///	@todo	operator>> implementation for cv::Mat conversion
///
#ifndef LINUX_CAMERA_HPP
#define LINUX_CAMERA_HPP

	#ifndef __GNUC__
	#error 	GNU-Linux only!
	#endif

	/// C-STD
	#include <stdio.h>  
	#include <stdlib.h>  
	#include <signal.h>  
	#include <string.h>  
	#include <assert.h>  
	#include <getopt.h>
	#include <fcntl.h>
	#include <unistd.h>  
	#include <errno.h>  
		

	/// SYS
	#include <sys/stat.h>  
	#include <sys/types.h>  
	#include <sys/time.h>  
	#include <sys/mman.h>  
	#include <sys/ioctl.h> 
	

	/// LINUX
	#include <linux/videodev2.h>
	

	/// CXX-STD
	#include <string>
	#include <list>
	#include <fstream>
	#include <iostream>


	/// OpenCV Includes
	#include "opencv/cv.h"  
	#include "opencv2/core/core_c.h"  
	#include "opencv2/highgui/highgui_c.h"   


	/// Boost Includes
	#include <boost/thread.hpp>
	#include <boost/filesystem.hpp>


	namespace LC
	{


		typedef enum __LC_PixelFormat__
		{
			P_MJPG 	= V4L2_PIX_FMT_MJPEG,
			P_YUYV 	= V4L2_PIX_FMT_YUYV ,
			P_H264 	= V4L2_PIX_FMT_H264
		} PixelFormat;



		typedef enum __LC_Flags__
		{
 			F_DeviceOpen 		= 0UL	,
 			F_DeviceInit 				,
 			F_MemMapInit 				, 
			F_Capturing 				,
			F_ThreadActive 				,
			F_ReadingFrame				,
			F_ContinuousSaveMode		,
			F_AdaptiveFPS				,
			F_AdaptiveFPSBackoff
		} Flags;



		typedef struct __LC_Buffer__ 
		{
			void*   	start;
			size_t 	 	length;
		} Buffer;  


		typedef std::list<IplImage*> FrameBuf;




		class LinuxCamera
		{		
		friend void 			s_capture_loop( LinuxCamera* cam );
		friend std::ostream&	operator<<( std::ostream& os, const LinuxCamera& cam );
		private:
			int 			fd;
			struct timeval 	tv;

			std::string 	dev_name;
			std::string 	dir_name;
			uint32_t		flags;
			uint16_t 		frame_width;
			uint16_t		frame_height;
			uint16_t 		framerate;
			uint16_t 		timeout;
			uint32_t 		usleep_len_idle;
			uint32_t 		usleep_len_read;
 			PixelFormat 	pixel_format;
			Buffer*			buffers;
			FrameBuf		frames;
			uint16_t 		n_buffers;
			uint16_t 		max_size;
			uint32_t		capture_count;

			boost::thread* 	proc_thread;

			float 			fps_profile;
			uint32_t 		fps_profile_framecount;
			struct timespec fps_profile_pts[2UL];

			void 			_InitMMap(void);
			void 			_OpenDevice(void);
			void 			_InitDevice(void);
			void 			_UnInitDevice(void);
			void 			_CloseDevice(void); 
			void 			_StartCapture(void);
			void 			_StopCapture(void);
			void 			_Dispatch();
			void 			_UnDispatch();
			
			void 			_AutoSave();
			bool 			_ReadFrame();
			void 			_GrabFrame();
			void 			_StoreFrame(const void *p, int size);
			void 			_RegulateFrameBuffer();
			bool 			_ScrollFrameBuffer();

			void 			_ResetFPSProfile();
			void 			_UpdateFPSProfile();
			void 			_UpdateAdaptiveSleep();
			void 			_BackoffAdaptiveSleep();
		public:

			/// @brief 		Default
			LinuxCamera();


			/// @brief 		Constructor from configuration-file.
			///				Allowed Configuration File Tokens:
			/// 			===================================
			///				+ '-start'	marks the beggining of a configuration profile
			///				+ '-end'	marks the end of a configuration profile
 			/// 			+ '-dev'	device name
			/// 			+ '-w'		frame width
			/// 			+ '-h'		frame height
			/// 			+ '-fps'	framerate
			/// 			+ '-t'		timeout (non-zero)
			/// 			+ '-us'		usleep length (us)
			/// 			+ '-fbuf'	max frame buffer size (non-zero)
			///
			///	@param 		fname 		config file name (*.conf)
			LinuxCamera( 
				const char* 		fname 
			);
			

			/// @brief 		Primary 	Constructor
			///	@param 		width 		image frame width 						(default is 640 pixels)
			///	@param 		height		image frame height 						(default is 480 pixels)
			///	@param 		fps 		image capture rate from camera device 	(default is 30  fps)
			///	@param 		ormat 		pixel formate @see LC::PixelFormat 		(default is MPEG)
			///	@param 		dev_name	device name 							(default is '/dev/video0')
			LinuxCamera( 	
				uint16_t 	width,
				uint16_t 	height,
				uint16_t 	fps,
				PixelFormat format,
			 	const char*	dev_name
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


			///	@brief 		Enables adaptive FPS (adapt sleep cycling to match cameras FPS)
			void 			enable_adaptive_fps();

			///	@brief 		Disables adaptive FPS (constant sleep cycling)
			void 			disable_adaptive_fps();

			///	@brief 		Sets the micro-sleep length (in micro-seconds) when the camera thread is idling
			void 			set_usleep( uint32_t _n );

			/// @brief 		Sets the max length of the frame-capture buffer
			void 			set_max( uint16_t 	_n );

			/// @brief 		Sets the auto-save directory
			void 			set_dir( const char* _dir );

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