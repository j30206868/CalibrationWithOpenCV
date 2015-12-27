#include <iostream>
#include <sstream>
#include <time.h>
#include <stdio.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "Source.cpp"

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif

using namespace cv;
using namespace std;

static void help()
{
    cout <<  "This is a camera calibration sample." << endl
         <<  "Usage: calibration configurationFile"  << endl
         <<  "Near the sample file you'll find the configuration file, which has detailed help of "
             "how to edit it.  It may be any OpenCV supported file format XML/YAML." << endl;
}
class Settings
{
public:
    Settings() : goodInput(false) {}
    enum Pattern { NOT_EXISTING, CHESSBOARD, CIRCLES_GRID, ASYMMETRIC_CIRCLES_GRID };
    enum InputType {INVALID, CAMERA, VIDEO_FILE, IMAGE_LIST};

    void write(FileStorage& fs) const                        //Write serialization for this class
    {
        fs << "{" << "BoardSize_Width"  << boardSize.width
                  << "BoardSize_Height" << boardSize.height
                  << "Square_Size"         << squareSize
                  << "Calibrate_Pattern" << patternToUse
                  << "Calibrate_NrOfFrameToUse" << nrFrames
                  << "Calibrate_FixAspectRatio" << aspectRatio
                  << "Calibrate_AssumeZeroTangentialDistortion" << calibZeroTangentDist
                  << "Calibrate_FixPrincipalPointAtTheCenter" << calibFixPrincipalPoint

                  << "Write_DetectedFeaturePoints" << bwritePoints
                  << "Write_extrinsicParameters"   << bwriteExtrinsics
                  << "Write_outputFileName"  << outputFileName

                  << "Show_UndistortedImage" << showUndistorsed

                  << "Input_FlipAroundHorizontalAxis" << flipVertical
                  << "Input_Delay" << delay
                  << "Input" << input
           << "}";
    }
    void read(const FileNode& node)                          //Read serialization for this class
    {
        node["BoardSize_Width" ] >> boardSize.width;
        node["BoardSize_Height"] >> boardSize.height;
        node["Calibrate_Pattern"] >> patternToUse;
        node["Square_Size"]  >> squareSize;
        node["Calibrate_NrOfFrameToUse"] >> nrFrames;
        node["Calibrate_FixAspectRatio"] >> aspectRatio;
        node["Write_DetectedFeaturePoints"] >> bwritePoints;
        node["Write_extrinsicParameters"] >> bwriteExtrinsics;
        node["Write_outputFileName"] >> outputFileName;
        node["Calibrate_AssumeZeroTangentialDistortion"] >> calibZeroTangentDist;
        node["Calibrate_FixPrincipalPointAtTheCenter"] >> calibFixPrincipalPoint;
        node["Input_FlipAroundHorizontalAxis"] >> flipVertical;
        node["Show_UndistortedImage"] >> showUndistorsed;
        node["Input"] >> input;
        node["Input_Delay"] >> delay;
        interprate();
    }
    void interprate()
    {
        goodInput = true;
        if (boardSize.width <= 0 || boardSize.height <= 0)
        {
            cerr << "Invalid Board size: " << boardSize.width << " " << boardSize.height << endl;
            goodInput = false;
        }
        if (squareSize <= 10e-6)
        {
            cerr << "Invalid square size " << squareSize << endl;
            goodInput = false;
        }
        if (nrFrames <= 0)
        {
            cerr << "Invalid number of frames " << nrFrames << endl;
            goodInput = false;
        }

        if (input.empty())      // Check for valid input
                inputType = INVALID;
        else
        {
            if (input[0] >= '0' && input[0] <= '9')
            {
                stringstream ss(input);
                ss >> cameraID;
                inputType = CAMERA;
            }
            else
            {
                if (readStringList(input, imageList))
                    {
                        inputType = IMAGE_LIST;
                        nrFrames = (nrFrames < (int)imageList.size()) ? nrFrames : (int)imageList.size();
                    }
                else
                    inputType = VIDEO_FILE;
            }
            if (inputType == CAMERA)
                inputCapture.open(cameraID);
            if (inputType == VIDEO_FILE)
                inputCapture.open(input);
            if (inputType != IMAGE_LIST && !inputCapture.isOpened())
                    inputType = INVALID;
        }
        if (inputType == INVALID)
        {
            cerr << " Inexistent input: " << input;
            goodInput = false;
        }

        flag = 0;
        if(calibFixPrincipalPoint) flag |= CV_CALIB_FIX_PRINCIPAL_POINT;
        if(calibZeroTangentDist)   flag |= CV_CALIB_ZERO_TANGENT_DIST;
        if(aspectRatio)            flag |= CV_CALIB_FIX_ASPECT_RATIO;


        calibrationPattern = NOT_EXISTING;
        if (!patternToUse.compare("CHESSBOARD")) calibrationPattern = CHESSBOARD;
        if (!patternToUse.compare("CIRCLES_GRID")) calibrationPattern = CIRCLES_GRID;
        if (!patternToUse.compare("ASYMMETRIC_CIRCLES_GRID")) calibrationPattern = ASYMMETRIC_CIRCLES_GRID;
        if (calibrationPattern == NOT_EXISTING)
            {
                cerr << " Inexistent camera calibration mode: " << patternToUse << endl;
                goodInput = false;
            }
        atImageList = 0;

    }
    Mat nextImage()
    {
        Mat result;
        if( inputCapture.isOpened() )
        {
            Mat view0;
            inputCapture >> view0;
            view0.copyTo(result);
        }
        else if( atImageList < (int)imageList.size() )
            result = imread(imageList[atImageList++], CV_LOAD_IMAGE_COLOR);

        return result;
    }

