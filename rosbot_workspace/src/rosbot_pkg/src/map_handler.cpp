#include <ros/ros.h>
#include <nav_msgs/OccupancyGrid.h>
#include <nav_msgs/Odometry.h>
#include <nav_msgs/MapMetaData.h>
#include <nav_msgs/GetMap.h>

nav_msgs::OccupancyGrid curr_map;
nav_msgs::MapMetaData curr_map_metadata;
ros::Publisher map_pub;
ros::Publisher map_metadata_pub;

bool new_map = false;

void amcl_updated_map_callback(const nav_msgs::OccupancyGrid::ConstPtr& msg){
    // curr_map.header = msg->header;
    // curr_map.info = msg->info;
    // curr_map.data = msg->data;
    // map_pub.publish(curr_map);
    // new_map = true;
}

int main(int argc, char **argv){
    ros::init(argc, argv, "map_handler");
    ros::NodeHandle n("~");
    map_pub = n.advertise<nav_msgs::OccupancyGrid>("/map_amcl", 1);
    map_metadata_pub = n.advertise<nav_msgs::MapMetaData>("/map_amcl_metadata", 1);
    ros::Subscriber updated_map_sub = n.subscribe("/map_amcl_updated", 10, amcl_updated_map_callback);
    nav_msgs::GetMap srv;
    ros::Rate loop_rate(10);
    while (ros::ok()){
        ros::spinOnce();
        if (!new_map){
            ros::ServiceClient client = n.serviceClient<nav_msgs::GetMap>("/static_map");
            if (client.call(srv)){
                curr_map = srv.response.map;
            } else{
                std::cout << "Getting map failed" << std::endl << std::endl;
            }
        }
        loop_rate.sleep();
    }
}