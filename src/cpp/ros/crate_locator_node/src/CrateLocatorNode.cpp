/**
 * @file CrateLocatorNode.cpp
 * @brief Detects the crates and publishes events on the crateEvent topic.
 * @date Created: 2011-11-11
 * @date Rerexos_visioned: 2012-10-22
 *
 * @author Kasper van Nieuwland
 * @author Zep Mouris
 * @author Koen Braham
 * @author Daan Veltman
 *
 * @section LICENSE
 * License: newBSD
 *
 * Copyright © 2012, HU University of Applied Sciences Utrecht.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * - Neither the name of the HU University of Applied Sciences Utrecht nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE HU UNIVERSITY OF APPLIED SCIENCES UTRECHT
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>

#include <crate_locator_node/CrateLocatorNode.h>
#include <crate_locator_node/Services.h>
#include <crate_locator_node/Topics.h>


/**
 * @var WINDOW_NAME
 * Name for the opencv image window.
 **/
static const char WINDOW_NAME[] = "Image window";

/**
 * On mouse click event. Prints the real life (Deltarobot) and pixel coordinate of the clicked pixel.
 *
 * @param event CV_EVENT (eg. CV_EVENT_LBUTTONDOWN)
 * @param x X coordinate of the click
 * @param y Y coordinate of the click
 * @param flags CV_EVENT_FLAG
 * @param param rexos_vision::PixelToRealCoordinateTransformer * param Pointer to the PixelToRealCoordinateTransformer used to conversion.
 **/
void on_mouse(int event, int x, int y, int flags, void* param){
	if(event == CV_EVENT_LBUTTONDOWN){
		rexos_vision::PixelAndRealCoordinateTransformer* cordTransformer = (rexos_vision::PixelAndRealCoordinateTransformer*) param;
		rexos_datatypes::Point2D result = cordTransformer->pixelToRealCoordinate(rexos_datatypes::Point2D(x, y));
		ROS_INFO("RX: %f, RY:%f", result.x, result.y);
		ROS_INFO("PX: %d, PY:%d", x, y);
		std::cout.flush();
	}
}

/**
 * Subscribes to the camera node, starts the QR detector and opens a window to show the output.
 **/
CrateLocatorNode::CrateLocatorNode(int moduleID) :
		rexos_statemachine::StateMachine("crate_locator_node_" + moduleID),
		measurementCount(0),
		measurements(0),
		failCount(0),
		imageTransport(node)
{
	// Setup the QR detector
	qrDetector = new rexos_vision::QRCodeDetector();

	// Setup the fiducial detector
	fidDetector = new rexos_vision::FiducialDetector();
	// Minimum for the radius of the circle = 10, so a diameter of 20 pixels
	fidDetector->setMinRad(10);
	// Maximum for the radius of the circle = 40, so a diameter of 80 pixels
	fidDetector->setMaxRad(40);
	// Minimum distance between two circles, 30 pixels
	fidDetector->setDistance(30);
	fidDetector->setMinDist(2.0f);
	fidDetector->setMaxDist(5.0f);
	fidDetector->verbose = true;
	fidDetector->setCircleVotes(60);

	// Setup the coordinate transformation(from pixel to real life)
	// The real-life coordinates of the fiducials in mm.
	// 0,0 is center of the delta robot, all coordinates are relative.
	std::vector<rexos_datatypes::Point2D> rc;
	rc.push_back(rexos_datatypes::Point2D(-75, 115));
	rc.push_back(rexos_datatypes::Point2D(25, 115));
	rc.push_back(rexos_datatypes::Point2D(25, 65));

	cordTransformer = new rexos_vision::PixelAndRealCoordinateTransformer(rc, rc);

	// Crate tracking configuration
	// The amount of mm a point has to move before we mark it as moving. When not moving we found a deviation of ~0.5 pixel.
	double crateMovementThreshold = 0.75;

	// The number of frames before a change is marked definite.
	int numberOfStableFrames = 5;
	crateTracker = new rexos_vision::CrateTracker(numberOfStableFrames, crateMovementThreshold);

	crateEventPublisher = node.advertise<crate_locator_node::CrateEventMsg>(CrateLocatorNodeTopics::CRATE_EVENT, 100);

	// Opencv GUI
	cv::namedWindow(WINDOW_NAME);
	cvSetMouseCallback(WINDOW_NAME, &on_mouse, cordTransformer);
}


