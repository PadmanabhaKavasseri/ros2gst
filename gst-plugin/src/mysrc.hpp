

#ifndef __GST_MYSRC_HPP__
#define __GST_MYSRC_HPP__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/base/gstdataqueue.h>


#include <iostream>

#include <thread>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <sensor_msgs/msg/image.hpp>
// #include <mutex>
#include <opencv2/opencv.hpp>


G_BEGIN_DECLS

#define GST_TYPE_MYSRC (gst_my_src_get_type())
G_DECLARE_FINAL_TYPE (GstMySrc, gst_my_src,
    GST, MYSRC, GstPushSrc)
  
struct _GstMySrc
{
  GstPushSrc parent;

  GstPad *srcpad;

  gboolean silent;

  // GstDataQueue *data_queue;
};

// Class structure declaration
struct _GstMySrcClass
{
  GstPushSrcClass parent_class; // Parent class is GstPushSrcClass
};

G_END_DECLS

#endif /* __GST_MYFILTER_H__ */

//  export GST_PLUGIN_PATH=/data/gst/gst-template/bld/gst-plugin/

//ros2 topic pub /my_topic std_msgs/msg/String '{data: "Hello, ROS2!"}' -r 1      
//export GST_DEBUG=2,mysrc:5,waylandsink:5 
//gst-launch-1.0 mysrc ! videoconvert ! video/x-raw,format=RGB16 ! waylandsink
//gst-launch-1.0 mysrc ! videoconvert ! pngenc ! multifilesink location=/home/ubuntu/frames/frame_%05d.png

