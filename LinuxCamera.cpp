#include "LinuxCamera.hpp"


#define LC_MSG(str)							("LC_MSG : " str "\n")
#define LC_CLR_BUFFER(x)					memset(&(x), 0, sizeof(x))  
#define LC_MASK(n)							(uint32_t)(1<<n)
#define LC_REG_CMP(reg,mask)				(bool)((reg & mask)==mask)
#define LC_GET_BIT(reg,n) 					(bool)(reg & (1<<n) )
#define LC_SET_BIT(reg,n) 					reg |=  (1<<n)
#define LC_CLR_BIT(reg,n) 					reg &= ~(1<<n)
#define LC_TOG_BIT(reg,n,t)					(t)?(SET_BIT(reg,n)):(CLR_BIT(reg,n))
#define LC_FPS_ADAPTINC						20UL

#define LC_IMG_TS_FMTSTR 					"%s/%_s_%d_%d_%d_%d_%d.%s"			
#defime LC_IMG_TS_FMTLIST(dir,name,n,ts,ext) dir , name, n , ts.hours, ts.min , ts.sec , ts.millis , ext

namespace LC
{

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// UTILS
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::ostream&	operator<<( std::ostream& os, const TimeStamp& ts )
	{
		os 	<< "H : " <<  ts.hours
			<< "M : " <<  ts.mins
			<< "S : " <<  ts.secs
			<< "m : " <<  ts.millis;
		return os;
	}


	std::ostream&	operator<<( std::ostream& os, const LinuxCamera& cam )
	{
		#define BITSTAT(_n) 	(LC_GET_BIT(cam.flags,_n) ? "YES" : "NO")

		os << "=========================================================" 	<< std::endl;
		os 																	<< std::endl;
		os << ":[Configurations]" 											<< std::endl;
		os << "----------------------------------------------------" 		<< std::endl;
		os << "Device Name           : " << cam.dev_name 					<< std::endl;
		os << "Autosave Directory    : " << cam.dir_name 					<< std::endl;
		os << "Pixel Format          : ";
		switch(cam.pixel_format)
		{
			case P_MJPG: os << "MPJG"; break;
			case P_YUYV: os << "YUYV"; break;
			case P_H264: os << "H264"; break;
		}
		os << std::endl;
		os << "Frame Width           : " << cam.frame_width 				<< std::endl;
		os << "Frame Height          : " << cam.frame_height				<< std::endl;
		os << "Framerate             : " << cam.framerate		<< " fps"	<< std::endl;
		os << "Framerate  (Actual)   : " << cam.fps_profile		<< " fps"	<< std::endl;
		os << "Sleep-time (Read)     : " << cam.usleep_len_read	<< " us"	<< std::endl;
		os << "Sleep-time (Idle)     : " << cam.usleep_len_idle	<< " us"	<< std::endl;
		os << "Registered Time-stamp : " << cam.timestamp 					<< std::endl;
		os 																	<< std::endl;
		os << ":[Flags]" 													<< std::endl;
		os << "----------------------------------------------------" 		<< std::endl;
		os << "Device Open           : " << BITSTAT(F_DeviceOpen)			<< std::endl;
		os << "Device Init           : " << BITSTAT(F_DeviceInit)			<< std::endl;
		os << "Memory-Map Init       : " << BITSTAT(F_MemMapInit)			<< std::endl;
		os << "Capturing             : " << BITSTAT(F_Capturing )			<< std::endl;
		os << "Thread Running        : " << BITSTAT(F_ThreadActive)			<< std::endl;
		os << "Reading Frame         : " << BITSTAT(F_ReadingFrame)			<< std::endl;
		os << "AutoSave Enabled      : " << BITSTAT(F_ContinuousSaveMode)	<< std::endl;
		os << "Adaptive-FPS Enabled  : " << BITSTAT(F_AdaptiveFPS)			<< std::endl;
		os << "Backoff Enabled       : " << BITSTAT(F_AdaptiveFPSBackoff)	<< std::endl;
		os << "=========================================================" 	<< std::endl;
		return os;

		#undef BITSTAT
	}


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// HELPERS
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////



