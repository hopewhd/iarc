#include "ros/ros.h"
#include "sensor_msgs/Imu.h"
#include <thread>
#include "LpmsSensorI.h"
#include "LpmsSensorManagerI.h"
#include <imu_ultrasonic_data/imu_data_srv.h>
#define pi  3.14159
using namespace std;
sensor_msgs::Imu msg_srv;
bool imu_data_callback(imu_ultrasonic_data::imu_data_srv::Request &req , imu_ultrasonic_data::imu_data_srv::Response &resp);
int main(int argc,char **argv)
{
    ros::init(argc,argv,"imu_node");
    ros::NodeHandle n;
    ros::Publisher imu_pub = n.advertise<sensor_msgs::Imu>("imu_data",100);      
    ros::ServiceServer imu_srv = n.advertiseService("imu_data_srv", imu_data_callback);
    ros::Rate loop_rate(250);  
    sensor_msgs::Imu msg;

    ImuData d;
    LpmsSensorManagerI* manager = LpmsSensorManagerFactory();                           // Gets a LpmsSensorManager instance
    LpmsSensorI* lpms = manager->addSensor(DEVICE_LPMS_U, "A5022WCW");        //A5022WCW lpms_cu   //A4004fs5 lpms_curs 
    int flag=0;
    float roll,pitch,yaw,yawoffside=0,rolloffside=0,pitchoffside=0,yawtemp;
    while(ros::ok())
      {
         if (lpms->getConnectionStatus() == SENSOR_CONNECTION_CONNECTED && lpms->hasImuData()) 
          {		
            d = lpms->getCurrentData();                          // Reads data
            if(flag < 100)
            {
                flag ++;
                if(flag == 100)
                {
                      yawoffside = d.r[2];
                      rolloffside = d.r[0];
                      pitchoffside = d.r[1];  
                } 
            }
            roll = ((d.r[0]-rolloffside)/180.0)*pi;
            pitch = ((d.r[1]-pitchoffside)/180.0)*pi;
            yaw = ((d.r[2]-yawoffside)/180.0)*pi;

            /*if ((d.r[2]-yawoffside)>0)
            {
                    yawtemp = ((d.r[2]-yawoffside)/(180.0-yawoffside))*180.0;
                    yaw = (yawtemp/180.0)*pi;
            }
            else
            {
                  yawtemp = ((d.r[2]-yawoffside)/(-180.0-yawoffside))*(-180.0);
                  yaw = (yawtemp/180.0)*pi; 
            }*/

            msg.orientation.x = -1.0*roll;                              // Euler angle data.                    float r[3];  
            msg.orientation.y = -1.0*pitch;
            msg.orientation.z = yaw;
            msg.angular_velocity.x = d.g[0];                    // Angular velocity data.               float w[3];    rad/s
            msg.angular_velocity.y = d.g[1];
            msg.angular_velocity.z = d.g[2];
            msg.linear_acceleration.x = d.linAcc[0];        // Linear acceleration x, y and z.      float linAcc[3];   n/g
            msg.linear_acceleration.y = d.linAcc[1];
            msg.linear_acceleration.z = d.linAcc[2];	

            imu_pub.publish(msg); 
            msg_srv = msg;
          }
       ros::spinOnce();
       loop_rate.sleep();
     }
     manager->removeSensor(lpms);   	// Removes the initialized sensor	
     delete manager;                    // Deletes LpmsSensorManager object 
     return 0;	 
}

bool imu_data_callback(imu_ultrasonic_data::imu_data_srv::Request &req , imu_ultrasonic_data::imu_data_srv::Response &resp)
{
    resp.imu_data_ = msg_srv;
    return true;
}