    static bool readStringList( const string& filename, vector<string>& l )
    {
        l.clear();
        FileStorage fs(filename, FileStorage::READ);
        if( !fs.isOpened() )
            return false;
        FileNode n = fs.getFirstTopLevelNode();
        if( n.type() != FileNode::SEQ )
            return false;
        FileNodeIterator it = n.begin(), it_end = n.end();
        for( ; it != it_end; ++it )
            l.push_back((string)*it);
        return true;
    }
public:
    Size boardSize;            // The size of the board -> Number of items by width and height
    Pattern calibrationPattern;// One of the Chessboard, circles, or asymmetric circle pattern
    float squareSize;          // The size of a square in your defined unit (point, millimeter,etc).
    int nrFrames;              // The number of frames to use from the input for calibration
    float aspectRatio;         // The aspect ratio
    int delay;                 // In case of a video input
    bool bwritePoints;         //  Write detected feature points
    bool bwriteExtrinsics;     // Write extrinsic parameters
    bool calibZeroTangentDist; // Assume zero tangential distortion
    bool calibFixPrincipalPoint;// Fix the principal point at the center
    bool flipVertical;          // Flip the captured images around the horizontal axis
    string outputFileName;      // The name of the file where to write
    bool showUndistorsed;       // Show undistorted images after calibration
    string input;               // The input ->



    int cameraID;
    vector<string> imageList;
    int atImageList;
    VideoCapture inputCapture;
    InputType inputType;
    bool goodInput;
    int flag;

private:
    string patternToUse;


};

static void read(const FileNode& node, Settings& x, const Settings& default_value = Settings())
{
    if(node.empty())
        x = default_value;
    else
        x.read(node);
}

enum { DETECTION = 0, CAPTURING = 1, CALIBRATED = 2 };

bool runCalibrationAndSave(Settings& s, Size imageSize, Mat&  cameraMatrix, Mat& distCoeffs,
                           vector<vector<Point2f> > imagePoints );

static void readCameraParams();

void split_left_right_frame_stereo_frame(cv::Mat stereo_frame, cv::Mat &left, cv::Mat &right, int w, int h){
	left = stereo_frame(Rect(0, 0, w, h));
	right = stereo_frame(Rect(w, 0, w, h));
}

void copy_left_right_into_view(cv::Mat left, cv::Mat right, cv::Mat &view){
	for (int i=0;i<view.cols;i++) {
		if (i < left.cols) {
				view.col(i) = left.col(i);
		} else {
				view.col(i) = right.col(i - left.cols);
		}
	}
}

