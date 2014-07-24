LinuxCamera
===========

An optimized camera-driver library based on Matthew Witherwax's FrameGrabberCV. This 
frame-grabber impelmentation is to be used as a replacment for OpenCV's VideoCapture
structure.

+ Important:
As the name suggests : Linux support ONLY!


## Dependencies
+ V4L[1|2]
+ Boost
+ OpenCV


## Configuration Files

If you choose to setup a camera from a configuration file (reccomended), there are several
structural rules to be followed in generating said file.

An example configuration file for the camera object appears as follows : 

-start
	-dev 		/dev/video0
	-dir 		home/brian/myFrames
	-fmt		MJPG
	-w			640
	-h 			480
	-fps		30
	-t			1
	-us 		10000
	-autosave
	-autofps
-end

All usable tags are shown in the above example. Tags can appear in any order, but the '-start' 
tag MUST appear first, and the '-end' tag last. The configuration file path is specified as an 
argurment to the 'LinuxCamera::LinuxCamera(const char* fname)' constructor, which automatically 
loads the configuration file. 

Fields that are not configured explicity are defaulted internally.

All tags/values must be delimited by standard whitespace characters.


