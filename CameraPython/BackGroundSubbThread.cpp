/* Background subtraction code with threading in order to avoid dropped 
 * frames.
 * 
 * Author: Damon Hutley
 * Date: 15th February 2017
 * 
 */
 
//opencv
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/video.hpp>
//C
#include <stdio.h>
//C++
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <chrono>
#include <ctime>
#include <queue>
#include <deque>
//#include "Producer.h"
#include <boost/thread.hpp>


#define THRESHOLD 5.0
#define INIT_DISCARD 100

using namespace cv;
using namespace std;
// Global variables
Mat frame; //current frame
Mat fgMaskMOG2; //fg mask fg mask generated by MOG2 method
Mat backFrame; //fg mask converted to colour for saving to video
Ptr<BackgroundSubtractor> pMOG2; //MOG2 Background subtractor

//queue<Producer> Producers;
queue<Mat> Producers;
queue<int> CloseStatus;
boost::mutex sharedMutex;
boost::mutex closedMutex;

int keyboard; //input from keyboard
void help();
void threadStore(char* fname);
void processVideo(char* videoFilename);
void processImages(char* firstFrameFilename);


void help()
{
    cout
    << "--------------------------------------------------------------------------" << endl
    << "This program shows how to use background subtraction methods provided by "  << endl
    << " OpenCV. You can process both videos (-vid) and images (-img)."             << endl
                                                                                    << endl
    << "Usage:"                                                                     << endl
    << "./bs {-vid <video filename>|-img <image filename>}"                         << endl
    << "for example: ./bs -vid video.avi"                                           << endl
    << "or: ./bs -img /data/images/1.png"                                           << endl
    << "--------------------------------------------------------------------------" << endl
    << endl;
}



int main(int argc, char* argv[])
{
    //print help information
    help();
    //check for the input parameter correctness
    if(argc != 3) {
        cerr <<"Incorret input list" << endl;
        cerr <<"exiting..." << endl;
        return EXIT_FAILURE;
    }
    //create GUI windows
    namedWindow("Frame");
    namedWindow("FG Mask MOG 2");
    //create Background Subtractor objects
    pMOG2 = createBackgroundSubtractorMOG2(); //MOG2 approach
    
    
    
    if(strcmp(argv[1], "-vid") == 0) {
        //input data coming from a video
        boost::thread producerThread(&threadStore, argv[2]);
        processVideo(argv[2]);
    }
    else {
        //error in reading input parameters
        cerr <<"Please, check the input parameters." << endl;
        cerr <<"Exiting..." << endl;
        return EXIT_FAILURE;
    }
    //destroy GUI windows
    destroyAllWindows();
    cout << "Done" << endl;
    return EXIT_SUCCESS;
}



void threadStore(char* fname) {
	VideoCapture capture(fname);
	Mat Fr1;
	
	if(!capture.isOpened()){
        //error in opening the video input
        cerr << "Unable to open video file: " << fname << endl;
        exit(EXIT_FAILURE);
    }
	
	while (1) {
		if(!capture.read(Fr1)) {
            cerr << "Unable to read next frame." << endl;
            cerr << "Exiting..." << endl;
            break;
            //exit(EXIT_FAILURE);
        }

		sharedMutex.lock();
		Producers.push(Fr1.clone());
		sharedMutex.unlock();
	}
	
	capture.release();
	
	closedMutex.lock();
	CloseStatus.push(1);
	closedMutex.unlock();
	//while (1) {continue};
}



