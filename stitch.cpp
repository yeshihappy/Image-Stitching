#ifdef WINDOWS
#include "StdAfx.h"
#endif
#include <iostream>
#include <cv.h>
#include <highgui.h>
#include <vector>
#include "opencv2/stitching/stitcher.hpp"
#include "opencv2/stitching/detail/matchers.hpp"
#include "stitch.h"
#include "gpc.h"
#include "math.h"

#define PI 3.14159

using namespace std;
using namespace cv;
using namespace cv::detail;

ImageWithGPS::ImageWithGPS(){}

ImageWithGPS::ImageWithGPS(Mat image, gpc_polygon gpsPolygon){
  scale = findScale(image, gpsPolygon);
  ang = findAngleGPS(gpsPolygon.contour->vertex[0].x,
              gpsPolygon.contour->vertex[0].y,
              gpsPolygon.contour->vertex[1].x,
              gpsPolygon.contour->vertex[1].y);
}

bool near(double a, double b, double epsilon = 0.001){
  return fabs(a-b) < epsilon;
}

vector<int> ImageWithGPS::gpsToPixels(double lon, double lat){
  vector<int> result;  
  
  int x =(int) (scale * ((lon * cos(ang)) - (lat * sin(ang))));
  int y =(int) (scale * ((lat * sin(ang)) - (lat * cos(ang))));
  result.push_back(x);
  result.push_back(y);
  return result;
}


Mat rotateImage(const Mat &source, double angle, Size size){
  Point2f src_center(source.cols/2.0F, source.rows/2.0F);
  Mat rot_mat = getRotationMatrix2D(src_center,angle, 1.0);
  Mat dst;
  warpAffine(source, dst, rot_mat, source.size());
  return dst;
}

vector<double> getExtremes (gpc_polygon polygon){
    gpc_vertex* vertices = polygon.contour->vertex;
	double minLat = INT_MAX;
	double minLon = INT_MAX;
	double maxLat = INT_MIN;
	double maxLon = INT_MIN;
	vector<double> result;
	for(int i = 0; i < 4; i++){
		if (vertices[i].y < minLon ) minLon = vertices[i].y;
		if (vertices[i].y > maxLon ) maxLon = vertices[i].y;
		if (vertices[i].x < minLat ) minLat = vertices[i].x;
	    if (vertices[i].x > maxLat ) maxLat = vertices[i].x;
	}
	result.push_back(minLat);
	result.push_back(minLon);
	result.push_back(maxLat);
	result.push_back(maxLon);
	return result;
}

void testGetExtremes(){
  cerr << "Testing getExtremes...";
  gpc_vertex bottomLeft{32,-117};
  gpc_vertex bottomRight{32,-116};
  gpc_vertex topRight{33,-116};
  gpc_vertex topLeft{33,-117};
  gpc_vertex vertices[] = {topLeft,topRight,bottomRight,bottomLeft};
  gpc_vertex_list* list = new gpc_vertex_list{4,vertices};
  gpc_polygon polygon{1,0,list};
  vector<double> extremes = getExtremes(polygon);
  assert(extremes[0] == 32); // Min Lat
  assert(extremes[1] == -117); // Min Lon
  assert(extremes[2] == 33); // Max Lat
  assert(extremes[3] == -116); // Max Lon
  cerr <<"Complete\n";
}



double findScale(Mat img, gpc_polygon gpsPoly){
	double scale;
	double x1 = gpsPoly.contour->vertex[0].x;
	double y1 = gpsPoly.contour->vertex[0].y; 
	double x2 = gpsPoly.contour->vertex[1].x;
	double y2 = gpsPoly.contour->vertex[1].y;
	double x3 = gpsPoly.contour->vertex[3].x;
	double y3 = gpsPoly.contour->vertex[3].y; 
	
	double distance_1 = distance(x1,y1,x2,y2);
	double distance_2 = distance(x1,y1,x3,y3);
	double x; double y;
	if(distance_1 >= distance_2){ x = distance_1 ; y = distance_2 ;} 
	else {y = distance_1 ; x = distance_2 ;}

	double img_x = img.cols;
	double img_y = img.rows;
    scale = img_y/y;
	return scale;
}

double testFindScale(){
  cout <<"Testing findScale...";
  cout <<"Complete\n";
}

