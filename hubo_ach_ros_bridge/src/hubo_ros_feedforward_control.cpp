/*
ROS code:
---------
Copyright (c) 2012, Calder Phillips-Grafflin, WPI DRC Team
(2-clause BSD)

ACH code:
---------
Copyright (c) 2012, Daniel M. Lofaro
(3-clause BSD)
*/

// Basic includes
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sstream>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

// ROS includes
#include "ros/ros.h"
#include "std_msgs/String.h"

// Hubo kinematic command includes
#include "hubo_robot_msgs/JointCommand.h"
#include "trajectory_msgs/JointTrajectoryPoint.h"

// HUBO-ACH includes
#include "ach.h"
#include "hubo_components/hubo.h"

#define FT_LW 1
#define FT_RW 2
#define FT_LA 0
#define FT_RA 3
#define LEFT_IMU 0
#define RIGHT_IMU 1
#define BODY_IMU 2

// Defines
#define FT_SENSOR_COUNT 4
#define IMU_SENSOR_COUNT 3

// Global variables
ach_channel_t chan_hubo_ref_filter;

// Joint name and mapping storage
std::vector<std::string> g_joint_names;
std::map<std::string,int> g_joint_mapping;

// Debug mode switch
int hubo_debug = 0;

// From the name of the joint, find the corresponding joint index for the Hubo-ACH struct
int IndexLookup(std::string joint_name)
{
    for (unsigned int i = 0; i < g_joint_names.size(); i++)
    {
        if(g_joint_names[i].compare(joint_name) == 0)
        {
            return g_joint_mapping[g_joint_names[i]];
        }
    }
    return -1;
}

// Callback to convert the ROS joint commands into Hubo-ACH commands
void hubo_cb(const hubo_robot_msgs::JointCommand& msg)
{
    //Make the necessary hubo struct for ACH
    struct hubo_ref H_ref_filter;
    memset( &H_ref_filter, 0, sizeof(H_ref_filter));
    size_t fs;
    //First, get the current state of the Hubo from ACH
    int r = ach_get(&chan_hubo_ref_filter, &H_ref_filter, sizeof(H_ref_filter), &fs, NULL, ACH_O_LAST);
    if(ACH_OK != r)
    {
        ROS_ERROR("Something went wrong in the callback - r = %i\n", r);
    }
    else
    {
        //ROS_INFO("fs : %d, sizeof(H_ref_filter) : %d\n",fs,sizeof(H_ref_filter));
        assert(sizeof(H_ref_filter) == fs);
    }
    //Add the joint values one at a time into the hubo struct
    //for each joint command, we lookup the best matching
    //joint in the header to set the index
    if (msg.command.positions.size() != msg.joint_names.size())
    {
        ROS_ERROR("Hubo JointCommand malformed!");
    }
    for (unsigned int i = 0; i < msg.command.positions.size(); i++)
    {
        int index = IndexLookup(msg.joint_names[i]);
        if (index != -1)
        {
            H_ref_filter.ref[index] = msg.command.positions[i];
        }
    }
    //Put the new message into the ACH channel
    ach_put(&chan_hubo_ref_filter, &H_ref_filter, sizeof(H_ref_filter));
}

//NEW MAIN LOOP
int main(int argc, char **argv)
{
    //initialize ROS node
    ros::init(argc, argv, "hubo_ros_feedforward");
    ros::NodeHandle nh;
    ros::NodeHandle nhp("~");
    ROS_INFO("Initializing ROS-to-ACH bridge [feedforward]");
    // Load joint names and mappings
    XmlRpc::XmlRpcValue joint_names;
    if (!nhp.getParam("joints", joint_names))
    {
        ROS_FATAL("No joints given. (namespace: %s)", nhp.getNamespace().c_str());
        exit(1);
    }
    if (joint_names.getType() != XmlRpc::XmlRpcValue::TypeArray)
    {
        ROS_FATAL("Malformed joint specification.  (namespace: %s)", nhp.getNamespace().c_str());
        exit(1);
    }
    for (unsigned int i = 0; i < joint_names.size(); ++i)
    {
        XmlRpc::XmlRpcValue &name_value = joint_names[i];
        if (name_value.getType() != XmlRpc::XmlRpcValue::TypeString)
        {
            ROS_FATAL("Array of joint names should contain all strings.  (namespace: %s)", nhp.getNamespace().c_str());
            exit(1);
        }
        g_joint_names.push_back((std::string)name_value);
    }
    // Gets the hubo ach index for each joint
    for (size_t i = 0; i < g_joint_names.size(); ++i)
    {
        std::string ns = std::string("mapping/") + g_joint_names[i];
        int h;
        nhp.param(ns + "/huboachid", h, -1);
        g_joint_mapping[g_joint_names[i]] = h;
    }
    //initialize ACH channel
    int r = ach_open(&chan_hubo_ref_filter, HUBO_CHAN_REF_NAME , NULL);
    assert(ACH_OK == r);
    ROS_INFO("Hubo-ACH channel loaded");
    //construct ROS Subscriber
    ros::Subscriber hubo_command_sub = nh.subscribe(nh.getNamespace() + "/hubo_command", 1, hubo_cb);
    ROS_INFO("HuboCommand subscriber up");
    //spin
    ros::spin();
    //Satisfy the compiler
    return 0;
}
