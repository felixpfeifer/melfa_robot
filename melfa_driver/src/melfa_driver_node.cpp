// Copyright 2018 Tokyo Opensource Robotics Kyokai Association (TORK)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file melfa_driver_node.cpp
 * 
 * Driver node for MELFA robot controller
 */
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <ros/ros.h>
#include <vector>
#include <controller_manager/controller_manager.h>
#include <diagnostic_updater/diagnostic_updater.h>
#include "melfa_driver/melfa_hardware_interface.h"
#include "melfa_driver/io.h"

MelfaHW* _robot;

/****************************************************/
void callback_io(const melfa_driver::io::ConstPtr& io_msg)
{
    if (io_msg->sending_inputs == 1)
    {
        _robot->write_IO_sending_Type(MXT_IO_IN);
    }
    else
    {
        _robot->write_IO_sending_Type(MXT_IO_OUT);
    }
    _robot->write_IO_sending_value(io_msg->values);
    if (io_msg->reading_inputs == 1)
    {
        _robot->write_IO_reading_Type(MXT_IO_IN);
    }
    else
    {
        _robot->write_IO_reading_Type(MXT_IO_OUT);
    }
    
    

}
/****************************************************/

int main(int argc, char **argv) {
    // init ROS node
    ros::init(argc, argv, "melfa_driver");
    ros::NodeHandle nh;

    // Parameters
    bool realtime;
    ros::param::param<bool>("~realtime", realtime, false);
    double period;
    ros::param::param<double>("~period", period, 0.0071);

    // create hardware interface
    MelfaHW robot(period);
    _robot = &robot;
    controller_manager::ControllerManager cm(&robot, nh);

    // diagnostic_updater
    diagnostic_updater::Updater updater;
    updater.setHardwareID("melfa_driver");
    updater.add("diagnose", &robot, &MelfaHW::diagnose);

    // Setup realtime scheduler
    if (realtime) {
        struct sched_param param;
        memset(&param, 0, sizeof(param));
        int policy = SCHED_FIFO;
        param.sched_priority = sched_get_priority_max(policy);

        ROS_WARN("Setting up Realtime scheduler");
        if (sched_setscheduler(0, policy, &param) < 0) {
            ROS_ERROR("sched_setscheduler: %s", strerror(errno));
            ROS_ERROR("Please check you are using PREEMPT_RT kernel and set /etc/security/limits.conf");
            exit(1);
        }
        if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
            ROS_ERROR("mlockall: %s", strerror(errno));
            exit(1);
        }
    }
    // set spin rate
    ros::Rate rate(1.0 / robot.getPeriod().toSec());
    ros::AsyncSpinner spinner(1);
    spinner.start();

    // write first setting packet
    robot.write_first();
    int counter;
    /****************************************************/
    // Create a publischer for io msg
    ros::Subscriber sub = nh.subscribe<melfa_driver::io>("/io_controller",1,callback_io);
    ros::Publisher pub = nh.advertise<melfa_driver::io>("/io_robot",1);
    /****************************************************/
    while (ros::ok()) {
        // Wait for reciving a packet
        robot.read();
        /****************************************************/
        // Read IO Message if pressent
        melfa_driver::io io_msg;
        io_msg.values = robot.read_IO_Value();
        uint8_t type = robot.read_IO_return_type();
        if (type == MXT_IO_IN)
        {
            io_msg.reading_inputs = 1;
        }else{
            io_msg.reading_inputs = 0;
        }

        pub.publish(io_msg);
        /****************************************************/

        cm.update(ros::Time::now(), robot.getPeriod());


        robot.write();

        // Update diagnostics
        updater.update();

        rate.sleep();
    }
    spinner.stop();
    return 0;
}