class GPSFeaturesFinder: public FeaturesFinder {
  public:
    vector<ImageWithGPS> images;
    GPSFeaturesFinder(vector<ImageWithGPS> images){
      this->otherImages = images;
    }
    void find(const Mat &image, ImageFeatures &features){
      (*this)(image,features);
    }
    void operator ()(const Mat &image, ImageFeatures &features) {
      vector<Point2f> gpsData;
      vector<KeyPoint> all;
      ImageWithGPS data;
	  int img_idx;

      
	  for (int j =0; j < otherImages.size(); j++ ){
		  if ( otherImages.at(j).image.data == image.data) { // ?????????
          data = otherImages.at(j);
		  img_idx = j;
          break;
        }
      }

      for (unsigned int i = 0; i< otherImages.size(); i++){
		  if(image.data == otherImages.at(i).image.data) continue;

        gpc_polygon* intersection;
		gpc_polygon_clip( GPC_INT, &data.gpsPolygon, &otherImages[i].gpsPolygon,intersection);
		vector<double> coord = getExtremes(data.gpsPolygon);

       float maxLon = (float) coord.back(); coord.pop_back();
	   float maxLat = (float) coord.back(); coord.pop_back();
	   float minLon = (float) coord.back(); coord.pop_back();
	   float minLat = (float) coord.back(); coord.pop_back();

        vector<int> ul = data.gpsToPixels(maxLon, minLat);
        vector<int> ur = data.gpsToPixels(maxLon, maxLat);
        vector<int> bl = data.gpsToPixels(minLon, minLat);
        vector<int> br = data.gpsToPixels(minLon, maxLat);

        Point2f ulPoint = Point2f((float)ul[0], (float)ul[1]);
        Point2f urPoint = Point2f((float)ur[0], (float)ur[1]);
        Point2f blPoint = Point2f((float)bl[0], (float)bl[1]);
        Point2f brPoint = Point2f((float)br[0], (float)br[1]);

        KeyPoint ulKeyPt = KeyPoint(ulPoint, 1);
        KeyPoint urKeyPt = KeyPoint(urPoint, 1);
        KeyPoint blKeyPt = KeyPoint(blPoint, 1);
        KeyPoint brKeyPt = KeyPoint(brPoint, 1);

        all.push_back(ulKeyPt);
        all.push_back(urKeyPt);
        all.push_back(blKeyPt);
        all.push_back(brKeyPt);

        gpsData.push_back(Point2f (maxLon,minLat));
        gpsData.push_back(Point2f (maxLon,maxLat));
        gpsData.push_back(Point2f (minLon, minLat));
        gpsData.push_back(Point2f (minLon,maxLat));
      }

      Mat descriptors(all.size(),2,CV_32FC1);

      for(unsigned int i =0; i < gpsData.size(); i++){
        descriptors.push_back(gpsData[i].x);
        descriptors.push_back(gpsData[i].y);
      }
	  
      features.img_idx = img_idx;
      features.img_size =  image.size();
      features.keypoints = all;
      features.descriptors = descriptors;
    }
private:
	vector<ImageWithGPS> otherImages;
};

double toDegrees(double radians){
  return radians / PI * 180.0;
}


double distance(double x1, double y1, double x2, double y2){
  return sqrt(pow(x2-x1,2)+pow(y2-y1,2));
}

double testDistance(){
  cerr <<"Testing distance...";
  assert(near(distance(0,0,3,4),5));
  cerr<<"Complete\n";
}

double findAngleGPS(double lat1, double lon1, double lat2, double lon2){
  double dlat = lat2-lat1;
  double dlon = lon2-lon1;
  return toDegrees(atan(dlat/dlon));
}

