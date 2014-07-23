#include <LinuxCamera.hpp>


#define LC_MSG(str)		("LC_MSG : " str "\n")

namespace LC
{

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// IO-HANDLING/SETUP
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
	}


	void LinuxCamera::_InitDevice(void)
	{
		struct v4l2_capability 	cap;
		struct v4l2_cropcap 	cropcap;
		struct v4l2_crop 		crop;
		struct v4l2_format 		fmt;
		struct v4l2_streamparm 	frameint;
		unsigned int min;

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

		CLEAR(cropcap);

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
		CLEAR(fmt);

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

		if (fmt.fmt.pix.pixelformat != UserParams::pixel_format) 
		{  
			fprintf(stderr,"Libv4l didn't accept pixel format. Can't proceed.\n");  
			exit(EXIT_FAILURE);  
		}

		CLEAR(frameint);  

		/* Attempt to set the frame interval. */  
		frameint.type= V4L2_BUF_TYPE_VIDEO_CAPTURE;  
		frameint.parm.capture.timeperframe.numerator    = 1;  
		frameint.parm.capture.timeperframe.denominator  = UserParams::fps;  
		if (-1 == xioctl(fd, VIDIOC_S_PARM, &frameint))  
			fprintf(stderr, "Unable to set frame interval.\n");  

		/* Buggy driver paranoia. */  
		min = fmt.fmt.pix.width * 2;  
		
		if (fmt.fmt.pix.bytesperline < min)  
			fmt.fmt.pix.bytesperline = min;  
		
		min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;  
		
		if (fmt.fmt.pix.sizeimage < min)  
			fmt.fmt.pix.sizeimage = min;  

		/// [MMAP Tear-down]
		init_mmap(); 
	}  






	/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// USER-LEVEL
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////

	LinuxCamera( uint16_t width, uint16_t height, uint16_t fps, const char* dev_name ):
		frame_width	(width),
		frame_height(height),
		framerate	(fps),
		dev_name 	(dev_name),
		timeout 	(1UL)
	{
		_OpenDevice();
	}

}
