
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "mysrc.hpp"

GST_DEBUG_CATEGORY_STATIC (gst_my_src_debug);
#define GST_CAT_DEFAULT gst_my_src_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
};

static GstStaticPadTemplate src_factory = 
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_my_src_parent_class parent_class
G_DEFINE_TYPE (GstMySrc, gst_my_src, GST_TYPE_PUSH_SRC);

static void gst_my_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_my_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_my_src_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_my_src_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);


// Define the ROS publisher and node
std::shared_ptr<rclcpp::Node> node;
rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher;
rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription;
int cnt = 0;

// Function to run in the new thread
void ros_publish_thread() {
  rclcpp::Rate rate(1);  // 1 Hz
  while (rclcpp::ok()) {
    auto message = std_msgs::msg::String();
    message.data = "Hello from GStreamer plugin!";
    publisher->publish(message);
    rate.sleep();
  }
}

// Callback function to handle received messages
void ros_message_callback(const std_msgs::msg::String::SharedPtr msg) {
  // GST_INFO("Received message: %s", msg->data.c_str());
  std::cout << "Received message: " << msg->data << " " << cnt++ << std::endl;
}

// Function to run ROS spin in a separate thread
void ros_spin_thread() {
  rclcpp::spin(node);
}

static gboolean
gst_my_src_start (GstBaseSrc * src)
{
  //I think
  //This function should initialize all ROS related subscribers and start the threads to receive data from them 
  return TRUE;
}

static GstFlowReturn
gst_my_src_fill (GstPushSrc * src, GstBuffer * buf)
{
  //I think
  //This function should be called by the ros subscriber cb right?
  //because it is a pushsrc.. so whenever some ROS gets a camera frame, it pushes the frame into the pipeline. 
  //this function should somehow receive a ROS message. Not sure how to do that. 
  return GST_FLOW_OK;
}

/* initialize the myfilter's class */
static void
gst_my_src_class_init (GstMySrcClass * klass)
{
  // GObjectClass *gobject_class;
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));


  gobject_class->set_property = gst_my_src_set_property;
  gobject_class->get_property = gst_my_src_get_property;

  base_src_class->start = GST_DEBUG_FUNCPTR(gst_my_src_start);
  push_src_class->fill = GST_DEBUG_FUNCPTR(gst_my_src_fill);
  //todo add dispose




  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple(gstelement_class,
    "ROS-SRC",
    "ROS2 Source",
    "Subscribes to ROS2 sensor messages, converts data to gst buffers, and pushes them into pipeline",
    "Padmanabha Kavasseri <<pkavasseri@gmail.com>>");


}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_my_src_init (GstMySrc * mysrc)
{
  mysrc->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (mysrc->srcpad);
  gst_element_add_pad (GST_ELEMENT (mysrc), mysrc->srcpad);

  mysrc->silent = FALSE;
}

static void
gst_my_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMySrc *src = GST_MYSRC (object);

  switch (prop_id) {
    case PROP_SILENT:
      src->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_my_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMySrc *src = GST_MYSRC (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, src->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_my_src_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMySrc *src;
  gboolean ret;

  src = GST_MYSRC (parent);

  GST_LOG_OBJECT (src, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_my_src_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstMySrc *src;

  src = GST_MYSRC (parent);

  if (src->silent == FALSE){
    // g_print ("Loaded!");
    // Now we can use iostream C++:
    // std::cout << "Test1" << std::endl;
  }

  /* just push out the incoming buffer without touching it */
  return gst_pad_push (src->srcpad, buf);
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
mysrc_init (GstPlugin * mysrc)
{
  
  // Initialize ROS
  rclcpp::init(0, nullptr);
  node = std::make_shared<rclcpp::Node>("my_src_node");
  // publisher = node->create_publisher<std_msgs::msg::String>("my_topic", 10);
  subscription = node->create_subscription<std_msgs::msg::String>(
    "/my_topic",
    10,
    ros_message_callback
  );

  // Create and detach the thread
  // std::thread pub_thread(ros_publish_thread);
  // pub_thread.detach();
  // Create and detach the thread for ROS spin
  std::thread spin_thread(ros_spin_thread);
  spin_thread.detach();

  GST_DEBUG_CATEGORY_INIT (gst_my_src_debug, "mysrc",
      0, "Template mysrc");

  return gst_element_register (mysrc, "mysrc", GST_RANK_NONE,
      GST_TYPE_MYSRC);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstmysrc"
#endif


#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.19.0.1"
#endif
/* gstreamer looks for this structure to register myfilters
 *
 * exchange the string 'Template myfilter' with your myfilter description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    mysrc,
    "my_src",
    mysrc_init,
    PACKAGE_VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)

