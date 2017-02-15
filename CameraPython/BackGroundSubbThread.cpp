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
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#define THRESHOLD 5.0
#define INIT_DISCARD 100

using namespace cv;
using namespace std;

// Global variables
Mat frame; //current frame
Mat fgMaskMOG2; //fg mask fg mask generated by MOG2 method
Mat backFrame; //fg mask converted to colour for saving to video
Ptr<BackgroundSubtractor> pMOG2; //MOG2 Background subtractor

// Queues
queue<Mat> Producers;
queue<int> CloseStatus;
boost::mutex sharedMutex;
boost::mutex closedMutex;

int keyboard; //input from keyboard

void help();
void threadStore(char* fname);
void processVideo(char* videoFilename, string imgFile);
void extractImages(string path);


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
	string defImg = "";
	
    //print help information
    help();
    
    //check for the input parameter correctness
    /*if(argc != 3) {
        cerr <<"Incorret input list" << endl;
        cerr <<"exiting..." << endl;
        return EXIT_FAILURE;
    }*/
    
    //create GUI windows
    namedWindow("Frame");
    namedWindow("FG Mask MOG 2");
    
    //create Background Subtractor objects
    pMOG2 = createBackgroundSubtractorMOG2(); //MOG2 approach
    
    if(strcmp(argv[1], "-vid") == 0) {
        // Initialise thread to read video
        boost::thread producerThread(&threadStore, argv[2]);

        // Process output of video thread
		if (argc == 5 && strcmp(argv[3], "-back") == 0) {
			processVideo(argv[2], argv[4]);
		}
		else if (argc == 3) {
			processVideo(argv[2], defImg);
		}
		else {
			cerr <<"Incorret input list" << endl;
			cerr <<"exiting..." << endl;
			return EXIT_FAILURE;
		}
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
	/*
	 * This thread reads the video file or stream, and stores each frame on a queue.
	 * This function is run concurrently to the image subtraction processing algorithm.
	 */
	
	VideoCapture capture(fname);
	Mat Fr1;
	
	if(!capture.isOpened()){
        //error in opening the video input
        cerr << "Unable to open video file: " << fname << endl;
        exit(EXIT_FAILURE);
    }
    
	while (1) {
		// Read the frame
		if(!capture.read(Fr1)) {
			// End the thread when video stops
            cerr << "Unable to read next frame." << endl;
            cerr << "Exiting..." << endl;
            break;
        }
        
		imshow("Frame", Fr1);
		
		// Load the frame onto a queue
		sharedMutex.lock();
		Producers.push(Fr1.clone());
		sharedMutex.unlock();
	}
	
	// Close video resource
	capture.release();
	
	// Let the main thread know that no more video is coming
	closedMutex.lock();
	CloseStatus.push(1);
	closedMutex.unlock();
}



