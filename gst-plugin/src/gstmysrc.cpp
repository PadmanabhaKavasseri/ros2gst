
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
    GST_STATIC_CAPS (
        "video/x-raw, "
        "format=(string)RGB, "
        "width=(int)1280, "
        "height=(int)800, "
        "framerate=(fraction)30/1"
    )
);

#define gst_my_src_parent_class parent_class
G_DEFINE_TYPE (GstMySrc, gst_my_src, GST_TYPE_PUSH_SRC);

static void gst_my_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_my_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

// Define the ROS publisher and node
std::shared_ptr<rclcpp::Node> node;
rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher;
rclcpp::Subscription<std_msgs::msg::String>::SharedPtr string_sub;
rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub;
GstDataQueue *data_queue;
std::mutex data_queue_mutex;
bool flag = true;

int cnt = 0;

// Function to check if the queue is full
gboolean custom_check_full(GstDataQueue *queue, guint visible, guint bytes, guint64 time, gpointer checkdata) {
    return visible > 100; // Example condition: Full if visible items > 100
}

void custom_full_callback(GstDataQueue *queue, gpointer user_data) {
    g_print("Queue is full! Handle logic here.\n");
}

void custom_empty_callback(GstDataQueue *queue, gpointer user_data) {
    g_print("Queue is empty! Handle logic here.\n");
}

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
void ros_message_callback(const sensor_msgs::msg::Image::SharedPtr msg) {
    std::cout << "Received message: " << cnt++ << std::endl;

    const uint8_t *image_data = msg->data.data();
    size_t image_size = msg->data.size();
    std::cout << "image_size: " << image_size << std::endl;

    //print random numbers from frame
    // size_t sample_pixels = std::min((size_t)10, image_size); // Check first 10 pixels
    // for (size_t i = 0; i < sample_pixels; ++i) {
    //     std::cout << "Pixel " << i << ": " << (int)image_data[i] << std::endl;
    // }

    //save image to file using openCV
    // if(flag){
    //   cv::Mat frame(msg->height, msg->width, CV_8UC3, const_cast<uint8_t*>(msg->data.data()));
    //   cv::imwrite("/home/ubuntu/frames/test_frame.jpg", frame);
    //   flag = false;
    // }
    // std::cout << "image data 14: " << image_data[14] << std::endl;


    GstBuffer *buffer = gst_buffer_new_allocate(NULL, image_size, NULL);
    if (!buffer) {
        std::cerr << "Failed to allocate GstBuffer" << std::endl;
        return;
    }

    // Copy image data into the GstBuffer
    GstMapInfo map_info;
    if (gst_buffer_map(buffer, &map_info, GST_MAP_WRITE)) {
        memcpy(map_info.data, image_data, image_size);
        gst_buffer_unmap(buffer, &map_info);
    } else {
        std::cerr << "Failed to map GstBuffer for writing" << std::endl;
        gst_buffer_unref(buffer);
        return;
    }

    // Create a GstDataQueueItem  
    GstDataQueueItem *item = g_slice_new0(GstDataQueueItem);
    item->object = GST_MINI_OBJECT(buffer);
    item->size = gst_buffer_get_size(buffer);
    item->visible = TRUE;
    item->duration = GST_CLOCK_TIME_NONE;



    // only for reading
    GstMapInfo map_info1;
    if (gst_buffer_map(buffer, &map_info1, GST_MAP_READ)) {
        std::cout << "ROS CB FUNC:: map data 14: " << (int)map_info1.data[14] << std::endl;
        gst_buffer_unmap(buffer, &map_info1);
    } else {
        std::cerr << "Failed to map GstBuffer for writing" << std::endl;
        gst_buffer_unref(buffer);
        return;
    }






    // Push the item onto the queue
      // std::lock_guard<std::mutex> lock(data_queue_mutex);
    if (!gst_data_queue_push(data_queue, item)) {
        std::cerr << "Failed to push item into the queue!" << std::endl;
        gst_buffer_unref(buffer);
        g_slice_free(GstDataQueueItem, item);
        return;
    }


    std::cout << "Item successfully pushed into the queue!" << std::endl;

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

  // Initialize ROS
  std::cout << "/arm_camera/color/image_raw" << std::endl;
  rclcpp::init(0, nullptr);
  auto qos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
  std::string topic = "/arm_camera/color/image_raw";
  node = std::make_shared<rclcpp::Node>("my_src_node");
  // publisher = node->create_publisher<std_msgs::msg::String>("my_topic", 10);
  image_sub = node->create_subscription<sensor_msgs::msg::Image>(
    topic,
    qos,
    ros_message_callback
  );

  // Create and detach the thread
  // std::thread pub_thread(ros_publish_thread);
  // pub_thread.detach();
  // Create and detach the thread for ROS spin
  std::thread spin_thread(ros_spin_thread);
  spin_thread.detach();

  return TRUE;
}