int main(int argc, char* argv[])
{
	StereoCalib("list.txt", 13, 9, 1);

	int width  = 640;
	int height = 400;

	readCameraParams();
	namedWindow("Image View",1);
    help();
    Settings l_s, r_s;
    //const string inputSettingsFile = argc > 1 ? argv[1] : "xml/camera_calibration.xml";
    FileStorage l_fs("xml/left_calibrate.xml", FileStorage::READ); // Read the settings
	FileStorage r_fs("xml/right_calibrate.xml", FileStorage::READ); // Read the settings
    if (!l_fs.isOpened())
    {
        cout << "Could not open the configuration file: \" left_calibrate.xml can't be found! \"" << endl;
        return -1;
    }
	if (!r_fs.isOpened())
    {
        cout << "Could not open the configuration file: \" right_calibrate.xml can't be found! \"" << endl;
        return -1;
    }
    l_fs["Settings"] >> l_s;
	r_fs["Settings"] >> r_s;
    l_fs.release();                                         // close Settings file
	r_fs.release();

    if (!l_s.goodInput)
    {
        cout << "Invalid input detected. Application stopping. " << endl;
        return -1;
    }

	vector<vector<Point2f> > l_imagePoints;
	vector<vector<Point2f> > r_imagePoints;

    Mat cameraMatrix, distCoeffs;
	Mat l_cameraMatrix, l_distCoeffs;
	Mat r_cameraMatrix, r_distCoeffs;

    Size imageSize;
    int mode = l_s.inputType == Settings::IMAGE_LIST ? CAPTURING : DETECTION;
    clock_t prevTimestamp = 0;
    const Scalar RED(0,0,255), GREEN(0,255,0);
    const char ESC_KEY = 27;

	cv::Mat left, right;

    for(int i = 0;;++i)
    {
		Mat view;
		bool blinkOutput = false;

		view = l_s.nextImage();

		//-----  If no more image, or got enough, then stop calibration and show result -------------
		/*if( mode == CAPTURING && imagePoints.size() >= (unsigned)s.nrFrames )
		{
			if( runCalibrationAndSave(s, imageSize,  cameraMatrix, distCoeffs, imagePoints))
				mode = CALIBRATED;
			else
				mode = DETECTION;
		}*/
		if( mode == CAPTURING && l_imagePoints.size() >= (unsigned)l_s.nrFrames ){
			if( runCalibrationAndSave(l_s, imageSize,  l_cameraMatrix, l_distCoeffs, l_imagePoints) && 
				runCalibrationAndSave(r_s, imageSize,  r_cameraMatrix, r_distCoeffs, r_imagePoints))
				mode = CALIBRATED;
			else
				mode = DETECTION;
		}
		if(view.empty())          // If no more images then run calibration, save and stop loop.
		{
			if( l_imagePoints.size() > 0 ){
				runCalibrationAndSave(l_s, imageSize,  l_cameraMatrix, l_distCoeffs, l_imagePoints);
				runCalibrationAndSave(r_s, imageSize,  r_cameraMatrix, r_distCoeffs, r_imagePoints);
			}
			break;
			//continue;
		}

        if( l_s.flipVertical )    flip( view, view, 0 );

		//split left right img from stereo frame
		split_left_right_frame_stereo_frame(view, left, right, width, height);

		imageSize = left.size();  // Format input image.

		vector<Point2f> l_pointBuf;
		vector<Point2f> r_pointBuf;

		bool l_found = findChessboardCorners( left , l_s.boardSize, l_pointBuf,
												CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FAST_CHECK | CV_CALIB_CB_NORMALIZE_IMAGE);
		bool r_found = findChessboardCorners( right, r_s.boardSize, r_pointBuf,
												CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FAST_CHECK | CV_CALIB_CB_NORMALIZE_IMAGE);

		if (l_found && r_found)                // If both found
		{
			// improve the found corners' coordinate accuracy for chessboard
			Mat l_viewGray;
			Mat r_viewGray;

			cvtColor(left , l_viewGray, COLOR_BGR2GRAY);
			cvtColor(right, r_viewGray, COLOR_BGR2GRAY);

			cornerSubPix( l_viewGray, l_pointBuf, Size(11,11),
				Size(-1,-1), TermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1 ));
			cornerSubPix( r_viewGray, r_pointBuf, Size(11,11),
				Size(-1,-1), TermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1 ));

			// For camera only take new samples after delay time
			l_imagePoints.push_back(l_pointBuf);
			r_imagePoints.push_back(r_pointBuf);
			prevTimestamp = clock();

			blinkOutput = l_s.inputCapture.isOpened();

			//draw
			vector<Point2f> shift_r_pointBuf;
			for ( Point2f &i : r_pointBuf ) {
				shift_r_pointBuf.push_back(Point2f(i.x+width, i.y));
			}
			drawChessboardCorners( view, l_s.boardSize, Mat(l_pointBuf), true );
			drawChessboardCorners( view, r_s.boardSize, Mat(shift_r_pointBuf), true );
		}

        //----------------------------- Output Text ------------------------------------------------
        string msg = (mode == CAPTURING) ? "100/100" :
                      mode == CALIBRATED ? "Calibrated" : "Press 'g' to start";
        int baseLine = 0;
        Size textSize = getTextSize(msg, 1, 1, 1, &baseLine);
        Point textOrigin(view.cols - 2*textSize.width - 10, view.rows - 2*baseLine - 10);

        if( mode == CAPTURING )
        {
            if(l_s.showUndistorsed)
                msg = format( "%d/%d Undist", (int)l_imagePoints.size(), l_s.nrFrames );
            else
                msg = format( "%d/%d", (int)l_imagePoints.size(), l_s.nrFrames );
        }

        putText( view, msg, textOrigin, 1, 1, mode == CALIBRATED ?  GREEN : RED);

        if( blinkOutput )
            bitwise_not(view, view);

        //------------------------- Video capture  output  undistorted ------------------------------
        if( mode == CALIBRATED && l_s.showUndistorsed )
        {
            Mat tmp_l = left.clone();
			Mat tmp_r = right.clone();
            
			undistort(tmp_l , left, l_cameraMatrix, l_distCoeffs);
			undistort(tmp_r, right, r_cameraMatrix, r_distCoeffs);

			copy_left_right_into_view(left, right, view);
			/*for(int x=0 ; x<width*2*3 ; x++)for(int y=0 ; y<height ; y++){
				view.at<uchar>(y, x) = 0;
			}*/
        }

        //------------------------------ Show image and check for input commands -------------------
        imshow("Image View", view);
        char key = (char)waitKey(l_s.inputCapture.isOpened() ? 50 : l_s.delay);

        if( key  == ESC_KEY )
            break;

        if( key == 'u' && mode == CALIBRATED ){
           l_s.showUndistorsed = !l_s.showUndistorsed;
		   r_s.showUndistorsed = !r_s.showUndistorsed;
		}
        if( l_s.inputCapture.isOpened() && key == 'g' )
        {
            mode = CAPTURING;
            l_imagePoints.clear();
			r_imagePoints.clear();
        }
    }

	printf("Jump out of capturing loop already!\n");

    // -----------------------Show the undistorted image for the image list ------------------------
    /*if( l_s.inputType == Settings::IMAGE_LIST && l_s.showUndistorsed )
    {
        Mat view, rview, rleft, rright, l_map1, l_map2, r_map1, r_map2;
        initUndistortRectifyMap(l_cameraMatrix, l_distCoeffs, Mat(),
            getOptimalNewCameraMatrix(l_cameraMatrix, l_distCoeffs, imageSize, 1, imageSize, 0),
            imageSize, CV_16SC2, l_map1, l_map2);
		initUndistortRectifyMap(r_cameraMatrix, r_distCoeffs, Mat(),
            getOptimalNewCameraMatrix(r_cameraMatrix, r_distCoeffs, imageSize, 1, imageSize, 0),
            imageSize, CV_16SC2, r_map1, r_map2);

        for(int i = 0; i < (int)l_s.imageList.size(); i++ )
        {
			view = imread(l_s.imageList[i], CV_LOAD_IMAGE_COLOR);
			system("PAUSE");
            if(view.empty())
                continue;
            
			split_left_right_frame_stereo_frame(view, left, right, width, height);
			
			remap(left, rleft, l_map1, l_map2, INTER_LINEAR);
			remap(right, rright, r_map1, r_map2, INTER_LINEAR);

			copy_left_right_into_view(left, right, rview);

            imshow("Image View", rview);
            char c = (char)waitKey(0);
            if( c  == ESC_KEY || c == 'q' || c == 'Q' )
                break;
        }
    }*/

    return 0;
}

