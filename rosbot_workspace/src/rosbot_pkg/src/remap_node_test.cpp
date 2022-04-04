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
std::vector<std::vector<int>> amcl_grid;
std::vector<std::vector<int>> gmap_grid;

int remap_radius = 0;
double resolution = 0.05;
int localization_radius = 0;
double localization_confidence = 0.6;
int localization_count_threshold = 10;
int localization_count = 0;
bool localized = false;

double amcl_center_x = 0;
double amcl_center_y = 0;
double gmap_center_x = 0;
double gmap_center_y = 0;

void printTF(tf::Transform transform)
{
    tf::Matrix3x3 m(transform.getRotation());
    double roll, pitch, yaw;    // vary from -PI to PI
    m.getRPY(roll, pitch, yaw);
    std::cout << "X: " << transform.getOrigin().x() << ", Y: " << transform.getOrigin().y() << ", R: " << yaw << std::endl;
    // std::cout << "Rotation: " << transform.getRotation().x() << ", " << transform.getRotation().y() << ", " << transform.getRotation().z() << ", " << transform.getRotation().w() << std::endl;
}

double percent_black_calc(std::vector<std::vector<int>>& map, int x, int y, int radius, int probability_threshold)
{
    int black = 0;
    int num_tested = 0;
    for (int i = -radius; i <= radius; i++)
    {
        for (int j = -radius; j <= radius; j++)
        {
            int x_val = x + i;
            int y_val = y + j;
            if (x_val < 0 || x_val >= map.size() || y_val < 0 || y_val >= map[0].size())
            {
                continue;
            }
            num_tested++;
            if (map[x + i][y + j] >= probability_threshold)
            {
                black++;
            }
        }
    }
    return (double)black / (double)(num_tested);
}

