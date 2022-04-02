#include <ros/ros.h>
#include <std_msgs/Header.h>
#include <sensor_msgs/LaserScan.h>
#include <geometry_msgs/PoseStamped.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>

ros::Publisher scan_pub;
tf::StampedTransform transform;

void printTF(tf::Transform transform)
{
    std::cout << "Translation: " << transform.getOrigin().x() << ", " << transform.getOrigin().y() << ", " << transform.getOrigin().z() << std::endl;
    std::cout << "Rotation: " << transform.getRotation().x() << ", " << transform.getRotation().y() << ", " << transform.getRotation().z() << ", " << transform.getRotation().w() << std::endl;
}

void scanCallback(const sensor_msgs::LaserScan::ConstPtr &scan)
{
    sensor_msgs::LaserScan scan_msg;
    scan_msg.header = scan->header;
    scan_msg.header.frame_id = "laser_amcl";
    scan_msg.angle_min = scan->angle_min;
    scan_msg.angle_max = scan->angle_max;
    scan_msg.angle_increment = scan->angle_increment;
    scan_msg.time_increment = scan->time_increment;
    scan_msg.scan_time = scan->scan_time;
    scan_msg.range_min = scan->range_min;
    scan_msg.range_max = scan->range_max;
    scan_msg.ranges = scan->ranges;
    scan_msg.intensities = scan->intensities;
    scan_pub.publish(scan_msg);
}

int main(int argc, char **argv){
    ros::init(argc, argv, "scan_republisher");
    ros::NodeHandle n("~");
    scan_pub = n.advertise<sensor_msgs::LaserScan>("/scan_amcl", 1);
    ros::Subscriber sub = n.subscribe("/scan", 10, scanCallback);
    ros::Rate loop_rate(20);
    tf::TransformListener listener;
    while (ros::ok())
    {
        ros::spinOnce();
        try{
            static tf::TransformBroadcaster br;
            listener.lookupTransform("/base_link", "/laser", ros::Time(0), transform);
            br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "/base_link_amcl", "/laser_amcl"));
            listener.lookupTransform("/base_link", "/imu_link", ros::Time(0), transform);
            br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "/base_link_amcl", "/imu_link_amcl"));
            listener.lookupTransform("/odom", "/base_link", ros::Time(0), transform);
            br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "/odom_amcl", "/base_link_amcl"));
            // printTF(transform);
        }
        catch (tf::TransformException ex){
            // std::cout << "Error caught" << std::endl;
            ROS_ERROR("%s",ex.what());
            ros::Duration(1.0).sleep();
            continue;
        }
        loop_rate.sleep();
    }
}