/**
 * Destructor for the CrateLocator. Removes the QR detector, fiducial detector, crate tracker and coordinate transformer.
 **/
CrateLocatorNode::~CrateLocatorNode(){
	delete qrDetector;
	delete fidDetector;
	delete cordTransformer;
	delete crateTracker;
	cv::destroyWindow(WINDOW_NAME);
}

/**
 * Get crate service.
 * Looks up a crate in the known crates and returns it if found. If the crate does not exists it will return an empty crate and rexos_datatypes::Crate::state_non_existing
 *
 * @param req Request arguments for the service. Should include the name of the crate
 * @param res Reponse for the caller. Contains a crate message and crate state.
 * @return true if the service is handled.
 **/
bool CrateLocatorNode::getCrate(crate_locator_node::getCrate::Request &req, crate_locator_node::getCrate::Response &res){
	rexos_datatypes::Crate crate;
	bool succeeded = crateTracker->getCrate(req.name, crate);
	if(succeeded){
		res.state = crate.getState();
		crate_locator_node::CrateMsg msg;
		msg.name = crate.name;
		msg.x = crate.rect().center.x;
		msg.y = crate.rect().center.y;
		msg.angle = crate.rect().angle;
		res.crate = msg;
	} else{
		res.state = rexos_datatypes::Crate::state_non_existing;
		crate_locator_node::CrateMsg msg;
		msg.name = "";
		msg.x = 0;
		msg.y = 0;
		msg.angle = 0;
		res.crate = msg;
	}
	return true;
}

/**
 * Get all crates service.
 * Puts all known and located crates into the crates vector.
 *
 * @param req Request arguments for the service. Empty as unused.
 * @param res Reponse for the caller. Contains a crate message and crate state for each crate.
 * @return true if the service is handled.
 **/
bool CrateLocatorNode::getAllCrates(crate_locator_node::getAllCrates::Request &req, crate_locator_node::getAllCrates::Response &res){
	std::vector<rexos_datatypes::Crate> allCrates = crateTracker->getAllCrates();
	for(std::vector<rexos_datatypes::Crate>::iterator it = allCrates.begin(); it != allCrates.end(); ++it){
		res.states.push_back(it->getState());
		crate_locator_node::CrateMsg msg;
		msg.name = it->name;
		msg.x = it->rect().center.x;
		msg.y = it->rect().center.y;
		msg.angle = it->rect().angle;
		res.crates.push_back(msg);
	}
	return true;
}

/**
 * Determines the average X of a std::vector<cv::Point2f>
 *
 * @param points Vector of point2f points.
 * @return float Average X of the vector
 **/
inline float averageX(std::vector<cv::Point2f> points){
	int pointsCounter = 0;
	double total = 0;
	for(std::vector<cv::Point2f>::iterator it = points.begin(); it != points.end(); ++it, ++pointsCounter){
		total += (*it).x;
	}
	return total / pointsCounter;
}

/**
 * Determines the average Y of a std::vector<cv::Point2f>
 *
 * @param points Vector of point2f points.
 * @return float Average Y of the vector
 **/
inline float averageY(std::vector<cv::Point2f> points){
	int pointsCounter = 0;
	double total = 0;
	for(std::vector<cv::Point2f>::iterator it = points.begin(); it != points.end(); ++it, ++pointsCounter){
		total += (*it).y;
	}
	return total / pointsCounter;
}

/**
 * Starts the calibration procedure
 **/