	TimeStamp::TimeStamp( const float time_s )
	{
		uint32_t time32 = time_s*1000UL;
		hours 	= time32%3600000UL; 	time32-= 3600000UL	*hours;
		mins 	= time32%60000UL; 		time32-= 60000UL	*mins;
		secs 	= time32%1000UL; 		time32-= 1000UL 	*secs;
		secs 	= time32%1000UL; 		time32-= 1000UL 	*secs;
	}



	TimeStamp( const uint16_t hrs, const uint16_t min, const uint16_t sec, const uint16_t millis )
	{


	}



	static int xioctl(int fh, int request, void *arg) 
	{  
		int r;  

		do 
		{
			r = ioctl(fh, request, arg);
		} while (-1 == r && EINTR == errno);

		return r;
	}



	static void errno_exit(char *s) 
	{  
		fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));  
		exit(EXIT_FAILURE);  
	}



	static void s_capture_loop( LinuxCamera* cam )
	{
		///	Bad camera check
		if( cam==NULL || !cam->is_open() )
		{
			fprintf(stderr, LC_MSG("Could not start capture thread"));  
			exit(EXIT_FAILURE);
		}

		/// Profiling Clock-Start
		cam->_ResetFPSProfile();

		/// So long as setups were successful, keep this running in the background.
		for(;;)
		{
			if( LC_GET_BIT(cam->flags, F_Capturing) )
			{
				/// Grab next frame
				cam->_GrabFrame();

				/// Maintain the frame-buffer (auto-release)
				cam->_RegulateFrameBuffer();

				/// Autosave frames
				cam->_AutoSave();

 				/// Updated Calculated FPS
 				cam->_UpdateFPSProfile();

 				/// Update Adaptive Sleep (Try Match Camera FPS)
 				cam->_UpdateAdaptiveSleep();

				/// Wait Until Next Frame Is Ready
 				usleep(cam->usleep_len_read);
			}
			else
			{
				/// Profiling Clock-Reset
				cam->_ResetFPSProfile();

				/// Sleep...
				usleep(cam->usleep_len_idle);
			}
		}
	}



	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// IO-HANDLING/SETUP
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////



	void LinuxCamera::_InitMMap(void)
	{
		struct v4l2_requestbuffers req;  

		LC_CLR_BUFFER(req);  

		req.count   = 4;  
		req.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
		req.memory  = V4L2_MEMORY_MMAP;  

		if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) 
		{  
			if (EINVAL == errno) 
			{  
				fprintf(stderr, LC_MSG("%s does not support memory mapping"), dev_name.c_str());  
				exit(EXIT_FAILURE);  
			} else {  
				errno_exit("VIDIOC_REQBUFS");  
			}  
		}  

		if (req.count < 2) 
		{  
			fprintf(stderr, LC_MSG("Insufficient buffer memory on %s"),  
			dev_name.c_str());  
			exit(EXIT_FAILURE);  
		}  

		buffers = calloc(req.count, sizeof (*buffers));  

		if (!buffers) 
		{  
			fprintf(stderr, LC_MSG("Out of memory"));  
			exit(EXIT_FAILURE);  
		}  

		for (n_buffers = 0; n_buffers < req.count; ++n_buffers) 
		{  
			struct v4l2_buffer buf;  

			LC_CLR_BUFFER(buf);  

			buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
			buf.memory  = V4L2_MEMORY_MMAP;  
			buf.index   = n_buffers;  

			if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))  
				errno_exit("VIDIOC_QUERYBUF");  

			buffers[n_buffers].length = buf.length;  
			buffers[n_buffers].start =  mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);  

			if (MAP_FAILED == buffers[n_buffers].start)  
				errno_exit("mmap");  
		} 

		LC_SET_BIT(flags,F_MemMapInit);
	}




	void LinuxCamera::_OpenDevice(void) 
	{  
		struct stat st;  

		if (-1 == stat(dev_name.c_str(), &st)) 
		{
			fprintf(stderr, LC_MSG("Cannot identify '%s': %d, %s"),dev_name.c_str(), errno, strerror(errno));
			exit(EXIT_FAILURE);
		}


		if (!S_ISCHR(st.st_mode)) 
		{
			fprintf(stderr, LC_MSG("%s is no device"), dev_name.c_str());
			exit(EXIT_FAILURE);
		}


		fd = open(dev_name.c_str(), O_RDWR /* required */ | O_NONBLOCK, 0);
		if (-1 == fd) 
		{
			fprintf(stderr, LC_MSG("Cannot open '%s': %d, %s"), dev_name.c_str(), errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		LC_SET_BIT(flags,F_DeviceOpen);
	}




	void LinuxCamera::_InitDevice(void)
	{
		struct v4l2_capability 	cap;
		struct v4l2_cropcap 	cropcap;
		struct v4l2_crop 		crop;
		struct v4l2_format 		fmt;
		struct v4l2_streamparm 	frameint;
		uint16_t 				min;

		/* Caculated Frame Wait */
		usleep_len_read = (1000000/framerate);

		/* Timeout. */
		tv.tv_sec 	= timeout;  
		tv.tv_usec  = 0UL;  

		if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) 
		{  
			if (EINVAL == errno) 
			{  
				fprintf(stderr, LC_MSG("%s is no V4L2 device"), dev_name.c_str());  
				exit(EXIT_FAILURE);  
			} 
			else 
			{
				errno_exit("VIDIOC_QUERYCAP");
			}
		}


		if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
		{
			fprintf(stderr, LC_MSG("%s is no video capture device"), dev_name.c_str());
			exit(EXIT_FAILURE);
		}


		if (!(cap.capabilities & V4L2_CAP_STREAMING)) 
		{
			fprintf(stderr, LC_MSG("%s does not support streaming i/o"), dev_name.c_str());
			exit(EXIT_FAILURE);
		}


		/* Select video input, video standard and tune here. */

		LC_CLR_BUFFER(cropcap);

		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) 
		{
			crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			crop.c = cropcap.defrect; /* reset to default */

			if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) 
			{
				switch (errno) 
				{
					case EINVAL:	break;
					default:		break;
				}
			}
		} 
		LC_CLR_BUFFER(fmt);

		///
		/// Force config from file
		///
		fmt.type 				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width 		= frame_width;  
		fmt.fmt.pix.height      = frame_height;  
		fmt.fmt.pix.pixelformat = pixel_format;  
		fmt.fmt.pix.field 		= V4L2_FIELD_INTERLACED;  

		if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))  
			errno_exit("VIDIOC_S_FMT");  

		if (fmt.fmt.pix.pixelformat != pixel_format) 
		{  
			fprintf(stderr,LC_MSG("Libv4l didn't accept pixel format. Can't proceed."));  
			exit(EXIT_FAILURE);  
		}

		LC_CLR_BUFFER(frameint);  

		/* Attempt to set the frame interval. */  
		frameint.type= V4L2_BUF_TYPE_VIDEO_CAPTURE;  
		frameint.parm.capture.timeperframe.numerator    = 1;  
		frameint.parm.capture.timeperframe.denominator  = framerate;  
		if (-1 == xioctl(fd, VIDIOC_S_PARM, &frameint))  
			fprintf(stderr, LC_MSG("Unable to set frame interval."));  

		/* Buggy driver paranoia. */  
		min = fmt.fmt.pix.width * 2;  
		
		if (fmt.fmt.pix.bytesperline < min)  
			fmt.fmt.pix.bytesperline = min;  
		
		min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;  
		
		if (fmt.fmt.pix.sizeimage < min)  
			fmt.fmt.pix.sizeimage = min;  

		LC_SET_BIT(flags,F_DeviceInit);
	}  




	void LinuxCamera::_UnInitDevice(void)
	{
		uint16_t i;

		for (i = 0; i < n_buffers; ++i)
			if (-1 == munmap(buffers[i].start, buffers[i].length))
				errno_exit("munmap");

		free(buffers);

		LC_CLR_BIT(flags,F_DeviceOpen);
	}




	void LinuxCamera::_Dispatch(void)
	{
		if(proc_thread==NULL)
		{
			proc_thread = new boost::thread( s_capture_loop, this );
			proc_thread->detach();

			LC_SET_BIT(flags,F_ThreadActive);
		}
		else
		{
			fprintf(stderr, LC_MSG("Could not dispatch capture thread (_Dispatch attempted)"));
		}
	}




	void LinuxCamera::_UnDispatch(void)
	{
		if(proc_thread && LC_GET_BIT(flags,F_ThreadActive) )
		{
			proc_thread->interrupt();
			delete proc_thread;

			LC_CLR_BIT(flags,F_ThreadActive);
		}
		else
		{
			fprintf(stderr, LC_MSG("Capture not running (_UnDispatch attempted) "));
		}
	}




	void LinuxCamera::_StartCapture(void)
	{
		uint16_t i;
		enum v4l2_buf_type type;  

		for (i = 0; i < n_buffers; ++i) 
		{  
			struct v4l2_buffer buf;  

			LC_CLR_BUFFER(buf);  
			buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
			buf.memory  = V4L2_MEMORY_MMAP;  
			buf.index   = i;  

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))  
				errno_exit("VIDIOC_QBUF");  
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  

		if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))  
			errno_exit("VIDIOC_STREAMON"); 
		
		LC_SET_BIT(flags,F_Capturing);
	}




	void LinuxCamera::_StopCapture(void)
	{
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
			errno_exit("VIDIOC_STREAMOFF");  
		
		LC_CLR_BIT(flags,F_Capturing);
	}




	void LinuxCamera::_CloseDevice(void) 
	{
		if (-1 == close(fd))
			errno_exit("close");

		fd = -1;

		LC_CLR_BIT(flags,F_DeviceOpen);
	}





	void LinuxCamera::_StoreFrame(const void *p, int size)
	{
		// Get raw image from buffer
		CvMat mat = cvMat(frame_height, frame_width,CV_8UC3,(void*)p);  

		// Decode the image  
		frames.push_back(cvDecodeImage(&mat, 1));

		// Count the capture
		++capture_count;
	}
	


	bool LinuxCamera::_ScrollFrameBuffer()
	{
		if( !frames.empty() && frames.front())
		{
			/// Release first image
			cvReleaseImage(&frames.front());

			/// Pop from buffer
			frames.pop_front();

			return true;
		}
		return false;
	}



	void LinuxCamera::_RegulateFrameBuffer()
	{
		while(!LC_GET_BIT(flags,F_ReadingFrame) && frames.size() > max_frames )
			_ScrollFrameBuffer();
	}




	bool LinuxCamera::_ReadFrame(void) 
	{
		struct v4l2_buffer buf;  
		unsigned int i;  

		LC_CLR_BUFFER(buf);  
		buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
		buf.memory  = V4L2_MEMORY_MMAP;  

		if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) 
		{  
			switch (errno) 
			{  
				case EAGAIN:  return 0UL; 
				case EIO:  
				default:  
				errno_exit("VIDIOC_DQBUF");  
			}  
		}  

		assert(buf.index < n_buffers);  

		/// Send frame to Frame-Buffer
		_StoreFrame(buffers[buf.index].start, buf.bytesused);  

		if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) 
			errno_exit("VIDIOC_QBUF");
		else
			return true;

		return false;
	} 




	void LinuxCamera::_GrabFrame(void) 
	{
		uint16_t failures = 0UL;
		while(failures<2UL)
		{
			fd_set fds;  
			FD_ZERO(&fds);  
			FD_SET(fd, &fds);  

			if(-1 == select(fd + 1, &fds, NULL, NULL, &tv) ) 
			{  
				if (EINTR == errno)  
					continue;
				else
					errno_exit("select");  
			}  
			else
			{
				/// Read a frame
				if(_ReadFrame())
				{
					if( LC_GET_BIT(flags,F_AdaptiveFPS) )
					{
						LC_CLR_BIT(flags,F_AdaptiveFPSBackoff);
					}
					return;
				}
				else
				{
					++failures;
				}
			}
		}

		/// Program is running faster than camera can give it frames!
		if( LC_GET_BIT(flags,F_AdaptiveFPS) )
		{
			_BackoffAdaptiveSleep();

			// Backoff Sucka!
			LC_SET_BIT(flags,F_AdaptiveFPSBackoff);
		}
	} 


	void LinuxCamera::_AutoSave()
	{
		if( LC_GET_BIT(flags,F_ContinuousSaveMode))
		{
			char fname_buffer[50UL];
			switch(pixel_format)
			{
				case P_MJPG: sprintf(fname_buffer,LC_IMG_TS_FMTSTR,LC_IMG_TS_FMTLIST(dir_name.c_str(),"auto",capture_count,timestamp,"jpg")); break;
				case P_YUYV: sprintf(fname_buffer,LC_IMG_TS_FMTSTR,LC_IMG_TS_FMTLIST(dir_name.c_str(),"auto",capture_count,timestamp,"yuv")); break;
				case P_H264: sprintf(fname_buffer,LC_IMG_TS_FMTSTR,LC_IMG_TS_FMTLIST(dir_name.c_str(),"auto",capture_count,timestamp,"mkv")); break;
				default: break;
			}
			cvSaveImage(fname_buffer,frames.back(),NULL);
		}
	}



	void LinuxCamera::_ResetFPSProfile()
	{
		/// Reset Epoch
		clock_gettime(CLOCK_MONOTONIC,fps_profile_pts+0UL);
		
		/// Reset Profile Frame Counter
		fps_profile_framecount = 0UL;
	}



	void LinuxCamera::_UpdateFPSProfile()
	{
		/// Profiling Clock-End
		clock_gettime(CLOCK_MONOTONIC,fps_profile_pts+1UL);

		/// Update FPS Profile
		float t_diff = (fps_profile_pts[1].tv_sec - fps_profile_pts[0UL].tv_sec);
		if( t_diff > 0.0f )
			fps_profile = (float)(++fps_profile_framecount)/t_diff;
	}


	void LinuxCamera::_UpdateAdaptiveSleep()
	{
		if( LC_GET_BIT(flags,F_AdaptiveFPS) && (usleep_len_read > LC_FPS_ADAPTINC) )
		{
			if( (uint32_t)fps_profile > framerate  )
				usleep_len_read+=LC_FPS_ADAPTINC;
			else
				usleep_len_read-=LC_FPS_ADAPTINC;
		}
		else
		{
			usleep_len_read = 0UL;
		}
	}


	void LinuxCamera::_BackoffAdaptiveSleep()
	{
		usleep_len_read+=(2*LC_FPS_ADAPTINC);
	}


	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// USER-LEVEL
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////




	LinuxCamera::LinuxCamera() :
		frame_width		(640),
		frame_height	(480),
		framerate		(30),
		pixel_format	(P_MJPG),
		dev_name 		("/dev/video0"),
		n_buffers 		(0UL),
		timeout 		(1UL),
		proc_thread		(NULL),
		flags 			(0UL),
		fd 				(-1),
		max_frames		(5UL),
		capture_count	(0UL),
		usleep_len_idle (10000UL),
		fps_profile		(0.0f)
	{
	}




	LinuxCamera::LinuxCamera( const char* fname ) :
		frame_width		(640),
		frame_height	(480),
		framerate		(30),
		pixel_format	(P_MJPG),
		dev_name 		("/dev/video0"),
		n_buffers 		(0UL),
		timeout 		(1UL),
		proc_thread		(NULL),
		flags 			(0UL),
		fd 				(-1),
		max_frames		(5UL),
		capture_count	(0UL),
		usleep_len_idle (10000UL),
		fps_profile		(0.0f)
	{
		std::ifstream 	fconf(fname);
		std::string 	token;
		bool 			opened = false;

		if(fconf.is_open())
		{
			while(!fconf.eof())
			{
				fconf >> token;
				if(!opened&&token=="-start"	)
				{
					opened = true;
				}
				else
				if(opened&&token=="-end"	)
				{
					/// Close Config
					fconf.close(); 

					/// Init Camera
					_OpenDevice();
					_InitDevice();
					_InitMMap();
					_Dispatch();
					return;
				}
				else
				if(opened&&token=="-dev"	)
				{
					fconf >> dev_name;
				}
				else
				if(opened&&token=="-dir"	)
				{
					fconf >> dir_name;
				}
				else
				if(opened&&token=="-w"		)
				{
					fconf >> frame_width;
				}
				else					
				if(opened&&token=="-h"		)
				{
					fconf >> frame_height;
				}
				else	
				if(opened&&token=="-fps"	)
				{
					fconf >> framerate;
				}
				else	
				if(opened&&token=="-t"		)
				{
					fconf >> timeout;
				}
				else
				if(opened&&token=="-us"		)
				{
					fconf >> usleep_len_idle;
				}
				else
				if(opened&&token=="-fbuf"	)
				{
					fconf >> max_frames;
				}
				else
				if(opened&&token=="-fmt"	)
				{
					fconf >> token;
					if(token=="MJPG")
					{
						pixel_format = P_MJPG;
					}
					else
					if(token=="YUYV")
					{
						pixel_format = P_YUYV;
					}
					else
					if(token=="H264")
					{
						pixel_format = P_H264;
					}
					else
					{
						fprintf(stderr, LC_MSG("Unrecognized pixel format. Supported : [MJPG | YUYV | H264]"), fname, token.c_str() );
						exit(EXIT_FAILURE);
					}
				}
				else
				if(opened&&token=="-autosave")
				{
					LC_SET_BIT(flags,F_ContinuousSaveMode);
				}
				else
				if(opened&&token=="-autofps")
				{
					LC_SET_BIT(flags,F_AdaptiveFPS);
				}
				else
				{
					if(opened)
					{
						fprintf(stderr, LC_MSG("Configuration file '%s' ill-formated. Bad token : %s"), fname, token.c_str() );
						exit(EXIT_FAILURE);
					}
				}	
			}
			fprintf(stderr, LC_MSG("Configuration from file could not find '-start/-end' pair"));
			exit(EXIT_FAILURE);
		}
		else
		{
			fprintf(stderr, LC_MSG("Configuration file '%s' could not be opened."), fname);
			exit(EXIT_FAILURE);
		}
	}




	LinuxCamera::LinuxCamera( 				
		uint16_t 		width,
		uint16_t 		height,
		uint16_t 		fps,
		PixelFormat 	format,
		const char*		dev_name
	) :
		frame_width		(width),
		frame_height	(height),
		framerate		(fps),
		pixel_format	(format),
		dev_name 		(dev_name),
		n_buffers 		(0UL),
		timeout 		(1UL),
		proc_thread		(NULL),
		flags 			(0UL),
		fd 				(-1),
		max_frames		(5UL),
		capture_count	(0UL),
		usleep_len_idle (10000UL),
		fps_profile		(0.0f)
	{
		_OpenDevice();
		_InitDevice();
		_InitMMap();
		_Dispatch();
	}



	LinuxCamera::~LinuxCamera()
	{
		_UnDispatch();

		if( LC_CLR_BIT(flags,F_DeviceInit) )
			_UnInitDevice();

		if( LC_GET_BIT(flags,F_DeviceOpen) )
			_CloseDevice();
	}



	uint16_t 	LinuxCamera::size()				
	{ 
		return 	frames.size();
	}



	bool 	LinuxCamera::good()				
	{ 
		return 	LC_REG_CMP(flags, 
				LC_MASK(F_DeviceOpen) 		| 
				LC_MASK(F_DeviceInit) 		|
				LC_MASK(F_MemMapInit)		|
				LC_MASK(F_ThreadActive)		|
				LC_MASK(F_Capturing)
		);
	}



	bool 	LinuxCamera::is_open()			
	{ 
		return 	LC_REG_CMP(flags, 
				LC_MASK(F_DeviceOpen) 		| 
				LC_MASK(F_DeviceInit) 		|
				LC_MASK(F_MemMapInit)		|
				LC_MASK(F_ThreadActive)
		);
	}



	bool 	LinuxCamera::is_capturing()		
	{ 
		return 	LC_GET_BIT(flags, F_Capturing); 
	}
	


	void 	LinuxCamera::start() 			
	{ 
		_StartCapture(); 	
	}
	


	void 	LinuxCamera::stop() 			
	{ 
		_StopCapture(); 	
	}



	void 	LinuxCamera::set_maxframes( uint16_t _n ) 	
	{ 	
		if(_n == 0) 
		{
			fprintf(stderr,LC_MSG("Frame buffer max-size must be NON-ZERO."));  
			exit(EXIT_FAILURE);  
		}
		else 
			max_frames = _n; 
	}



	void 	LinuxCamera::set_usleep_idle( uint32_t usec )
	{
		if(usec == 0) 
		{
			fprintf(stderr,LC_MSG("Idle-sleep len must be NON-ZERO."));  
			exit(EXIT_FAILURE);  
		}
		else 
			usleep_len_idle = usec; 
	}



	void 	LinuxCamera::set_usleep_read( uint32_t usec )
	{
		if(usec == 0) 
		{
			fprintf(stderr,LC_MSG("Idle-sleep len must be NON-ZERO."));  
			exit(EXIT_FAILURE);  
		}
		else 
			usleep_len_read = usec; 
	}



	void 	LinuxCamera::set_autosavedir( const char* _dir )
	{
		dir_name = _dir;
		boost::filesystem::create_directories(_dir);
	}



	uint32_t LinuxCamera::get_capture_count()
	{
		return capture_count;
	}



	bool 	LinuxCamera::operator++(void)
	{
		LC_CLR_BIT(flags,F_ReadingFrame);
		return _ScrollFrameBuffer();
	}



	IplImage*	LinuxCamera::get_frame()
	{
		LC_SET_BIT(flags,F_ReadingFrame);
		if(frames.empty())
			return NULL;
		else
			return frames.front();
	}



	bool 		LinuxCamera::save_frame(const char* fname)
	{
		if(frames.empty())
			false;
		else
		{
			char fname_buffer[50UL];
			switch(pixel_format)
			{
				case P_MJPG: sprintf(fname_buffer,LC_IMG_TS_FMTSTR,LC_IMG_TS_FMTLIST(dir_name.c_str(),fname,capture_count,timestamp,"jpg")); break;
				case P_YUYV: sprintf(fname_buffer,LC_IMG_TS_FMTSTR,LC_IMG_TS_FMTLIST(dir_name.c_str(),fname,capture_count,timestamp,"yuv")); break;
				case P_H264: sprintf(fname_buffer,LC_IMG_TS_FMTSTR,LC_IMG_TS_FMTLIST(dir_name.c_str(),fname,capture_count,timestamp,"mkv")); break;
				default: break;
			}

			return 	cvSaveImage(fname,frames.back(),NULL);
		}
	}



	void 	LinuxCamera::enable_fps_matching()
	{
		LC_SET_BIT(flags,F_AdaptiveFPS);
	}



	void 	LinuxCamera::disable_fps_matching()
	{
		LC_CLR_BIT(flags,F_AdaptiveFPS);
	}



	bool 	LinuxCamera::operator>>( cv::Mat& mat_out )
	{
		if(!frames.empty())
		{
			LC_SET_BIT(flags,F_ReadingFrame);

			mat_out = cvarrToMat(frames.front());

			cvReleaseImage(frames.front());
			
			frames.pop_front();

			LC_CLR_BIT(flags,F_ReadingFrame);
			
			return true;
		}
		return false;
	}



	void 	LinuxCamera::operator<<( const TimeStamp& ts );
	{
		timestamp = ts;
	}

}
