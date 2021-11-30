#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

void output_to_file(vector<vector<int>> pixArr, string filename){
    // write a new P2 binary ascii image
    ofstream ostr(filename);
        // write header
    ostr << "P2" << endl;
    // write the rest of the metadata
    ostr << pixArr.size() << " " << pixArr[0].size() << endl << 255 << endl;
    // write data
    int line_ctr = 0;
    for (int i = 0; i < pixArr[0].size(); i++) {
        for (int j = 0; j < pixArr.size(); j++) {
            ostr << pixArr[j][i] << " ";
            // throw in an endl every 20 pixels for readability
            if (++line_ctr % 20 == 0) ostr << endl;
        }
        ostr << endl;
    }
    ostr.close();
}

vector<vector<int>> convert_to_P2(string filename, bool output){
    cout << endl << "Converting: " << filename << endl;
    int width, height, size, graylevels;
    string filetype, comment[5];

    ifstream istr(filename, ios::in | ios::binary);
    if (istr.fail()) return vector<vector<int>>();
    istr >> filetype;
    for (int i = 0; i < 5; ++i){
        istr >> comment[i];
    }
    istr >> width >> height >> graylevels;
    size = width * height;

    cout << "filetype: " << filetype << endl;
    cout << "comment: ";
    for (int i = 0; i < 5; ++i){
        cout << comment[i] << " ";
    }
    cout << endl;
    cout << "width: " << width << endl;
    cout << "height: " << height << endl;
    cout << "greylevels: " << graylevels << endl;
    cout << "size: " << size << endl;

    // parse data values
    int* data = new int[size];
    int fail_tracker = 0; // find which pixel failing on
    for(int* ptr = data; ptr < data+size; ptr++) {
        // read in binary char
        unsigned char t_ch = istr.get();
        // convert to integer
        int t_data = static_cast<int>(t_ch);
        // check if legal pixel
        if(t_data < 0 || t_data > graylevels) { 
            cout << "Failed on pixel: " << fail_tracker << endl;
            cout << "Pixel value: " << t_data << endl;
            return vector<vector<int>>();
        }
        // if passes add value to data array
        *ptr = t_data;
        fail_tracker++;
    }
    // close the stream
    istr.close();

    vector<vector<int>> pixArr(width);
    for (int i = 0; i < width; ++i){
        pixArr[i] = vector<int>(height);
    }
    for (int* ptr = data; ptr < data+size; ptr++) {
        int x = ptr - data;
        int y = x / width;
        int z = x % width;
        // cout << *ptr << endl;
        pixArr[z][y] = *ptr;
        // cout << pixArr[z][y] << endl;
    }
    
    if (output){
        cout << "Writing to file..." << endl;
        // write a new P2 binary ascii image
        string out_filename = filename.substr(0, filename.find_last_of(".")) + "_p2.pgm";
        output_to_file(pixArr, out_filename);
    }
    // clean up
    delete [] data;
    return pixArr;
}

vector<vector<int>> crop_sides(vector<vector<int>> pixArr){
    int width = pixArr.size();
    int height = pixArr[0].size();
    // cout << "width: " << width << endl;
    // cout << "height: " << height << endl;
    int left_bound, right_bound = -1;
    int crop_tolerance = 5;
    for (int i = 0; i < width; ++i){
        int num_black_pixels = 0;
        for (int j = 0; j < height; ++j){
            if (pixArr[i][j] < 50){
                num_black_pixels++;
            }
        }
        if (num_black_pixels > crop_tolerance){
            left_bound = i;
            break;
        }
    }
    for (int i = width-1; i > 0; --i){
        int num_black_pixels = 0;
        for (int j = 0; j < height; ++j){
            if (pixArr[i][j] < 50){
                num_black_pixels++;
            }
        }
        if (num_black_pixels > crop_tolerance){
            right_bound = i;
            break;
        }
    }

    int newWidth = width - left_bound - (width - right_bound);
    // cout << "left_bound: " << left_bound << endl;
    // cout << "right_bound: " << right_bound << endl;
    // cout << "newWidth: " << newWidth << endl;
    vector<vector<int>> newPixArr(newWidth);
    for (int i = 0; i < newWidth; ++i){
        newPixArr[i] = vector<int>(height);
        for (int j = 0; j < height; ++j){
            newPixArr[i][j] = 0;
        }
    }
    int xInd = 0;
    for (int i = 0; i < newWidth; ++i){
        for (int j = 0; j < height; ++j){
            newPixArr[i][j] = pixArr[i+left_bound][j];
        }
    }
    return newPixArr;
}