void processVideo(char* videoFilename) {
	int codec;
	double fps;
	string fnames;
	Mat Fr2;
	
    //create the capture object
    //VideoCapture capture(videoFilename);
    
    /*codec = CV_FOURCC('M','J','P','G');
	fps = capture.get(CV_CAP_PROP_FPS);
	frame.cols = capture.get(CV_CAP_PROP_FRAME_WIDTH);
	frame.rows = capture.get(CV_CAP_PROP_FRAME_HEIGHT);*/
    
    VideoWriter frameWriter;
    VideoWriter backWriter;
    
    int whitePixels;
    int totalPixels;
    double pixPercent;
    int savedFrames = 0;
    int totalFrames = 0;
    string fname;
    string bname;
    string vname;
    string sname;
    chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();
    chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
    time_t now;
    tm *ltm;

    /*if(!capture.isOpened()){
        //error in opening the video input
        cerr << "Unable to open video file: " << videoFilename << endl;
        exit(EXIT_FAILURE);
    }*/
    
    //read input data. ESC or 'q' for quitting
    while( (char)keyboard != 'q' && (char)keyboard != 27){
		
        //read the current frame
        /*if(!capture.read(frame)) {
            cerr << "Unable to read next frame." << endl;
            cerr << "Exiting..." << endl;
            break;
            //exit(EXIT_FAILURE);
        }*/
        while (!Producers.empty()) {
			sharedMutex.lock();
			Fr2 = Producers.front();
			frame = Fr2.clone();
			Producers.pop();
			sharedMutex.unlock();
			
			//update the background model
			pMOG2->apply(frame, fgMaskMOG2);
			
			// Count the number of white pixels
			whitePixels = countNonZero(fgMaskMOG2);
			totalPixels = fgMaskMOG2.total();
			pixPercent = ((float)whitePixels / (float)totalPixels) * 100;
			
			cout << "Perc: ";
			cout << fixed << setprecision(1) << pixPercent;
			cout << " %, Save: ";
			
			//t2 = chrono::high_resolution_clock::now();
			if (pixPercent > THRESHOLD && totalFrames >= INIT_DISCARD) {
				cout << "Y, Total: ";
				fname = "../../Images/Im";
				vname = "../../Videos/Vid";
				now = time(0);
				ltm = localtime(&now);
				fname = "../../Images/Img" + to_string(1900+ltm->tm_year) + "-" + to_string(1+ltm->tm_mon) + "-" + to_string(ltm->tm_mday) + "T" + to_string(ltm->tm_hour) + "-" + to_string(ltm->tm_min) + "-" + to_string(ltm->tm_sec) + "N" + to_string(savedFrames);
				vname = "../../Videos/Vid" + to_string(1900+ltm->tm_year) + "-" + to_string(1+ltm->tm_mon) + "-" + to_string(ltm->tm_mday) + "T" + to_string(ltm->tm_hour) + "-" + to_string(ltm->tm_min) + "-" + to_string(ltm->tm_sec) + "N" + to_string(savedFrames);
				bname = fname + "BS";
				sname = vname + "BS";
				imwrite(fname + ".jpg", frame);
				imwrite(bname + ".jpg", fgMaskMOG2);
				fnames += fname + ' ';
				
				/*if (!frameWriter.isOpened()){
					frameWriter.open(vname + ".avi", codec, fps, frame.size(), 1);
					backWriter.open(sname + ".avi", codec, fps, frame.size(), 1);
				}

				cvtColor(fgMaskMOG2, backFrame, COLOR_GRAY2RGB);
				frameWriter.write(frame);
				backWriter.write(backFrame);*/
				savedFrames++;
			}
			else {
				/*if (frameWriter.isOpened()) {
					frameWriter.release();
					backWriter.release();
				}*/
				
				cout << "N, Total: ";
			}
			//t1 = chrono::high_resolution_clock::now();
			
			cout << savedFrames;

			//get the frame number and write it on the current frame
			/*stringstream ss;
			rectangle(frame, cv::Point(10, 2), cv::Point(100,20),
					  cv::Scalar(255,255,255), -1);
			ss << capture.get(CAP_PROP_POS_FRAMES);
			string frameNumberString = ss.str();
			putText(frame, frameNumberString.c_str(), cv::Point(15, 15),
					FONT_HERSHEY_SIMPLEX, 0.5 , cv::Scalar(0,0,0));*/
			
			//show the current frame and the fg masks
			imshow("Frame", frame);
			imshow("FG Mask MOG 2", fgMaskMOG2);

			totalFrames++;
			
			/*cout << ", FA: ";
			cout << frameNumberString;*/
			cout << ", FB: ";
			cout << totalFrames;
			
			t2 = t1;
			t1 = chrono::high_resolution_clock::now();
			chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t1-t2);
			cout << ", Time: ";
			cout << fixed << setprecision(6) << time_span.count() << endl;
			
			//get the input from the keyboard
			keyboard = waitKey( 1 );
		}
		
		if (!CloseStatus.empty()) {
			break;
		}
		
    }
    //cout << fnames;
    //delete capture object
    //capture.release();
    //frameWriter.release();
    //backWriter.release();
}
