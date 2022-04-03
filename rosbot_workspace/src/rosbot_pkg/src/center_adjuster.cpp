#inlcude <ros/ros.h>
#include <std_msgs/Header.h>
#include <sensor_msgs/LaserScan.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf/transform_listener.h>

// the purpose of this node is to keep track of the dimensions of the gmapping map
// then when the dimensions change, it will publish the new center value to use in its new node
// it first 

ros::Publisher center_pub;
tf::StampedTransform transform;