static double computeReprojectionErrors( const vector<vector<Point3f> >& objectPoints,
                                         const vector<vector<Point2f> >& imagePoints,
                                         const vector<Mat>& rvecs, const vector<Mat>& tvecs,
                                         const Mat& cameraMatrix , const Mat& distCoeffs,
                                         vector<float>& perViewErrors)
{
    vector<Point2f> imagePoints2;
    int i, totalPoints = 0;
    double totalErr = 0, err;
    perViewErrors.resize(objectPoints.size());

    for( i = 0; i < (int)objectPoints.size(); ++i )
    {
        projectPoints( Mat(objectPoints[i]), rvecs[i], tvecs[i], cameraMatrix,
                       distCoeffs, imagePoints2);
        err = norm(Mat(imagePoints[i]), Mat(imagePoints2), CV_L2);

        int n = (int)objectPoints[i].size();
        perViewErrors[i] = (float) std::sqrt(err*err/n);
        totalErr        += err*err;
        totalPoints     += n;
    }

    return std::sqrt(totalErr/totalPoints);
}

static void calcBoardCornerPositions(Size boardSize, float squareSize, vector<Point3f>& corners,
                                     Settings::Pattern patternType /*= Settings::CHESSBOARD*/)
{
    corners.clear();

    switch(patternType)
    {
    case Settings::CHESSBOARD:
    case Settings::CIRCLES_GRID:
        for( int i = 0; i < boardSize.height; ++i )
            for( int j = 0; j < boardSize.width; ++j )
                corners.push_back(Point3f(float( j*squareSize ), float( i*squareSize ), 0));
        break;

    case Settings::ASYMMETRIC_CIRCLES_GRID:
        for( int i = 0; i < boardSize.height; i++ )
            for( int j = 0; j < boardSize.width; j++ )
                corners.push_back(Point3f(float((2*j + i % 2)*squareSize), float(i*squareSize), 0));
        break;
    default:
        break;
    }
}