void testFindAngleGPS(){
  cerr <<"Testing findAngle...";
  assert(near(findAngleGPS(0,0,5,5),45.0));
  assert(near(findAngleGPS(0,0,3,10),16.699));
  cerr <<"Complete\n";
}
// for simple testing, not include gpspolygon
vector<ImageWithGPS> getTestDataForImage(Mat image,
    int rows,
    int columns,
    double horizontalOverlap,
    double verticalOverlap,
    double scale){
  vector<ImageWithGPS> resultImages = vector<ImageWithGPS>(rows * columns);
  int normalWidth = image.cols / columns;
  int normalHeight = image.rows / rows;
  int overlapWidth = (int)((double)(normalWidth) * horizontalOverlap);
  int overlapHeight = (int)((double)(normalHeight) * verticalOverlap); 
  cout << "Original Width: " << image.cols << "\n";
  cout << "Original Height: " << image.rows << "\n";
  cout << "Normal Width: " << normalWidth <<"\n";
  cout << "Normal Height: " << normalHeight <<"\n";
  cout << "Overlap Width: " << overlapWidth <<"\n";
  cout << "Overlap Height: " << overlapHeight <<"\n";
  cout << endl;

  int imageX, imageY, imageWidth, imageHeight;
  for (int j = 0; j < rows; j++){
    for (int i = 0; i < columns; i++){
      imageX = max(i * normalWidth - overlapWidth,0);
      if (i == 0){
        imageWidth = min(normalWidth + overlapWidth, image.cols);
      } else {
        imageWidth = min(normalWidth + 2 * overlapWidth,image.cols - imageX);
      }
      imageY = max(j * normalHeight - overlapHeight,0);
      if (i == 0){
        imageHeight = min(normalHeight + overlapHeight, image.rows);
      } else {
        imageHeight = min(normalHeight + 2 * overlapHeight ,image.rows - imageY);
      }
      cout <<"Image "<<j * rows + i<<"\n";
      cout <<"X: "<<imageX<<"\n";
      cout <<"Y: "<<imageY<<"\n";
      cout <<"Width: "<<imageWidth<<"\n";
      cout <<"Height: "<<imageHeight<<"\n";
      cout <<endl;
      Mat result = Mat(image,Range(imageY, imageY+imageHeight),Range(imageX,imageX +imageWidth));

      vector<double> ul; ul.push_back(imageY*scale); ul.push_back(imageX*scale);
      vector<double> ur; ur.push_back(imageY*scale); ur.push_back((imageX+imageWidth)*scale);
      vector<double> br; br.push_back((imageY+imageHeight)*scale); br.push_back(imageX+imageWidth*scale);
      vector<double> bl; bl.push_back((imageY+imageHeight)*scale); bl.push_back(imageX*scale);
      vector<vector<double> > coords; coords.push_back(ul); coords.push_back(ur);coords.push_back(br); coords.push_back(bl); 
      //resultImages[rows *j +i] = ImageWithGPS(result,coords);	  
    }
  }
  return resultImages;
}
// 
ImageWithGPS iterativeStitch(ImageWithGPS accumulatedImage, vector<ImageWithGPS> newImages) {
  Mat result;
  //Rect_<double> rect = accumulatedImage.rect;
  gpc_polygon poly = accumulatedImage.gpsPolygon;

  vector<Mat> newVec(newImages.size()+1);
  /*for(unsigned int i =0; i < newImages.size(); i++){
    if(newImages[i].rect.x < rect.x)
      rect.x = newImages[i].rect.x;
    if(newImages[i].rect.y < rect.y)
      rect.y = newImages[i].rect.y;
    if(newImages[i].rect.height+newImages[i].rect.y > rect.y+rect.height)
      rect.height = newImages[i].rect.height+newImages[i].rect.y-rect.y;
    if(newImages[i].rect.width+newImages[i].rect.x > rect.x+rect.width)
      rect.width = newImages[i].rect.width+newImages[i].rect.x-rect.x;
    newVec[i] = newImages[i].image;
  }*/
  Stitcher stitcher = Stitcher::createDefault(true);

  newVec.push_back(accumulatedImage.image);
  stitcher.stitch(newVec, result);
  return ImageWithGPS(result,poly);
}

int main(){

  testGetExtremes();
  testFindAngleGPS();
  testDistance();

  ImageWithGPS accumulator, pano;
  vector<ImageWithGPS> images = getTestDataForImage(imread("image.jpg"),2,2,0.2,0.2,0.9);
  imwrite("a.jpg",images[0].image);
  imwrite("b.jpg",images[1].image);
  imwrite("c.jpg",images[2].image);
  imwrite("d.jpg",images[3].image);
  vector<Mat> _images;
  _images.push_back(images[0].image);
  _images.push_back(images[1].image);
  _images.push_back(images[2].image);
  _images.push_back(images[3].image);
  Stitcher stitcher = stitcher.createDefault(true);
  stitcher.setFeaturesFinder(cv::Ptr<FeaturesFinder>(new GPSFeaturesFinder(images)));
  imwrite("result.jpg",pano.image);

}