// takes in a pgm array and an x and y location in the array
// it then returns the radial distance to a gray pixel
// used to determine if this is a significant pixel
int dist_to_gray(vector<vector<int>> pixArr, int x, int y){
    int width = pixArr.size();
    int height = pixArr[0].size();
    for (int i = 1; i < 5; ++i){
        for (int j = -i; j <= i; ++j){
            for (int k = -i; k <= i; ++k){
                // cout << "i: " << i << " j: " << j << " k: " << k << endl;
                int x2 = x+j;
                int y2 = y+k;
                // cout << "x: " << x2 << endl;
                // cout << "y: " << y2 << endl;
                if (x2 < 0 || x2 >= width || y2 < 0 || y2 >= height){
                    continue;
                }
                if (pixArr[x2][y2] == 205){
                    // cout << pixArr[x2][y2] << endl;
                    return i;
                }
            }
        }
    }
    return 1000000;
}

float percent_gray(vector<vector<int>> pixArr, int radius, int x, int y){
    int width = pixArr.size();
    int height = pixArr[0].size();
    int num_gray = 0;
    int num_scanned = 0;
    for (int i = 1; i < radius; ++i){
        for (int j = -i; j <= i; ++j){
            for (int k = -i; k <= i; ++k){
                int x2 = x+j;
                int y2 = y+k;
                if (x2 < 0 || x2 >= width || y2 < 0 || y2 >= height){
                    continue;
                }
                if (pixArr[x2][y2] == 205){
                    num_gray++;
                }
                num_scanned++;
            }
        }
    }
    // cout << "num_gray: " << num_gray << endl;
    // cout << "num_scanned: " << num_scanned << endl;
    return (float)num_gray / (float)num_scanned;
}

float percent_black(vector<vector<int>> pixArr, int radius, int x, int y){
    int width = pixArr.size();
    int height = pixArr[0].size();
    int num_black = 0;
    int num_scanned = 0;
    for (int i = 1; i < radius; ++i){
        for (int j = -i; j <= i; ++j){
            for (int k = -i; k <= i; ++k){
                int x2 = x+j;
                int y2 = y+k;
                if (x2 < 0 || x2 >= width || y2 < 0 || y2 >= height){
                    continue;
                }
                if (pixArr[x2][y2] < 50){
                    num_black++;
                }
                num_scanned++;
            }
        }
    }
    // cout << "num_black: " << num_black << endl;
    // cout << "num_scanned: " << num_scanned << endl;
    return (float)num_black / (float)num_scanned;
}

float percent_white(vector<vector<int>> pixArr, int radius, int x, int y){
    int width = pixArr.size();
    int height = pixArr[0].size();
    int num_white = 0;
    int num_scanned = 0;
    for (int i = 1; i < radius; ++i){
        for (int j = -i; j <= i; ++j){
            for (int k = -i; k <= i; ++k){
                int x2 = x+j;
                int y2 = y+k;
                if (x2 < 0 || x2 >= width || y2 < 0 || y2 >= height){
                    continue;
                }
                if (pixArr[x2][y2] > 205){
                    num_white++;
                }
                num_scanned++;
            }
        }
    }
    // cout << "num_white: " << num_white << endl;
    // cout << "num_scanned: " << num_scanned << endl;
    return (float)num_white / (float)num_scanned;
}


// takes in a pgm array and returns another pgm array with grays and significant pixels
// where a significant pixel is defined as a pixel that has a certain small radial distance
// to a gray pixel
vector<vector<int>> significant_pixel_isolator(vector<vector<int>> pixArr, float threshold){
    int width = pixArr.size();
    int height = pixArr[0].size();
    vector<vector<int>> newPixArr(width);
    for (int i = 0; i < width; ++i){
        newPixArr[i] = vector<int>(height);
        for (int j = 0; j < height; ++j){
            if (pixArr[i][j] == 205){
                newPixArr[i][j] = pixArr[i][j];
            }
            else if (pixArr[i][j] == 0){
                float percent = percent_gray(pixArr, 5, i, j);
                if (percent > threshold){
                    newPixArr[i][j] = 0;
                }
                else{
                    newPixArr[i][j] = 125;
                }
            }
            else {
                newPixArr[i][j] = 254;
            }
        }
    }
    return newPixArr;
}

int main(int argc, char **argv)
{
    auto ground_truth = convert_to_P2("ground_truth.pgm", false);
    // for(int i = 0; i <= 3; i++){
    //     for (int j = 0; j < 3; j++){
    //         ground_truth[i][j] = 0;
    //     }
    // }
    auto ground_truth_cropped = crop_sides(ground_truth);
    // output_to_file(ground_truth_cropped, "ground_truth_cropped.pgm");
    auto pre_remapping = convert_to_P2("pre_remapping.pgm", false);
    auto pre_remapping_cropped = crop_sides(pre_remapping);
    // output_to_file(pre_remapping_cropped, "pre_remapping_cropped.pgm");
    // cout << percent_gray(ground_truth, 5, 0, 0) << endl;
    auto walls = significant_pixel_isolator(pre_remapping_cropped, .3);
    output_to_file(walls, "walls.pgm");
    return 0;
}