/**
 * Copyright (c) 2012 Andreas Heider, Julius Adorf, Markus Grimm
 *
 * MIT License (http://www.opensource.org/licenses/mit-license.php)
 */

#include "tpofinder/visualize.h"
#include "tpofinder/util.h"

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;
using namespace std;

namespace tpofinder {
    double positionX,positionY;
    double area;
    double maxArea = 0;
    int indexMaxArea = 0;

    typedef std::vector<cv::Point> Contour;

    char* str2char(string src);

    Mat drawModel(const PlanarModel& model) {
        Mat out = model.views[0].image.clone();
        drawKeypoints(out, model.allKeypoints, out, model.color);

        Mat cvtRoi;
        cvtColor(model.views[0].roi, cvtRoi, CV_GRAY2BGR);
        double alpha = 0.5;
        cvtRoi -= Scalar::all(128);
        addWeighted(out, alpha, cvtRoi, 1.0 - alpha, 0, out);

        string label = str(
                boost::format("%s (%d views, %d features)")
                % model.name % model.views.size() % model.allKeypoints.size());
        drawCenteredText(out, label, Point(out.cols / 2, 30), Scalar::all(255), 1, 0.7);

        return out;
    }

    Mat drawScene(const Scene& scene, bool keypoints) {
        Mat out;
        if (keypoints) {
            drawKeypoints(scene.image, scene.keypoints, out, Scalar::all(255));
        }
        return out;
    }

    void drawMatches(Mat& out, const Scene& scene, const Detection& detection) {
        const PlanarView& view = detection.model.views[0];
        drawMatches(scene.image, scene.keypoints, view.image,
                detection.model.allKeypoints, detection.matches, out);
    }

    /** non-public interface */
    void contourStatistics(const vector<Contour>& contours,
            const vector<Vec4i>& hierarchy,
            Point2i& cmean, Point2i& cmin, Point2i& cmax) {
        int x = 0;
        int y = 0;
        int n = 0;
        cmin.x = INT_MAX;
        cmin.y = INT_MAX;
        cmax.x = INT_MIN;
        cmax.y = INT_MIN;

        if (!contours.empty()) {
            // Just consider top-level
            for (int i = 0; i >= 0; i = hierarchy[i][0]) {
                for (size_t j = 0; j < contours[i].size(); j++) {
                    if (contours[i][j].x < cmin.x) {
                        cmin.x = contours[i][j].x;
                    }
                    if (contours[i][j].x > cmax.x) {
                        cmax.x = contours[i][j].x;
                    }
                    if (contours[i][j].y < cmin.y) {
                        cmin.y = contours[i][j].y;
                    }
                    if (contours[i][j].y > cmax.y) {
                        cmax.y = contours[i][j].y;
                    }
                    x += contours[i][j].x;
                    y += contours[i][j].y;
                    n++;
                }
            }
        }

        if (n > 0) {
            cmean.x = (x /= n);
            cmean.y = (y /= n);
        } else {
            cmean.x = 0;
            cmean.y = 0;
        }
    }

    void drawModelContour(Mat& out, const PlanarModel& model, const Mat& homography, const string& label) {
        Mat tRoi;
        // TODO: Is this the correct size? Need to test this with a model image
        // smaller or larger than the scene image
        warpPerspective(model.views[0].roi, tRoi, homography,
                model.views[0].image.size());
        vector<Contour> contours;
        vector<Vec4i> hierarchy;
        findContours(tRoi, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

      /// Get the moments
        vector<Moments> mu(contours.size() );
        for( int i = 0; i < contours.size(); i++ )
         { mu[i] = moments( contours[i], false ); }

      ///  Get the mass centers:
        vector<Point2f> mc( contours.size() );
        for( int i = 0; i < contours.size(); i++ )
        {  
            if(cv::contourArea(contours[i]) > maxArea){
                indexMaxArea = i;
                maxArea = cv::contourArea(contours[i]);
            }

            mc[i] = Point2f( mu[i].m10/mu[i].m00 , mu[i].m01/mu[i].m00 ); 
        }       

        drawContours(out, contours, -1, model.color, 5);
          for( int i = 0; i< contours.size(); i++ )
        {
            circle( out, mc[i], 4, Scalar(0,255,0), -1, 8, 0 );
        }
        positionX = mu[indexMaxArea].m10/mu[indexMaxArea].m00;
        positionY = mu[indexMaxArea].m01/mu[indexMaxArea].m00;

        if (!label.empty()) {
            Point2i cmean, cmin, cmax;
            contourStatistics(contours, hierarchy, cmean, cmin, cmax);
            drawCenteredText(out, label, Point2i(cmean.x, cmax.y + 20), model.color, 2);
        }
    }

    Point2f objPosition() {
        return Point2f(positionX,positionY);
    }


    void drawDetection(Mat& out, const Detection & detection) {
        vector<KeyPoint> inlierKpts, tKpts;

        BOOST_FOREACH(const DMatch& dm, detection.matches) {
            inlierKpts.push_back(detection.model.allKeypoints[dm.trainIdx]);
        }
        perspectiveTransformKeypoints(inlierKpts, tKpts, detection.homography);
        drawKeypoints(out, tKpts, out, detection.model.color);

        string label = str(boost::format("%s (%d/%d)") % detection.model.name %
                detection.inliers.size() % detection.matches.size());
        cout  << endl << label << endl;
        string chName = "错误";
        if (detection.model.name == "coffee")
            chName = "咖啡"; 
    	else if (detection.model.name == "irbook")
    	    chName = " 红外物理";
        else if (detection.model.name == "game")
            chName = "游戏";
        else if (detection.model.name == "laserbook")
            chName = "激光原理";
        system(str2char("ekho "+ chName));
        drawModelContour(out, detection.model, detection.homography, label);
    }

    void drawCenteredText(Mat& out, const string& text, const Point& org,
            const Scalar& color, int thickness, double fontScale, int fontFace) {
        int baseLine = 0;
        Size sz = getTextSize(text, fontFace, fontScale, thickness, &baseLine);
        putText(out, text, org - Point(sz.width / 2, -sz.height / 2), fontFace,
                fontScale, color, thickness);
    }

    Mat blend(const Mat& img1, const Mat& img2, const Mat& homography) {
        Mat img1_;
        warpPerspective(img1, img1_, homography, Size(img2.cols, img2.rows));
        addWeighted(img1_, 0.5, img2, 0.5, 0, img1_);
        return img1_;
    }

    char* str2char(string src) 
    {
        char *dst = new char[255];
        int i; 
        for(i=0;i <=src.length();i++) 
            dst[i]=src[i]; 
        dst[i] = '\0';
         return dst;
    }


}
