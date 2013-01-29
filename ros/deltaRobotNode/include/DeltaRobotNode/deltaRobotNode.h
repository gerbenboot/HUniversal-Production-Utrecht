/**
 * @file deltaRobotNode.h
 * @brief Names for the DeltaRobot services.
 * @date Created: 2012-10-17
 *
 * @author Arjan Groenewegen
 *
 * @section LICENSE
 * License: newBSD
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

#ifndef DELTAROBOTNODE_H
#define DELTAROBOTNODE_H

#include "ros/ros.h"
#include "rexosStdSrvs/Module.h"
#include "DeltaRobotNode/Point.h"

#include <DataTypes/Point3D.h>
#include <DeltaRobot/DeltaRobot.h>
#include <Motor/StepperMotor.h>
#include <DeltaRobotNode/Services.h>
#include <DeltaRobotNode/Point.h>
#include <rosMast/StateMachine.h>

#include "deltaRobotNode/MoveToPoint.h"
#include "deltaRobotNode/MovePath.h"
#include "deltaRobotNode/MoveToRelativePoint.h"
#include "deltaRobotNode/MoveRelativePath.h"
#include "deltaRobotNode/Motion.h"
#include "deltaRobotNode/Calibrate.h"

// GCC system header to suppress libjson warnings
#pragma GCC system_header
#include <Libjson/libjson.h>
// ---------------------------------------------

namespace deltaRobotNodeNamespace{
	/**
	 * the DeltaRobotNode which is a StateMachine
	 **/
	class DeltaRobotNode : public rosMast::StateMachine{
	public:
		DeltaRobotNode(int equipletID, int moduleID);
		virtual ~DeltaRobotNode();
		
		int transitionSetup();
		int transitionShutdown();
		int transitionStart();
		int transitionStop();
			
		// Main functions to be called from the services
		bool calibrate();
		bool moveToPoint(double x, double y, double z, double maxAcceleration);
		bool moveToRelativePoint(double x, double y, double z, double maxAcceleration);

		// old, deprecated services
		bool calibrate_old(deltaRobotNode::Calibrate::Request &req, deltaRobotNode::Calibrate::Response &res);
		bool moveToPoint_old(deltaRobotNode::MoveToPoint::Request &req, deltaRobotNode::MoveToPoint::Response &res);
		bool movePath_old(deltaRobotNode::MovePath::Request &req, deltaRobotNode::MovePath::Response &res);
		bool moveToRelativePoint_old(deltaRobotNode::MoveToRelativePoint::Request &req, deltaRobotNode::MoveToRelativePoint::Response &res);
		bool moveRelativePath_old(deltaRobotNode::MoveRelativePath::Request &req, deltaRobotNode::MoveRelativePath::Response &res);

		// json services
		bool calibrate_json(rexosStdSrvs::Module::Request &req, rexosStdSrvs::Module::Response &res);
		bool moveToPoint_json(rexosStdSrvs::Module::Request &req, rexosStdSrvs::Module::Response &res);
		bool movePath_json(rexosStdSrvs::Module::Request &req, rexosStdSrvs::Module::Response &res);
		bool moveToRelativePoint_json(rexosStdSrvs::Module::Request &req, rexosStdSrvs::Module::Response &res);
		bool moveRelativePath_json(rexosStdSrvs::Module::Request &req, rexosStdSrvs::Module::Response &res);

		Point parsePoint(std::string json);
		Point *parsePointArray(std::string json, int & size);

	private:
		/**
		 * @var DeltaRobot::DeltaRobot * deltaRobot
		 * the deltaRobot
		 **/
		DeltaRobot::DeltaRobot * deltaRobot;
		/**
		 * @var Motor::StepperMotor* motors[3]
		 * the collection of motors
		 **/
		Motor::StepperMotor* motors[3];
		/**
		 *
		 * @var ModbusController::ModbusController* modbus
		 * the modbuscontroller
		 **/
		ModbusController::ModbusController* modbus;
		/**
		 * @var Motor::MotorManager* motorManager
		 * The motor manager
		 **/
		Motor::MotorManager* motorManager;
		/**
		 * @var ros::ServiceServer moveToPointService_old
		 * Service for receiving move to point commands
		 **/
		ros::ServiceServer moveToPointService_old;
		/**
		 * @var ros::ServiceServer movePathService_old
		 * Service for receiving movePath commands
		 **/
		ros::ServiceServer movePathService_old;
		/**
		 * @var ros::ServiceServer moveToRelativePointService_old
		 * Service for receiving move to relative point commands
		 **/
		ros::ServiceServer moveToRelativePointService_old;
		/**
		 * @var ros::ServiceServer moveRelativePathService_old
		 * Service for receiving move relative path commands
		 **/
		ros::ServiceServer moveRelativePathService_old;
		/**
		 * @var ros::ServiceServer calibrateService_old
		 * Service for receiving calibrate commands
		 **/
		ros::ServiceServer calibrateService_old;

		/**
		 * @var ros::ServiceServer moveToPointService_json
		 * Service for receiving move to point commands
		 **/
		ros::ServiceServer moveToPointService_json;
		/**
		 * @var ros::ServiceServer movePathService_json
		 * Service for receiving movePath commands
		 **/
		ros::ServiceServer movePathService_json;
		/**
		 * @var ros::ServiceServer moveToRelativePointService_json
		 * Service for receiving move to relative point commands
		 **/
		ros::ServiceServer moveToRelativePointService_json;
		/**
		 * @var ros::ServiceServer moveRelativePathService_json
		 * Service for receiving move relative path commands
		 **/
		ros::ServiceServer moveRelativePathService_json;
		/**
		 * @var ros::ServiceServer calibrateService_json
		 * Service for receiving calibrate commands
		 **/
		ros::ServiceServer calibrateService_json;
	};
}
#endif