void processVideo(char* videoFilename, string imgFile) {
	/*
	 * This function reads each frame from a queue, and performs image subtraction.
	 * If the difference between frames exceeds a tolerance, then the frame and
	 * foreground mask is saved to both images and videos.
	 */
	
	int codec;
	double fps;
	int i;
	Mat Fr2;
	Mat ImgBack;
	Mat ImgRGB;
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
    string path;
    string tstamp;
	
	// Objects to write frames to video
    VideoWriter frameWriter;
    VideoWriter backWriter;
	
    //create the capture object
    VideoCapture capture(videoFilename);
    
    // Determine characteristics for writing to video
    codec = CV_FOURCC('M','J','P','G');
	fps = capture.get(CV_CAP_PROP_FPS);
	frame.cols = capture.get(CV_CAP_PROP_FRAME_WIDTH);
	frame.rows = capture.get(CV_CAP_PROP_FRAME_HEIGHT);
	
	// Close the video (as it's being used by another thread)
	capture.release();
	
	if (!(imgFile == "")) {
		ImgBack = imread(imgFile, 1);
	}
    
    //read input data. ESC or 'q' for quitting
    while( (char)keyboard != 'q' && (char)keyboard != 27){
		// Only run the code when a frame is waiting to be processed
        while (!Producers.empty()) {
			// Fetch and remove the first frame in the queue
			sharedMutex.lock();
			Fr2 = Producers.front();
			frame = Fr2.clone();
			Producers.pop();
			sharedMutex.unlock();
			
			// Background subtraction algorithm for a static background image
			if (!(imgFile == "")) {
				absdiff(frame, ImgBack, ImgRGB);
				fgMaskMOG2 = Mat::zeros(ImgRGB.rows, ImgRGB.cols, CV_8UC1);
				float thresh = 30.0f;
				float dist;
				
				// Iterate through every single pixel of the frame
				for (int j=0; j<ImgRGB.rows; ++j) {
					for (int k=0; k<ImgRGB.cols; ++k) {
						cv::Vec3b pix = ImgRGB.at<Vec3b>(j,k);
						dist = (pix[0]*pix[0] + pix[1]*pix[1] + pix[2]*pix[2]);
						dist = sqrt(dist);
						
						// Pixel is white if the change between frames exceed the threshold,
						// otherwise the pixel is black
						if (dist>thresh) {
							fgMaskMOG2.at<unsigned char>(j,k) = 255;
						}
					}
				}
			}
			else {
				// Background subtraction algorithm without static background
				pMOG2->apply(frame, fgMaskMOG2);
			}
			
			// Count the number of white pixels
			whitePixels = countNonZero(fgMaskMOG2);
			totalPixels = fgMaskMOG2.total();
			pixPercent = ((float)whitePixels / (float)totalPixels) * 100;
			
			// Print the percentage of white pixels
			cout << "Perc: ";
			cout << fixed << setprecision(1) << pixPercent;
			cout << " %, Save: ";
			
			// Save the frame if the number of white pixels exceeds the threshold
			// Also skip the first few frames as they often appear green
			if (pixPercent > THRESHOLD && totalFrames >= INIT_DISCARD) {
				if (savedFrames == 0) {
					// Create a folder which will contain all the saved images and videos
					now = time(0);
					ltm = localtime(&now);
					tstamp = to_string(1900+ltm->tm_year) + "-" + to_string(1+ltm->tm_mon) + "-" + to_string(ltm->tm_mday) + "T" + to_string(ltm->tm_hour) + "-" + to_string(ltm->tm_min) + "-" + to_string(ltm->tm_sec);
					path = "../../Images/IS" + tstamp;
					boost::filesystem::path dir(path);
					boost::filesystem::create_directory(dir);
				}
				
				cout << "Y, Total: ";
				
				// Determine the filenames based on the timestamp
				now = time(0);
				ltm = localtime(&now);
				fname = path + "/Img" + to_string(1900+ltm->tm_year) + "-" + to_string(1+ltm->tm_mon) + "-" + to_string(ltm->tm_mday) + "T" + to_string(ltm->tm_hour) + "-" + to_string(ltm->tm_min) + "-" + to_string(ltm->tm_sec) + "N" + to_string(savedFrames);
				vname = path + "/Vid" + to_string(1900+ltm->tm_year) + "-" + to_string(1+ltm->tm_mon) + "-" + to_string(ltm->tm_mday) + "T" + to_string(ltm->tm_hour) + "-" + to_string(ltm->tm_min) + "-" + to_string(ltm->tm_sec) + "N" + to_string(savedFrames);
				bname = fname + "BS";
				sname = vname + "BS";
				
				// Save the frame and foreground mask as separate images.
				//imwrite(fname + ".jpg", frame);
				//imwrite(bname + ".jpg", fgMaskMOG2);
				
				// Initialise the video writers when motion is first detected
				if (!frameWriter.isOpened()){
					frameWriter.open(vname + ".avi", codec, fps, frame.size(), 1);
					backWriter.open(sname + ".avi", codec, fps, frame.size(), 1);
				}
				
				// Add the frame and foreground masks to separate videos.
				cvtColor(fgMaskMOG2, backFrame, COLOR_GRAY2RGB);
				frameWriter.write(frame);
				backWriter.write(backFrame);
				savedFrames++;
			}
			else {
				// Close the video writersas soon as motion stops being detected
				if (frameWriter.isOpened()) {
					frameWriter.release();
					backWriter.release();
				}
				
				cout << "N, Total: ";
			}
			
			cout << savedFrames;
			
			//show the current frame and the fg masks
			//imshow("Frame", frame);
			imshow("FG Mask MOG 2", fgMaskMOG2);

			totalFrames++;
			
			cout << ", Frame: ";
			cout << totalFrames;
			
			// Calculate time to process each frame
			t2 = t1;
			t1 = chrono::high_resolution_clock::now();
			chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t1-t2);
			cout << ", Time: ";
			cout << fixed << setprecision(6) << time_span.count() << endl;
			
			//get the input from the keyboard
			keyboard = waitKey( 1 );
		}
		
		// Close the program when no frames are left to process
		if (!CloseStatus.empty()) {
			if (savedFrames > 0) {
				extractImages(path);
			}
			break;
		}
		
    }
    
    // Free video writer resources
    frameWriter.release();
    backWriter.release();
}



void extractImages(string p) {
	Mat extFrame;
	int nof;
	string fname;
	
	// Iterate through every video in the specified directory
	for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(p), {})) {
		//create the capture object
		VideoCapture capture(entry.path().string());
		cout << "Extracting: " << entry << endl;
		
		if(!capture.isOpened()){
			//error in opening the video input
			cerr << "Unable to open video file: " << entry.path().string() << endl;
			exit(EXIT_FAILURE);
		}
		
		nof = 0;
		
		// Iterate through every frame of the video
		while (1) {
			// Read the frame
			if(!capture.read(extFrame)) {
				// End the loop when video stops
				break;
			}
			
			// Save the frame as an image
			fname = entry.path().string().substr(0, entry.path().string().find(".avi")) + "I" + to_string(nof);
			imwrite(fname + ".jpg", extFrame);
			nof++;
		}
		
		// Close the video (as it's being used by another thread)
		capture.release();
	}
}
