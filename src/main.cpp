#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "camera/camera.h"
#include "net/netsend.h"

using namespace std;
using namespace cv;

#define MAX_SLOPE_ERR 3.0f
#define MIN_HORIZ_ERR 20
#define N_CLOSE 10

#define I_DONT_KNOW 0
#define FRAME_SEEN 1
#define FRAME_UNSEEN 2

// This is awful and I should be ashamed of myself.
#define line(...) /*...*/
#define circle(...) /*...*/

void printVec(Vec4i vec) {
  printf("%d, %d to %d, %d\n", vec[0], vec[1], vec[2], vec[3]);
}
int lowerFirstX(Vec4i first, Vec4i second) {
  return first[0] > second[0];
}

Vec4i calc_median(vector<Vec4i> pts) {
  return pts[pts.size()/2];
}

vector<Vec4i> exterminate_in_range(vector<Vec4i> pts, Vec4i mid, int range) {
  vector<Vec4i> results = vector<Vec4i>();
  for (int i = 0; i < pts.size(); i++) {
    if (abs(pts[i][0] - mid[0]) >= range) {
      results.push_back(pts[i]);
    }
  }  

  return results;
}

size_t num_in_range(vector<Vec4i> pts, Vec4i mid, int range) {
  size_t num = 0;
  for (int i = 0; i < pts.size(); i++) {
    if (abs(pts[i][0] - mid[0]) >= range) {
      ++num;
    }
  }  

  return num;
}

int final_out_to_bot(int* last_frames, int num_frames) {
  int num_unknown = 0;
  int num_seen = 0;
  int num_unseen = 0;
  for(int i=0;i<num_frames;i++){
    switch(last_frames[i]) {
      case I_DONT_KNOW:
        num_unknown++;
        break;
      case FRAME_SEEN:
        num_seen++;
        break;
      default:
        num_unseen++;
    }
  }
  if(num_seen>0)
    return FRAME_SEEN;
  if(num_unknown>0)
    return I_DONT_KNOW;
  return FRAME_UNSEEN;
}
void apply_filters(Mat* input) {
    inRange(*input, Scalar(220, 175, 200), Scalar(255,255,255), *input);
    GaussianBlur(*input, *input, Size(9,9), 9, 9);
    Mat kernel = getStructuringElement(MORPH_RECT, Size(30, 30));
    morphologyEx(*input, *input, MORPH_CLOSE, kernel);
}

vector<Vec4i> find_horizontals(Vector<Vec4i> lines) {
  vector<Vec4i> horizontals = vector<Vec4i>();
  for (int i = 0; i < lines.size(); i++) {
    double yd = (lines[i][3] - lines[i][2]);
    double xd = (lines[i][1] - lines[i][0]);
    double slope = yd / xd; 
    if (abs(slope) < MAX_SLOPE_ERR) {
      horizontals.push_back(lines[i]);
    }
  }
  return horizontals;
}
void target_seen(vector<Vec4i> newSet, Vec4i median, int* last_frames, int curr_frame) {
  int num = num_in_range(newSet, median, MIN_HORIZ_ERR);
  if ((num * N_CLOSE) > newSet.size()) {
    last_frames[curr_frame] = FRAME_SEEN; // We've seen the target
    printf("I THINK WE FOUND A HOT ONE! (%d of %lu)\n", num, newSet.size());
  } else {
    last_frames[curr_frame] = FRAME_UNSEEN; // No target seen
  }
}

int main() {
  const int num_frames = 10;
  int curr_frame = 0;
  int last_frames[num_frames]; // Values of last handful of frames
  for (int i = 0; i < num_frames; i++) {
    last_frames[i] = I_DONT_KNOW;
  }

  NetSend n = NetSend();
  send_value = I_DONT_KNOW;

  n.start_server();

  Camera cam;

  bool running = true;
  char buffer[64];
  while (running) {

    Mat frame = cam.getFrame();

    apply_filters(&frame);

    vector<Vec4i> lines;
    vector<Vec4i> horizontals;
    HoughLinesP(frame, lines, 3, CV_PI/5, 50, 30, 10);
    cvtColor(frame, frame, CV_GRAY2BGR);

    horizontals = find_horizontals(lines);

    printf("lines: %lu, horiz: %lu\n", lines.size(), horizontals.size());

    // Algorithm:
    // Find Median of leftmost X values
    // Eliminate lines within range of median
    // Find new median, check if enough points within error range (not done)
    //  if so, goal is hot
    //  else, goal is cold
    //
    if (horizontals.size() > 0) {

      sort(horizontals.begin(), horizontals.end(), lowerFirstX); //Sort our points

      Vec4i median = calc_median(horizontals);
      vector<Vec4i> newSet = exterminate_in_range(horizontals, median, MIN_HORIZ_ERR);
      line(frame, Point(median[0], 0), Point(median[0], frame.size().height), Scalar(0,255,0), 3, 8);
      if (newSet.size() > 0) {
        for (int i = 0; i < newSet.size(); i++) {
          circle(frame, Point(newSet[i][0], newSet[i][1]), 5, Scalar(0, 255, 0));
        }
        // Calculate second median
        median = calc_median(newSet);
        target_seen(newSet, median, last_frames, curr_frame);
      } 
    }
    curr_frame = ++curr_frame % num_frames;

    send_value = final_out_to_bot(last_frames, num_frames);
  }

  return 0;
}