static bool runCalibration( Settings& s, Size& imageSize, Mat& cameraMatrix, Mat& distCoeffs,
                            vector<vector<Point2f> > imagePoints, vector<Mat>& rvecs, vector<Mat>& tvecs,
                            vector<float>& reprojErrs,  double& totalAvgErr)
{

    cameraMatrix = Mat::eye(3, 3, CV_64F);
    if( s.flag & CV_CALIB_FIX_ASPECT_RATIO )
        cameraMatrix.at<double>(0,0) = 1.0;

    distCoeffs = Mat::zeros(8, 1, CV_64F);

    vector<vector<Point3f> > objectPoints(1);
    calcBoardCornerPositions(s.boardSize, s.squareSize, objectPoints[0], s.calibrationPattern);

    objectPoints.resize(imagePoints.size(),objectPoints[0]);

    //Find intrinsic and extrinsic camera parameters
    double rms = calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix,
                                 distCoeffs, rvecs, tvecs, s.flag|CV_CALIB_FIX_K4|CV_CALIB_FIX_K5);

    cout << "Re-projection error reported by calibrateCamera: "<< rms << endl;

    bool ok = checkRange(cameraMatrix) && checkRange(distCoeffs);

    totalAvgErr = computeReprojectionErrors(objectPoints, imagePoints,
                                             rvecs, tvecs, cameraMatrix, distCoeffs, reprojErrs);

    return ok;
}

static void  readCameraParams(){
	FileStorage fs2("VAIO_CAMERA.yml", FileStorage::READ);

	// first method: use (type) operator on FileNode.
	int frameCount = (int)fs2["nrOfFrames"];

	std::string date;
	// second method: use FileNode::operator >>
	fs2["calibration_Time"] >> date;

	Mat cameraMatrix2, distCoeffs2;
	fs2["Camera_Matrix"] >> cameraMatrix2;
	fs2["Distortion_Coefficients"] >> distCoeffs2;

	cout << "nrOfFrames: " << frameCount << endl
		 << "calibration_Time: " << date << endl
		 << "Camera_Matrix: " << cameraMatrix2 << endl
		 << "Distortion_Coefficients: " << distCoeffs2 << endl;

	FileNode features = fs2["features"];
	FileNodeIterator it = features.begin(), it_end = features.end();
	int idx = 0;
	std::vector<uchar> lbpval;

	// iterate through a sequence using FileNodeIterator
	for( ; it != it_end; ++it, idx++ )
	{
		cout << "feature #" << idx << ": ";
		cout << "x=" << (int)(*it)["x"] << ", y=" << (int)(*it)["y"] << ", lbp: (";
		// you can also easily read numerical arrays using FileNode >> std::vector operator.
		(*it)["lbp"] >> lbpval;
		for( int i = 0; i < (int)lbpval.size(); i++ )
			cout << " " << (int)lbpval[i];
		cout << ")" << endl;
	}
	fs2.release();
}

