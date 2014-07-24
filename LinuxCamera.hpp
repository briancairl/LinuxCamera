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

	
	/// Compile with ColorBlob detection support by defualt
	#ifndef LC_WITH_COLORBLOB_SUPPORT
	#define LC_WITH_COLORBLOB_SUPPORT  	1
	#endif 


	/// Default Frame-buffer len
	#ifndef LC_FRAMEBUFFER_LEN
	#define LC_FRAMEBUFFER_LEN 			5
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
	#include <array>
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
		class LinuxCamera;
		class ColorBlob_res;
		class ColorBlob_spec;

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
			F_WritingFrame				,
			F_FrameBufOverflow			,
			F_ContinuousSaveMode		,
			F_AdaptiveFPS				,
			F_AdaptiveFPSBackoff
		} Flags;



		typedef struct __LC_Buffer__ 
		{
			void*   	start;
			size_t 	 	length;
		} Buffer;  


		typedef std::array<IplImage*,LC_FRAMEBUFFER_LEN> FrameBuf;



		/// @brief 	Timestamp support structure with conversion support from floating-point time-count
		class TimeStamp
		{
		friend std::ostream&	operator<<( std::ostream& os, const TimeStamp& ts );
		public:
			uint16_t 	hours;
			uint16_t 	mins;
			uint16_t 	secs;
			uint16_t 	millis;

			TimeStamp();
			TimeStamp( const float time_s );
			TimeStamp( const uint16_t hrs, const uint16_t min, const uint16_t sec, const uint16_t millis );

			~TimeStamp() 
			{}
		};




		#if LC_WITH_COLORBLOB_SUPPORT /*default*/

		/// @brief Color-blob specification struction
		class ColorBlob_spec
		{
		friend class ColorBlob_res;
		private:
			uint8_t 	bgr_target[3UL];
		public:


			/// @brief 	Returns percent matching between the input pixel and the spec parameters (quadratic weighting )
			float operator==( uint8_t pixel[3UL] );


			ColorBlob_spec()
			{
				bgr_target[2UL] = 
				bgr_target[1UL] = 
				bgr_target[0UL] = 0UL;
			}


			ColorBlob_spec( 
				const uint8_t red ,
				const uint8_t blue,
				const uint8_t green

			) {
				bgr_target[2UL] = red;
				bgr_target[1UL] = blue;
				bgr_target[0UL] = green;
			} 

			~ColorBlob_spec() 
			{}
		};



		/// @brief	Weighted-sum based color-blob detection structure
		class ColorBlob_res
		{
		private:
			cv::Point2f pt;
			float 		saturation;	
		public:

			void 		perform( IplImage* img, const ColorBlob_spec& spec ); 

			ColorBlob_res() : 
				saturation(0.0f)
			{}

			~ColorBlob_res() 
			{}
		};

		#endif



		/// @brief	A USB-Camera handle for Linux
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

 			PixelFormat 	pixel_format;
			Buffer*			buffers;

			FrameBuf		frames;
			uint16_t		frame_rd_itr;
			uint16_t		frame_wr_itr;
			uint16_t		frames_avail;

			uint32_t 		usleep_len_idle;
			uint32_t 		usleep_len_read;

			uint16_t 		n_buffers;
			uint32_t		capture_count;

			boost::thread* 	proc_thread;

			TimeStamp 		timestamp;
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
			void 			_ClearFrameBuffer();
			bool 			_ScrollFrameBuffer();

			void 			_ResetFPSProfile();
			void 			_UpdateFPSProfile();
			void 			_UpdateAdaptiveSleep();
			void 			_BackoffAdaptiveSleep();

			#if LC_WITH_COLORBLOB_SUPPORT /*default*/
			ColorBlob_spec	color_blob_spec;
			#endif
		public:

			/// @brief 		Default
			LinuxCamera();


			/// @brief 		Constructor from configuration-file.
			///				Allowed Configuration File Tokens:
			/// 			===================================
			///				+ '-start'		marks the beggining of a configuration profile
			///				+ '-end'		marks the end of a configuration profile
 			/// 			+ '-dev'		device name
			/// 			+ '-w'			frame width
			/// 			+ '-h'			frame height
			/// 			+ '-fps'		framerate
			/// 			+ '-t'			timeout (non-zero)
			/// 			+ '-us'			usleep length (us)
			///				+ '-dir'		auto-save directory specifier
			/// 			+ '-autofps'	allows sleep-cycling to adjust to match camera fps
			/// 			+ '-autosave'	automatically saves each new frame to specified save directory
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


			/// @brief 		Returns the number of framces currently in the frame-buffer
			uint16_t 		size();

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
			void 			enable_fps_matching();

			///	@brief 		Disables adaptive FPS (constant sleep cycling)
			void 			disable_fps_matching();

			///	@brief 		Sets the micro-sleep length (in micro-seconds) when the camera thread is idling
			void 			set_usleep_idle( uint32_t usec );

			///	@brief 		Sets the micro-sleep length (in micro-seconds) between camera reads
			void 			set_usleep_read( uint32_t usec );

			/// @brief 		Sets the auto-save directory
			void 			set_autosavedir( const char* _dir );

			///	@brief		Returns the state of a flag
			///	@param 		qry_flag 	type of flag being queried
			bool 			get_flag( Flags qry_flag );

			///	@brief		Returns the total number of captured frames
			uint32_t 		get_capture_count();

			/// @brief 		[*UNSAFE] Returns the first un-released frame in the frame buffer. The frame must be
			///				release manually after processing using operator++(void)
			///				Safe Alternate : use operator>>(cv::Mat)
			///
			///	@return 	IplImage pointer (handle) to decoded image. 
			IplImage*		get_frame();

			///	@brief 		Saves most recently aquired. 
			///	@param 		fname 		name of the file to be save without extensions (handled with respect to pixel format)
			bool 			save_frame( const char* fname = "" );

			/// @brief 		[*UNSAFE] Releases first un-released frame and removes it from the frame buffer
			///				Safe Alternate : use operator>>(cv::Mat)
			///
			///	@return 	TRUE 	if the frame buffer is not empty
			bool 			operator++(void);

			/// @brief 		[*RECCOMENDED] Generate a safe cv::Mat copy of the current frame. The frame is released 
			///				from the the internal frame-buffer.
			///			
			/// @param 		mat_out 	safe-copy of the current frame as an cv::Mat
			/// @return 	TRUE if the output frame is valid (frame buffer had frames)
			bool 			operator>>( cv::Mat& mat_out );

			/// @brief 		Feeds external timestamp to camera for frame saves
			/// @param 		ts 			time-stamp
			void 			operator<<( const TimeStamp& ts );


			#if LC_WITH_COLORBLOB_SUPPORT /*default*/

			/// @brief 		Locates color blob with respect to registered ColorBlob_spec 
			///				NOTE: Automatically releases current frame after processing.
			///		
			/// @param 		blob_out 	Resulting Color-Blob output
			/// @return 	TRUE if the output structur is valid
			bool 			operator>>( ColorBlob_res& blob_out );


			/// @brief 		Used to register ColorBlob_spec
			/// @param 		blob_spec	ColorBlob_spec to register
			/// @return 	TRUE if the output frame is valid (frame buffer had frames)
			void 			operator<<( ColorBlob_spec& blob_spec );

			#endif
		};
	}

#endif 	