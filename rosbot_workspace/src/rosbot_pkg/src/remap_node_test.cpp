#include <ros/ros.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/OccupancyGrid.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <math.h>

#define PI 3.14159265

tf::StampedTransform amcl_transform;
tf::StampedTransform gmap_transform;

std::vector<std::vector<int>> amcl_map;
std::vector<std::vector<int>> gmap_map;

int remap_radius = 0;
double resolution = 0.05;
int localization_radius = 0;
double localization_confidence = 0.9;

double map(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void output_to_file(std::vector<std::vector<int>> pixArr, std::string filename){
    // write a new P2 binary ascii image
    std::ofstream ostr(filename);
        // write header
    ostr << "P2" << std::endl;
    // write the rest of the metadata
    ostr << pixArr.size() << " " << pixArr[0].size() << std::endl << 255 << std::endl;
    // write data
    int line_ctr = 0;
    for (int i = pixArr.size()-1; i >= 0; i--) {
        for (int j = 0; j < pixArr[0].size(); j++) {
            int outputVal = 255;
            if (pixArr[i][j] == -1){
                outputVal = 205;
            } else if (pixArr[i][j] >= 80){
                outputVal = 0;
            }
            ostr << outputVal << " ";
            // throw in an endl every 20 pixels for readability
            if (++line_ctr % 20 == 0) ostr << std::endl;
        }
        ostr << std::endl;
    }
    ostr.close();
}

std::vector<std::vector<int>> amcl_get_polar_grid_from_tf(tf::StampedTransform transform, int radius){
    // get the grid from the transform
    std::vector<std::vector<int>> grid;
    int x = transform.getOrigin().x()/resolution + amcl_map[0].size()/2;
    int y = transform.getOrigin().y()/resolution + amcl_map.size()/2;
    tf::Matrix3x3 m(transform.getRotation());
    double roll, pitch, yaw;    // vary from -PI to PI
    m.getRPY(roll, pitch, yaw);
    // yaw = 0;
    for (double angle = -PI+yaw; angle < PI+yaw; angle += 0.01){
        std::vector<int> row;
        for (double dist = 0; dist < radius/resolution; dist += 0.05){
            int x_val = x + (int)dist*cos(angle);
            int y_val = y + (int)dist*sin(angle);
            if (x_val >= 0 && x_val < amcl_map[0].size() && y_val >= 0 && y_val < amcl_map.size()){
                row.push_back(amcl_map[x_val][y_val]);
            } else {
                row.push_back(-1);
            }
        }
        grid.push_back(row);
    }

    // convert polar grid to cartesian grid
    std::vector<std::vector<int>> cartesian_grid;
    for (int i = 0; i < (2*radius)/resolution; i++){
        cartesian_grid.push_back(std::vector<int>((2*radius)/resolution, -1));
    }
    int x_ctr = cartesian_grid[0].size()/2;
    int y_ctr = cartesian_grid.size()/2;
    for (int i = 0; i < grid.size(); i++){
        for (int j = 0; j < grid[0].size(); j++){
            double angle = map((double)i, 0, grid.size(), PI+yaw, -PI+yaw);
            double dist = map((double)j, 0, grid[0].size(), 0, radius/resolution);
            int x_val = x_ctr + (int)dist*cos(angle);
            int y_val = y_ctr + (int)dist*sin(angle);
            if (x_val >= 0 && x_val < cartesian_grid[0].size() && y_val >= 0 && y_val < cartesian_grid.size()){
                cartesian_grid[y_val][x_val] = grid[i][j];
            }
        }
    }

    output_to_file(grid, "grid.pgm");
    output_to_file(cartesian_grid, "grid_rect.pgm");
    return grid;
}

std::vector<std::vector<int>> amcl_get_rect_grid_from_tf(tf::StampedTransform transform, int radius){
    // get the grid from the transform
    std::vector<std::vector<int>> grid;
    if (amcl_map.size() <= 0){
        return grid;
    }
    int x = transform.getOrigin().x()/resolution + amcl_map[0].size()/2;
    int y = (-transform.getOrigin().y())/resolution + amcl_map.size()/2;
    // double yaw = PI/4;
    tf::Matrix3x3 m(transform.getRotation());
    double roll, pitch, yaw;    // vary from -PI to PI
    m.getRPY(roll, pitch, yaw);
    for (int i = -radius/resolution; i < radius/resolution; i++){
        std::vector<int> row;
        for (int j = -radius/resolution; j < radius/resolution; j++){
            int x_val = x + i;
            int y_val = y + j;
            if (x_val >= 0 && x_val < amcl_map[0].size() && y_val >= 0 && y_val < amcl_map.size()){
                row.push_back(amcl_map[x_val][y_val]);
            } else {
                row.push_back(-1);
            }
        }
        grid.push_back(row);
    }

    // rotate the grid by yaw radians
    std::vector<std::vector<int>> rotated_grid;
    for (int i = 0; i < grid.size(); i++){
        std::vector<int> row;
        for (int j = 0; j < grid[0].size(); j++){
            int x_input = j-grid[0].size()/2;
            int y_input = i-grid.size()/2;
            int x_val_sub = (int)(x_input*cos(yaw) - y_input*sin(yaw));
            int y_val_sub = (int)(x_input*sin(yaw) + y_input*cos(yaw));
            int x_val = x_val_sub + grid[0].size()/2;
            int y_val = y_val_sub + grid.size()/2;
            if (x_val >= 0 && x_val < grid[0].size() && y_val >= 0 && y_val < grid.size()){
                row.push_back(grid[y_val][x_val]);
            } else {
                row.push_back(-1);
            }
        }
        rotated_grid.push_back(row);
    }
    output_to_file(grid, "grid_amcl.pgm");
    output_to_file(rotated_grid, "grid_amcl_rot.pgm");
    return grid;
}

std::vector<std::vector<int>> gmap_get_rect_grid_from_tf(tf::StampedTransform transform, int radius){
    // get the grid from the transform
    std::vector<std::vector<int>> grid;
    if (gmap_map.size() <= 0){
        return grid;
    }
    int x = transform.getOrigin().x()/resolution + gmap_map[0].size()/2;
    int y = (-transform.getOrigin().y())/resolution + gmap_map.size()/2;
    // double yaw = -PI/4;
    tf::Matrix3x3 m(transform.getRotation());
    double roll, pitch, yaw;    // vary from -PI to PI
    m.getRPY(roll, pitch, yaw);
    for (int i = -radius/resolution; i < radius/resolution; i++){
        std::vector<int> row;
        for (int j = -radius/resolution; j < radius/resolution; j++){
            int x_val = x + i;
            int y_val = y + j;
            if (x_val >= 0 && x_val < gmap_map[0].size() && y_val >= 0 && y_val < gmap_map.size()){
                row.push_back(gmap_map[x_val][y_val]);
            } else {
                row.push_back(-1);
            }
        }
        grid.push_back(row);
    }

    // rotate the grid by yaw radians
    std::vector<std::vector<int>> rotated_grid;
    for (int i = 0; i < grid.size(); i++){
        std::vector<int> row;
        for (int j = 0; j < grid[0].size(); j++){
            int x_input = j-grid[0].size()/2;
            int y_input = i-grid.size()/2;
            int x_val_sub = (int)(x_input*cos(yaw) - y_input*sin(yaw));
            int y_val_sub = (int)(x_input*sin(yaw) + y_input*cos(yaw));
            int x_val = x_val_sub + grid[0].size()/2;
            int y_val = y_val_sub + grid.size()/2;
            if (x_val >= 0 && x_val < grid[0].size() && y_val >= 0 && y_val < grid.size()){
                row.push_back(grid[y_val][x_val]);
            } else {
                row.push_back(-1);
            }
        }
        rotated_grid.push_back(row);
    }
    output_to_file(grid, "grid_gmap.pgm");
    output_to_file(rotated_grid, "grid_gmap_rot.pgm");
    return grid;
}

void amcl_map_callback(const nav_msgs::OccupancyGrid::ConstPtr& msg){
    // get the map from occupancy grid and store it into a vector of vectors
    amcl_map.clear();
    for (int i = 0; i < msg->info.height; i++) {
        std::vector<int> row;
        for (int j = 0; j < msg->info.width; j++) {
            row.push_back(msg->data[i*msg->info.width + j]);
        }
        amcl_map.push_back(row);
    }
    output_to_file(amcl_map, "amcl_map.pgm");
}

void gmap_map_callback(const nav_msgs::OccupancyGrid::ConstPtr& msg){
    // get the map from the occupancy grid and store it into a vector of vectors
    gmap_map.clear();
    for (int i = 0; i < msg->info.height; i++) {
        std::vector<int> row;
        for (int j = 0; j < msg->info.width; j++) {
            row.push_back(msg->data[i*msg->info.width + j]);
        }
        gmap_map.push_back(row);
    }
    std::cout << gmap_map.size() << std::endl;
    std::cout << gmap_map[0].size() << std::endl;
    output_to_file(gmap_map, "gmap_map.pgm");
}

void printTF(tf::StampedTransform transform)
{
    // tf:Matrix3x3 rot = transform.getBasis();
    tf::Matrix3x3 rot(transform.getRotation());
    double roll, pitch, yaw;
    rot.getRPY(roll, pitch, yaw);
    std::cout << "X: " << transform.getOrigin().x() << ", Y: " << transform.getOrigin().y() << ", R: " << yaw << std::endl;
    // std::cout << "Translation: " << transform.getOrigin().x() << ", " << transform.getOrigin().y() << ", " << transform.getOrigin().z() << std::endl;
    // std::cout << "Rotation: " << transform.getRotation().x() << ", " << transform.getRotation().y() << ", " << transform.getRotation().z() << ", " << transform.getRotation().w() << std::endl;
}

int main(int argc, char **argv){
    ros::init(argc, argv, "remap_node");
    ros::NodeHandle n("~");
    n.param<int>("remap_radius", remap_radius, 2);
    n.param<double>("resolution", resolution, .05);
    n.param<int>("localization_radius", localization_radius, 2);
    n.param<double>("localization_confidence", localization_confidence, .9);
    ros::Subscriber amcl_map_sub = n.subscribe("/map_amcl", 10, amcl_map_callback);
    ros::Subscriber gmap_map_sub = n.subscribe("/map", 10, gmap_map_callback);
    ros::Rate loop_rate(100);
    tf::TransformListener listener;
    while (ros::ok())
    {
        ros::spinOnce();
        try{
            listener.lookupTransform("/base_link_amcl", "/map_amcl", ros::Time(0), amcl_transform);
            listener.lookupTransform("/base_link", "/map_gmapping", ros::Time(0), gmap_transform);
            std::cout << "AMCL: ";
            printTF(amcl_transform);
            std::cout << "GMapping: ";
            printTF(gmap_transform);
            amcl_get_rect_grid_from_tf(amcl_transform, localization_radius);
            gmap_get_rect_grid_from_tf(gmap_transform, localization_radius);
        } catch(tf::TransformException ex){
            ROS_ERROR("%s",ex.what());
            ros::Duration(1.0).sleep();
            continue;
        }
        loop_rate.sleep();
    }
}