// Print camera parameters to the output file
static void saveCameraParams( Settings& s, Size& imageSize, Mat& cameraMatrix, Mat& distCoeffs,
                              const vector<Mat>& rvecs, const vector<Mat>& tvecs,
                              const vector<float>& reprojErrs, const vector<vector<Point2f> >& imagePoints,
                              double totalAvgErr )
{
    FileStorage fs( s.outputFileName, FileStorage::WRITE );

    time_t tm;
    time( &tm );
    struct tm *t2 = localtime( &tm );
    char buf[1024];
    strftime( buf, sizeof(buf)-1, "%c", t2 );

    fs << "calibration_Time" << buf;

    if( !rvecs.empty() || !reprojErrs.empty() )
        fs << "nrOfFrames" << (int)std::max(rvecs.size(), reprojErrs.size());
    fs << "image_Width" << imageSize.width;
    fs << "image_Height" << imageSize.height;
    fs << "board_Width" << s.boardSize.width;
    fs << "board_Height" << s.boardSize.height;
    fs << "square_Size" << s.squareSize;

    if( s.flag & CV_CALIB_FIX_ASPECT_RATIO )
        fs << "FixAspectRatio" << s.aspectRatio;

    if( s.flag )
    {
        sprintf( buf, "flags: %s%s%s%s",
            s.flag & CV_CALIB_USE_INTRINSIC_GUESS ? " +use_intrinsic_guess" : "",
            s.flag & CV_CALIB_FIX_ASPECT_RATIO ? " +fix_aspectRatio" : "",
            s.flag & CV_CALIB_FIX_PRINCIPAL_POINT ? " +fix_principal_point" : "",
            s.flag & CV_CALIB_ZERO_TANGENT_DIST ? " +zero_tangent_dist" : "" );
        cvWriteComment( *fs, buf, 0 );

    }

    fs << "flagValue" << s.flag;

    fs << "Camera_Matrix" << cameraMatrix;
    fs << "Distortion_Coefficients" << distCoeffs;

    fs << "Avg_Reprojection_Error" << totalAvgErr;
    if( !reprojErrs.empty() )
        fs << "Per_View_Reprojection_Errors" << Mat(reprojErrs);

    if( !rvecs.empty() && !tvecs.empty() )
    {
        CV_Assert(rvecs[0].type() == tvecs[0].type());
        Mat bigmat((int)rvecs.size(), 6, rvecs[0].type());
        for( int i = 0; i < (int)rvecs.size(); i++ )
        {
            Mat r = bigmat(Range(i, i+1), Range(0,3));
            Mat t = bigmat(Range(i, i+1), Range(3,6));

            CV_Assert(rvecs[i].rows == 3 && rvecs[i].cols == 1);
            CV_Assert(tvecs[i].rows == 3 && tvecs[i].cols == 1);
            //*.t() is MatExpr (not Mat) so we can use assignment operator
            r = rvecs[i].t();
            t = tvecs[i].t();
        }
        cvWriteComment( *fs, "a set of 6-tuples (rotation vector + translation vector) for each view", 0 );
        fs << "Extrinsic_Parameters" << bigmat;
    }

    if( !imagePoints.empty() )
    {
        Mat imagePtMat((int)imagePoints.size(), (int)imagePoints[0].size(), CV_32FC2);
        for( int i = 0; i < (int)imagePoints.size(); i++ )
        {
            Mat r = imagePtMat.row(i).reshape(2, imagePtMat.cols);
            Mat imgpti(imagePoints[i]);
            imgpti.copyTo(r);
        }
        fs << "Image_points" << imagePtMat;
    }
}

bool runCalibrationAndSave(Settings& s, Size imageSize, Mat&  cameraMatrix, Mat& distCoeffs,vector<vector<Point2f> > imagePoints )
{
    vector<Mat> rvecs, tvecs;
    vector<float> reprojErrs;
    double totalAvgErr = 0;

    bool ok = runCalibration(s,imageSize, cameraMatrix, distCoeffs, imagePoints, rvecs, tvecs,
                             reprojErrs, totalAvgErr);
    cout << (ok ? "Calibration succeeded" : "Calibration failed")
        << ". avg re projection error = "  << totalAvgErr ;

    if( ok )
        saveCameraParams( s, imageSize, cameraMatrix, distCoeffs, rvecs ,tvecs, reprojErrs,
                            imagePoints, totalAvgErr);
    return ok;
}
