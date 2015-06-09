#include "ros/ros.h"
#include "geometry_msgs/Point.h"
#include "time.h"
#include <std_msgs/Header.h>
#include <std_msgs/Float32.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <opencv2/opencv.hpp>
#include "geometry_msgs/PointStamped.h"
#include <optical_wyz/optical_wyz_msg.h>
#include <px_comm/OpticalFlow.h>
using namespace std;
using namespace cv;
double Height;
Mat frame;
string frame_encoding;
list<float> filter_vx(5,0),filter_vy(5,0);
void opt_flow_callback(const px_comm::OpticalFlow::ConstPtr& msg )
{
	Height = msg -> ground_distance ;
}
/*
void SubHeightCallback(const std_msgs::Float32ConstPtr& height_msg)
{
        Height = height_msg->data;
}
*/
void image_sub_callback(const sensor_msgs::ImageConstPtr& msg)
{
	try
	{
		frame_encoding =  msg->encoding.c_str();
		frame = cv_bridge::toCvShare(msg, frame_encoding)->image.clone();
	}
	catch (cv_bridge::Exception& e)
	{
		ROS_ERROR("Could not convert from '%s' to 'bgr8'.", msg->encoding.c_str());
	}
}
int main(int argc, char **argv)
{
        Height = 0;
        ros::init(argc,argv,"optical_node");
        ros::NodeHandle nh;
//ros publisher
        ros::Publisher optical_pub = nh.advertise<optical_wyz::optical_wyz_msg>("optical_wyz",100);
        ros::Publisher pointstamped_pub = nh.advertise<geometry_msgs::PointStamped>("myPointStamped",100);
//ros image_transport publisher
        image_transport::ImageTransport it(nh);
        image_transport::Publisher output_pub = it.advertise("output_image",1);
        sensor_msgs::ImagePtr output_msg;
//ros image_transport subscriber
        image_transport::Subscriber image_sub = it.subscribe("usb_camera/image_rect", 10, image_sub_callback);
//ros subscriber
        //ros::Subscriber height_sub = nh.subscribe("height",10,SubHeightCallback);
	ros::Subscriber px4flow_sub = nh.subscribe("/px4flow/opt_flow", 100, opt_flow_callback);
//ros msgs
        geometry_msgs::Point physical_coordinate,physical_velocity;
        geometry_msgs::PointStamped pointstamped;
	optical_wyz::optical_wyz_msg optical_msg;
//variables
        int fx = 300,fy = 300;
        int m,n;
        double err_qLevel;
	int reset = 0;
        double physical_x,physical_y;
        double pre_phy_x,pre_phy_y;
        double phy_dx,phy_dy;
        double t,pre_t,dt_sec;
        Mat output;
        Mat frame_pre;
        vector<Point2f> points[2];
        vector<Point2f> features;
        vector<uchar> status;
	vector<float> err;
        vector<geometry_msgs::PointStamped> pointstamped_vector;
        double dpx,dpy;
        string winname = "optical params";
	namedWindow(winname,CV_WINDOW_AUTOSIZE);
        ros::Rate loop_rate(20);

        if(!nh.getParam("m",m)){m = 5;}
        if(!nh.getParam("n",n)){n = 5;}
        if(!nh.getParam("err_qLevel",err_qLevel)){err_qLevel = 10;}
//initialization
        physical_x=0.0;
        physical_y=0.0;
        pre_phy_x=physical_x;
        pre_phy_y=physical_y;
/*
	string filename = "/home/wyz/catkin_ws/src/usb_camera/camera_board.yml";
	FileStorage fs_cam( filename, FileStorage::READ );
	fs_cam ["camera_matrix"] >> cameraMatrix;
*/
        //ofstream file1("/home/ycc/iarc/src/optical_wyz/coordinate_x_y.txt");
        //ofstream file2("/home/ycc/iarc/src/optical_wyz/velocity_x_y.txt");
        //if((!file1) || (!file2)) {cout<<"file open failed!";return -1; }
	createTrackbar( " reset:", winname, &reset, 1, NULL );
	createTrackbar( " fx:", winname, &fx, 1000, NULL );
	createTrackbar( " fy:", winname, &fy, 1000, NULL );

        while(ros::ok())
        {
                ROS_INFO("waiting for taking off...");
                if(Height>0.8){ROS_INFO("starting optical flow..."); break;}
                loop_rate.sleep();
                ros::spinOnce();
        }
        features.clear();
        Point2f feature_point;
        for(int i = 1;i <= m;i++)
        {
                for(int j = 1;j <= n;j++)
                {
                        feature_point.x = (int)(frame.cols/(m+1)*i);
                        feature_point.y = (int)(frame.rows/(n+1)*j);
                        features.push_back(feature_point);
                }
        }
	while(frame.empty()){ROS_ERROR("frame is empty...");ros::spinOnce();loop_rate.sleep();}

        pointstamped_vector.clear();
        pre_t = (double)getTickCount();
        while(ros::ok())
        {
                frame.copyTo(output);
                if(frame_pre.empty()){frame.copyTo(frame_pre);}
                points[0].clear();
                points[1].clear();
                points[0].insert(points[0].end(),features.begin(),features.end());
                if(points[0].size()>0) calcOpticalFlowPyrLK(frame_pre,frame,points[0],points[1],status, err);
		else points[1].clear();
                int k = 0;
                for(size_t i = 0;i < points[1].size();i++)
                {
                        if((status[i]>0?(err[i]<err_qLevel?1:0):0) && ((abs(points[0][i].x - points[1][i].x) + abs(points[0][i].y - points[1][i].y)) > 2)) {points[0][k] = points[0][i];points[1][k++] = points[1][i];}
                }
                points[0].resize(k);
                points[1].resize(k);
                for(size_t i = 0;i < points[1].size();i++)
                {
                        circle(output,points[0][i],2,Scalar(255,255,255),-1);
                        line(output,points[0][i], points[1][i], Scalar(255,255,255));
                        circle(output, points[1][i],2,Scalar(255,255,255),-1);
                }

                if(points[1].size()>1)
                {
                        vector<int> dx_vec,dy_vec;
                        for (int i = 0;i != points[1].size();i++)
                        {
                                dx_vec.push_back(points[0][i].x-points[1][i].x);
                                dy_vec.push_back(points[1][i].y-points[0][i].y);
                        }
                        int min_dis=60000;
                        int index;
                        for (int i = 0;i != dx_vec.size();i++)
                        {
                                int sum=0;
                                for (int j = 0;j != dx_vec.size();j++)
                                        if(i != j)
                                                sum += abs(dx_vec[i] - dx_vec[j])+abs(dy_vec[i] - dy_vec[j]);
                                if (sum < min_dis)
                                {
                                        min_dis = sum;
                                        index = i;
                                }
 }
                        dpx = dx_vec[index];
                        dpy = dy_vec[index];
                }
                else
                {
                        dpx = dpy = 0;
                }
                frame.copyTo(frame_pre);
//ros publish output image
                output_msg=cv_bridge::CvImage(std_msgs::Header(),frame_encoding,output).toImageMsg();
                output_pub.publish(output_msg);
//calc physical velocity && coordinate
                phy_dx = dpx*Height/fx;
                phy_dy = dpy*Height/fy;
//filter velocity
                float sum_vx=0,sum_vy=0;
                filter_vx.push_front(phy_dx);
                filter_vy.push_front(phy_dy);
                filter_vx.pop_back();
                filter_vy.pop_back();
                for(list<float>::iterator vx_iter=filter_vx.begin();vx_iter!=filter_vx.end();vx_iter++)
                        sum_vx+=*vx_iter;
                phy_dx = sum_vx/filter_vx.size();
                for(list<float>::iterator vy_iter=filter_vy.begin();vy_iter!=filter_vy.end();vy_iter++)
                        sum_vy+=*vy_iter;
                phy_dy = sum_vy/filter_vy.size();
                *filter_vx.begin() = phy_dx;
                *filter_vy.begin() = phy_dy;

                if(Height > 0.4)
                {
                        physical_x += phy_dx;
                        physical_y += phy_dy;
                }
//reset physical coordinate
		if(reset){physical_x = physical_y = 0.0;}

		physical_velocity.x = phy_dx;
		physical_velocity.y = phy_dy;
		physical_velocity.z = 0.0;
                physical_coordinate.x=physical_x;
                physical_coordinate.y=physical_y;
                physical_coordinate.z=Height;

		optical_msg.header.frame_id = "optical_wyz";
		optical_msg.header.stamp = ros::Time::now();
		optical_msg.velocity = physical_velocity;
		optical_msg.coordinate = physical_coordinate;
//publish wyz_optical
		optical_pub.publish(optical_msg);

		//coordinate_pub.publish(physical_coordinate);
                //file1<<physical_x<<" "<<physical_y<<" "<<Height<<endl;
                pointstamped.header.frame_id = "/my_frame";
                pointstamped.header.stamp = ros::Time::now();
                pointstamped.point = physical_coordinate;
                pointstamped_pub.publish(pointstamped);

		t = (double)getTickCount();
                dt_sec = (t-pre_t) / getTickFrequency();
                pre_t = t;
                cout<<"Time= "<<dt_sec<<"  FramePerSec= "<<1/dt_sec<<endl;;
                if (waitKey(1)==27)
                {
                        //file1.close();
                        //file2.close();
                        break;
                }
                ros::spinOnce();
        }
}

