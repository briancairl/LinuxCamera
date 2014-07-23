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
///			- OpenCV	[-D WITH_OPENCV]
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


	class LinuxCamera
	{
	private:
		std::string 	name;
		uint8_t	



	};



#endif 	