bool CrateLocatorNode::calibrate(unsigned int measurements, unsigned int maxErrors){
	ROS_INFO("Updating calibration markers...");

	// Clear all buffers and settings for the new calibration
	fid1_buffer.clear();
	fid2_buffer.clear();
	fid3_buffer.clear();

	measurementCount = 0;
	this->measurements = measurements;
	failCount = 0;

	// ROS says: Once a subscriber is removed (aka out of context) the subscription is removed.
	// This allows us to use a temporary callback handler.
	{
		std::cout << "[DEBUG] Starting calibration" << std::endl;
		image_transport::Subscriber subscriber = imageTransport.subscribe("camera/image", 1, &CrateLocatorNode::calibrateCallback, this, image_transport::TransportHints("compressed"));

		while(ros::ok() && (measurementCount < measurements && failCount < maxErrors)){
			ros::spinOnce();
		}
		std::cout << "[DEBUG] Done calibrating, removing subscription." << std::endl;
	}

	// If it was a successful capture of all fiducials process the results
	if(measurementCount >= measurements){
		std::cout << "[DEBUG] Computing average X and Y coordinate for each marker." << std::endl;
		rexos_datatypes::Point2D fid1(averageX(fid1_buffer), averageY(fid1_buffer));
		rexos_datatypes::Point2D fid2(averageX(fid2_buffer), averageY(fid2_buffer));
		rexos_datatypes::Point2D fid3(averageX(fid3_buffer), averageY(fid3_buffer));

		// Put new marked locations into the cordinate transformer
		markers.push_back(rexos_datatypes::Point2D(fid1.x, fid1.y));
		markers.push_back(rexos_datatypes::Point2D(fid2.x, fid2.y));
		markers.push_back(rexos_datatypes::Point2D(fid3.x, fid3.y));
		cordTransformer->setFiducialPixelCoordinates(markers);

		ROS_INFO( "Calibration markers updated.\nMeasured: %d Failed: %d", measurements, failCount);
		return true;
	}

	ROS_INFO( "Calibration timed out, too many failed attempts. Measurements needed: %d Measured: %d", measurements, measurementCount);
	return false;
}

/**
 * Callback function for the calibration procedure.
 * Receives an image of the image topic and tries to detect all fiducials that are present.
 **/
void CrateLocatorNode::calibrateCallback(const sensor_msgs::ImageConstPtr& msg){
	// Receive image
	cv_bridge::CvImagePtr cv_ptr;
	try{
		cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
	} catch(cv_bridge::Exception& e){
		ROS_ERROR("cv_bridge exception: %s", e.what());
		return;
	}

	// First copy the image to a gray scale image.
	cv::Mat gray;
	cv::cvtColor(cv_ptr->image, gray, CV_BGR2GRAY);

	// Locate all fiducial points
	std::vector<cv::Point2f> fiducialPoints;
	fidDetector->detect(gray, fiducialPoints, &cv_ptr->image);

	// If three fiducials (all on the workin area) have been found, sort them on distance.
	// The fiducials are put into their own buffer as they are sorted.
	if(fiducialPoints.size() == 3){
		measurementCount++;
		rexos_vision::FiducialDetector::order(fiducialPoints);
		fid1_buffer.push_back(fiducialPoints[0]);
		fid2_buffer.push_back(fiducialPoints[1]);
		fid3_buffer.push_back(fiducialPoints[2]);
	} else{
		failCount++;
//size() on a vector returns a size_type, which has a different size on a 64-bit platform
#if defined(__LP64__) || defined(_LP64)
		ROS_WARN("Incorrect number of markers. Needed: 3 Saw: %lu", fiducialPoints.size());
#else
		ROS_WARN("Incorrect number of markers. Needed: 3 Saw: %u", fiducialPoints.size());
#endif
	}

	// Show the debug image and progress status.
	cv::imshow(WINDOW_NAME, cv_ptr->image);
	cv::waitKey(3);
	std::cout << "Measures " << measurementCount << "/" << measurements << std::endl;
	std::cout.flush();
}

/**
 * Callback function for the crate location.
 * Main steps in the callback are: receive a frame, detect all crates and send event(s).
 * Detects
 **/