//old imp
// static GstFlowReturn gst_my_src_fill(GstPushSrc *src, GstBuffer *buf) {
//   GstDataQueueItem *item;
//   gboolean success = gst_data_queue_pop(data_queue, &item);
//   std::cout << "here" << std::endl;

//   if (!success) {
//       std::cerr << "Queue is empty, returning GST_FLOW_EOS" << std::endl;
//       return GST_FLOW_EOS;
//   }
//   else {
//     std::cout << "NOT EMPTY" << std::endl;
//   }
//   std::cout << "before dest" << std::endl;
//   buf = gst_buffer_ref (GST_BUFFER (item->object));
//   std::cout << "Buffer size: " << gst_buffer_get_size(buf) << " bytes" << std::endl;
//   std::cout << "fill func buffer data 14: " << (int)buf->data[14] << std::endl;

//   std::cout << "after gst_bufref " << std::endl;
//   // item->destroy (item);
//   // std::cout << "after destroy %GST_PTR_FORMAT" << buf << std::endl;
//   g_print("after destroy " GST_PTR_FORMAT "\n", buf);


//   return GST_FLOW_OK;
// }

static GstFlowReturn gst_my_src_fill(GstPushSrc *src, GstBuffer *buf) {
    GstDataQueueItem *item;
    gboolean success = gst_data_queue_pop(data_queue, &item);

    if (!success) {
        std::cerr << "Queue is empty, returning GST_FLOW_EOS" << std::endl;
        return GST_FLOW_EOS;
    }

    buf = gst_buffer_ref(GST_BUFFER(item->object));
    GstMapInfo map1;

    int width = 1280; // Set the actual width of the frame
    int height = 800; // Set the actual height of the frame
    int channels = 3; // Number


    // Map the buffer to access its memory
    if (gst_buffer_map(buf, &map1, GST_MAP_READ)) {
        std::cout << "Buffer size: " << map1.size << " bytes" << std::endl;
        int expected_size = width * height * channels;
        if (map1.size != expected_size) {
            std::cerr << "Buffer size mismatch! Expected: " << expected_size
                      << ", Actual: " << map1.size << std::endl;
            gst_buffer_unmap(buf, &map1);
            return GST_FLOW_ERROR;
        }

        // // Assuming the data in the buffer is raw RGB or grayscale image data.
        
        // // Create an OpenCV matrix
        //how i print out the msg 
        // cv::Mat frame(msg->height, msg->width, CV_8UC3, const_cast<uint8_t*>(msg->data.data()));

        if(flag){
          cv::Mat frame(height, width, CV_8UC3);
          std::memcpy(frame.data, map1.data, map1.size);
          std::cout << "map.data put into openCV frame" << std::endl;
          cv::imwrite("/home/ubuntu/frames/output_frame.jpg", frame); 
          flag = false;
        }

        // Print the value at the 14th byte (if the buffer is large enough)
        if (map1.size > 14) {
            std::cout << "FILL FUNC:: Value at position 14: " << static_cast<int>(map1.data[14]) << std::endl;
        } else {
            std::cerr << "Buffer is smaller than 14 bytes!" << std::endl;
        }

        // Unmap the buffer after you're done
        gst_buffer_unmap(buf, &map1);
    } else {
        std::cerr << "Failed to map buffer memory!" << std::endl;
    }

    return GST_FLOW_OK;
}

static void gst_my_src_dispose(GObject *gobject)
{
    GstMySrc *mysrc = GST_MYSRC(gobject);

    if (mysrc->srcpad) {
      g_print("Disposing srcpad...\n");
      if (GST_IS_PAD(mysrc->srcpad)) {
          g_print("Removing and unreffing srcpad\n");
          gst_element_remove_pad(GST_ELEMENT(mysrc), mysrc->srcpad);
          g_object_unref(mysrc->srcpad);
      } 
      else {
        g_print("srcpad is not a valid GstPad object\n");
      }
      mysrc->srcpad = NULL;
    } 
    else {
        g_print("srcpad is already NULL\n");
    }


    // Call the parent class's dispose method
    G_OBJECT_CLASS(parent_class)->dispose(gobject);
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

  base_src_class->start = GST_DEBUG_FUNCPTR(gst_my_src_start); // called from ready to paused 
  push_src_class->fill = GST_DEBUG_FUNCPTR(gst_my_src_fill);
  gobject_class->dispose = GST_DEBUG_FUNCPTR(gst_my_src_dispose);




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
  std::cout << "adding src pad" << std::endl;
  // mysrc->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  // GST_PAD_SET_PROXY_CAPS (mysrc->srcpad);
  // gst_element_add_pad (GST_ELEMENT (mysrc), mysrc->srcpad);

  mysrc->silent = FALSE;

  data_queue = gst_data_queue_new(custom_check_full, custom_full_callback, custom_empty_callback, mysrc);
  if (!data_queue) {
      std::cerr << "Failed to initialize global data queue" << std::endl;
  }

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

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
mysrc_init (GstPlugin * mysrc)
{
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