double map(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void output_to_file(std::vector<std::vector<int>> pixArr, std::string filename){
    // write a new P2 binary ascii image
    std::ofstream ostr(filename);
        // write header
    ostr << "P2" << std::endl;
    // write the rest of the metadata
    ostr << pixArr[0].size() << " " << pixArr.size() << std::endl << 255 << std::endl;
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

void amcl_get_rect_grid_from_tf(tf::StampedTransform transform, int radius){
    // get the grid from the transform
    std::vector<std::vector<int>> grid;
    if (amcl_map.size() <= 0){
        return;
    }
    int x = (transform.getOrigin().x())/resolution + amcl_center_x/resolution;
    int y = (transform.getOrigin().y())/resolution + amcl_center_y/resolution;
    tf::Matrix3x3 m(transform.getRotation());
    double roll, pitch, yaw;    // vary from -PI to PI
    m.getRPY(roll, pitch, yaw);
    for (int i = -radius/resolution; i < radius/resolution; i++){
        std::vector<int> row;
        for (int j = -radius/resolution; j < radius/resolution; j++){
            int x_val = x + j;
            int y_val = y + i;
            if (x_val >= 0 && x_val < amcl_map[0].size() && y_val >= 0 && y_val < amcl_map.size()){
                row.push_back(amcl_map[y_val][x_val]);
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
    // return rotated_grid;
    amcl_grid = rotated_grid;
}

void gmap_get_rect_grid_from_tf(tf::StampedTransform transform, int radius){
// get the grid from the transform
    std::vector<std::vector<int>> grid;
    if (gmap_map.size() <= 0){
        return;
    }
    int x = (transform.getOrigin().x())/resolution + gmap_center_x/resolution;
    int y = (transform.getOrigin().y())/resolution + gmap_center_y/resolution;
    // double yaw = PI/4;
    tf::Matrix3x3 m(transform.getRotation());
    double roll, pitch, yaw;    // vary from -PI to PI
    m.getRPY(roll, pitch, yaw);
    for (int i = -radius/resolution; i < radius/resolution; i++){
        std::vector<int> row;
        for (int j = -radius/resolution; j < radius/resolution; j++){
            int x_val = x + j;
            int y_val = y + i;
            if (x_val >= 0 && x_val < gmap_map[0].size() && y_val >= 0 && y_val < gmap_map.size()){
                row.push_back(gmap_map[y_val][x_val]);
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
    gmap_grid = rotated_grid;
}

void update_amcl_map(std::vector<std::vector<int>> in_grid, tf::StampedTransform transform){
    if (amcl_map.size() <= 0 || in_grid.size() <= 0){
        return;
    }
    int x = ((transform.getOrigin().x())/resolution + amcl_center_x/resolution)-in_grid.size()/2;
    int y = ((transform.getOrigin().y())/resolution + amcl_center_y/resolution)-in_grid[0].size()/2;
    tf::Matrix3x3 m(transform.getRotation());
    double roll, pitch, yaw;    // vary from -PI to PI
    m.getRPY(roll, pitch, yaw);
    // unrotate the grid by yaw radians
    std::vector<std::vector<int>> unrotated_grid;
    for (int i = 0; i < in_grid.size(); i++){
        std::vector<int> row;
        for (int j = 0; j < in_grid[0].size(); j++){
            int x_input = j-in_grid[0].size()/2;
            int y_input = i-in_grid.size()/2;
            int x_val_sub = (int)(x_input*cos(yaw) + y_input*sin(yaw));
            int y_val_sub = (int)(-x_input*sin(yaw) + y_input*cos(yaw));
            int x_val = x_val_sub + in_grid[0].size()/2;
            int y_val = y_val_sub + in_grid.size()/2;
            if (x_val >= 0 && x_val < in_grid[0].size() && y_val >= 0 && y_val < in_grid.size()){
                row.push_back(in_grid[y_val][x_val]);
            } else {
                row.push_back(-1);
            }
        }
        unrotated_grid.push_back(row);
    }
    // update the amcl map
    // std::vector<std::vector<int>> test_map(unrotated_grid.size(), std::vector<int>(unrotated_grid[0].size(), 99));
    // unrotated_grid = test_map;
    std::vector<std::vector<int>> updated_amcl_map = amcl_map;
    for (int i = 0; i < unrotated_grid.size(); i++){
        for (int j = 0; j < unrotated_grid[0].size(); j++){
            int x_val = x + j;
            int y_val = y + i;
            if (x_val >= 0 && x_val < amcl_map[0].size() && y_val >= 0 && y_val < amcl_map.size()){
                updated_amcl_map[y_val][x_val] = unrotated_grid[i][j];
            }
        }
    }
    output_to_file(updated_amcl_map, "updated_amcl_map.pgm");
    output_to_file(unrotated_grid, "unrotated_grid.pgm");
    std::cout << "updated amcl map" << std::endl;
}

void amcl_map_callback(const nav_msgs::OccupancyGrid::ConstPtr& msg){
    // get the map from occupancy grid and store it into a vector of vectors
    std::cout << "Getting AMCL map" << std::endl;
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
    // std::cout << "gmap_map_callback" << std::endl;
    // std::cout << msg->info.height << " " << msg->info.width << std::endl;
    for (int i = 0; i < msg->info.height; i++) {
        std::vector<int> row;
        for (int j = 0; j < msg->info.width; j++) {
            row.push_back(msg->data[i*msg->info.width + j]);
        }
        gmap_map.push_back(row);
    }
    // std::cout << gmap_map.size() << std::endl;
    // std::cout << gmap_map[0].size() << std::endl;
    output_to_file(gmap_map, "gmap_map.pgm");

    // get the rotated grid from the gmap, then make a new map with only the overlap between the two maps
    gmap_get_rect_grid_from_tf(gmap_transform, localization_radius);
    amcl_get_rect_grid_from_tf(amcl_transform, localization_radius);
    std::vector<std::vector<int>> intersect_map;
    int pixels_black_gmap = 0;
    int pixels_black_intersect = 0;
    int probability_threshold = 50;
    for (int i = 0; i < 2*localization_radius/resolution; i++){
        std::vector<int> row;
        for (int j = 0; j < 2*localization_radius/resolution; j++){
            if (gmap_grid[i][j] >= probability_threshold){
                pixels_black_gmap++;
                int testing_radius = 1;
                double percent_black = percent_black_calc(amcl_grid, i, j, testing_radius, probability_threshold);
                int num_black_amcl = (int)((2*testing_radius+1)*percent_black);
                if (num_black_amcl > 0){
                    pixels_black_intersect++;
                    row.push_back(gmap_grid[i][j]);
                }
            } else if (gmap_grid[i][j] == -1 || amcl_grid[i][j] == -1){
                row.push_back(-1);
            } else {row.push_back(0);}
        }
        intersect_map.push_back(row);
    }
    double percent_intersect = (double)pixels_black_intersect/(double)pixels_black_gmap;
    std::cout << "percent_intersect: " << percent_intersect << std::endl;
    if (percent_intersect >= localization_confidence){
        if (localization_count+1 <= localization_count_threshold+5){
            localization_count++;
            std::cout << "localization_count: " << localization_count << std::endl;
        }
    } else {
        if (localization_count-1 >= 0){
            localization_count--;
            std::cout << "localization_count: " << localization_count << std::endl;
        }
    }
    output_to_file(intersect_map, "intersect_map.pgm");

    // revising the amcl map 
    if (localization_count >= localization_count_threshold){
        update_amcl_map(intersect_map, amcl_transform);
    }
}

void amcl_mapmetadata_callback(const nav_msgs::MapMetaData::ConstPtr& msg){
    amcl_center_x = -1*msg->origin.position.x;
    amcl_center_y = -1*msg->origin.position.y;
    // std::cout << "amcl_center_x: " << amcl_center_x << std::endl;
    // std::cout << "amcl_center_y: " << amcl_center_y << std::endl;
}

void gmap_mapmetadata_callback(const nav_msgs::MapMetaData::ConstPtr& msg){
    gmap_center_x = -1*msg->origin.position.x;
    gmap_center_y = -1*msg->origin.position.y;
    // std::cout << "gmap_center_x: " << gmap_center_x << std::endl;
    // std::cout << "gmap_center_y: " << gmap_center_y << std::endl;
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
    n.param<double>("localization_confidence", localization_confidence, .6);
    ros::Subscriber amcl_map_sub = n.subscribe("/map_amcl", 10, amcl_map_callback);
    ros::Subscriber gmap_map_sub = n.subscribe("/map", 10, gmap_map_callback);
    ros::Subscriber amcl_mapmetadata_sub = n.subscribe("/map_amcl_metadata", 10, amcl_mapmetadata_callback);
    ros::Subscriber gmap_mapmetadata_sub = n.subscribe("/map_metadata", 10, gmap_mapmetadata_callback);
    ros::Rate loop_rate(100);
    tf::TransformListener listener;
    while (ros::ok())
    {
        ros::spinOnce();
        try{
            listener.lookupTransform("/map_amcl", "/base_link_amcl", ros::Time(0), amcl_transform);
            listener.lookupTransform("/map_gmapping", "/base_link", ros::Time(0), gmap_transform); // change the first tf to map if problems
            // std::cout << "AMCL: ";
            // printTF(amcl_transform);
            // std::cout << "GMapping: ";
            // printTF(gmap_transform);
            // amcl_get_rect_grid_from_tf(amcl_transform, localization_radius);
            // gmap_get_rect_grid_from_tf(gmap_transform, localization_radius);
        } catch(tf::TransformException ex){
            ROS_ERROR("%s",ex.what());
            ros::Duration(1.0).sleep();
            continue;
        }
        loop_rate.sleep();
    }
}