void CrateLocatorNode::crateLocateCallback(const sensor_msgs::ImageConstPtr& msg){
	// Receive image
	cv_bridge::CvImagePtr cv_ptr;
	try{
		cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
	} catch(cv_bridge::Exception& e){
		ROS_ERROR("cv_bridge exception: %s", e.what());
		return;
	}

	// First copy the image to a gray scale image.
	cv::Mat gray;
	cv::cvtColor(cv_ptr->image, gray, CV_BGR2GRAY);

	// Draw the calibration points for visual debugging.
	for(std::vector<rexos_datatypes::Point2D>::iterator it = markers.begin(); it != markers.end(); ++it){
		cv::circle(cv_ptr->image, cv::Point(cv::saturate_cast<int>(it->x), cv::saturate_cast<int>(it->y)), 1, cv::Scalar(0, 0, 255), 2);

		cv::circle(cv_ptr->image, cv::Point(cordTransformer->realToPixelCoordinate(cordTransformer->pixelToRealCoordinate(rexos_datatypes::Point2D(cv::saturate_cast<int>(it->x), cv::saturate_cast<int>(it->y)))).x, cordTransformer->realToPixelCoordinate(cordTransformer->pixelToRealCoordinate(rexos_datatypes::Point2D(cv::saturate_cast<int>(it->x), cv::saturate_cast<int>(it->y)))).y), 7, cv::Scalar(255, 0, 255), 1);
	}

	// Detect all QR crates in the image.
	std::vector<rexos_datatypes::Crate> crates;
	qrDetector->detectQRCodes(gray, crates);

	// Transform crate coordinates
	for(std::vector<rexos_datatypes::Crate>::iterator it = crates.begin(); it != crates.end(); ++it){
		it->draw(cv_ptr->image);

		std::vector<cv::Point2f> points = it->getPoints();
		for(int n = 0; n < 3; n++){
			rexos_datatypes::Point2D coordinate(points[n].x, points[n].y);
			coordinate = cordTransformer->pixelToRealCoordinate(coordinate);
			points[n].x = coordinate.x;
			points[n].y = coordinate.y;
		}
		it->setPoints(points);
	}

	// Inform the crate tracker about the located crates
	std::vector<rexos_vision::CrateEvent> events = crateTracker->update(crates);

	// Publish events to event topic
	for(std::vector<rexos_vision::CrateEvent>::iterator it = events.begin(); it != events.end(); ++it){
		crate_locator_node::CrateEventMsg msg;
		msg.event = it->type;
		msg.crate.name = it->name;
		msg.crate.x = it->x;
		msg.crate.y = it->y;
		msg.crate.angle = it->angle;

		ROS_INFO("%s", it->toString().c_str());
		crateEventPublisher.publish(msg);
	}

	// Show the camera frame in a opencv window
	cv::imshow(WINDOW_NAME, cv_ptr->image);
	cv::waitKey(3);
}

/**
 * Blocking function that contains the main loop.
 * Spins in ROS to receive frames. These will execute the callbacks.
 * This function ends when ros receives a ^c
 **/
void CrateLocatorNode::run(){
	while(ros::ok()){
		ros::spinOnce();
	}
}

/**
 * Start the service servers of the module to make it able to perform tasks.
 */
void CrateLocatorNode::startServices(){
	// ROS services and topics
	getCrateService = node.advertiseService(CrateLocatorNodeServices::GET_CRATE, &CrateLocatorNode::getCrate, this);
	getAllCratesService = node.advertiseService(CrateLocatorNodeServices::GET_ALL_CRATES, &CrateLocatorNode::getAllCrates, this);
}

/**
 * Shutdown the service servers
 */
void CrateLocatorNode::stopServices(){
	getCrateService.shutdown();
	getAllCratesService.shutdown();
}

/**
 * Transition from Safe to Standby state
 * @return 0 if everything went OK else error
 **/
void CrateLocatorNode::transitionSetup(){
	ROS_INFO("Setup transition called");
	calibrate();
}

/**
 * Transition from Standby to Safe state
 * Will turn power off the motor
 * @return will be 0 if everything went ok else error
 **/
void CrateLocatorNode::transitionShutdown(){
	ROS_INFO("Shutdown transition called");
}

/**
 * Transition from Standby to Normal state
 * @return will be 0 if everything went ok else error
 **/
void CrateLocatorNode::transitionStart(){
	ROS_INFO("Start transition called");

	std::cout << "[DEBUG] Waiting for subscription" << std::endl;
	// Subscribe example: (poorly documented on ros wiki)
	// Images are transported in JPEG format to decrease tranfer time per image.
	// imageTransport.subscribe(<base image topic>, <queue_size>, <callback>, <tracked object>, <TransportHints(<transport type>)>)
	cameraSubscriber = imageTransport.subscribe("camera/image", 1, &CrateLocatorNode::crateLocateCallback, this, image_transport::TransportHints("compressed"));
	std::cout << "[DEBUG] Starting crateLocateCallback loop" << std::endl;

	startServices();
}

/**
 * Transition from Normal to Standby state
 * @return will be 0 if everything went ok else error
 **/
void CrateLocatorNode::transitionStop(){
	ROS_INFO("Stop transition called");
	cameraSubscriber.shutdown();
	stopServices();
}
/**
 * Main methods that starts the cratelocator node.
 **/
int main(int argc, char* argv[]){
	ros::init(argc, argv, "crateLocator");
	CrateLocatorNode crateLocatorNode(999);
	crateLocatorNode.run();

	return 0;
}
