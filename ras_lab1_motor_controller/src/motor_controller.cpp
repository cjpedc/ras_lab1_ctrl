#include "ros/ros.h"
#include "std_msgs/Float64.h"
#include "std_msgs/Int32.h"
#include <ras_arduino_msgs/PWM.h>
#include <ras_arduino_msgs/Encoders.h>
#include <geometry_msgs/Twist.h>
#include <sstream>


class motor_controller
{
public:

    ros::NodeHandle n_;
    ros::Subscriber twist_subscriber_;
    ros::Subscriber encoders_subscriber_;
    ros::Publisher pwm_publisher_;
    std::vector<float> twist_;
    std::vector<float> encoder_;
    const static float control_frequency = 10.0; // Control @ 10 Hz

    motor_controller()
    {

        n_ = ros::NodeHandle("~");

        twist_= std::vector<float>(2, 0);
        encoder_= std::vector<float>(5,0);

        twist_subscriber_ = n_.subscribe("/motor_controller/twist", 1, &motor_controller::twistCallback, this);
        encoders_subscriber_ = n_.subscribe("/kobuki/encoders", 1, &motor_controller::encodersCallback, this);
        pwm_publisher_ = n_.advertise<ras_arduino_msgs::PWM>("/kobuki/pwm", 1000);
    }

    void twistCallback(const geometry_msgs::Twist::ConstPtr &msg)
    {
        twist_[0] = msg->linear.x;
        twist_[1] = msg->angular.z;
       // ROS_INFO("linear: [%f]", twist_[0]); Receiving
       // ROS_INFO("angular: [%f]", twist_[1]); Receiving
    }

    void encodersCallback(const ras_arduino_msgs::Encoders::ConstPtr &msg)
    {
        encoder_[0] = msg->encoder1;
        encoder_[1] = msg->encoder2;
        encoder_[2] = msg->delta_encoder1;
        encoder_[3] = msg->delta_encoder2;
        encoder_[4] = msg->timestamp;
        //ROS_INFO("I heard: [%d]", encoder_[0]);
    }


    // wheel angular velocity
    std::vector<float> wheels_ang_vel (std::vector<float> twist_)
    {
     float b = 0.23; // separation of the two central wheels in [m]
     float r = 0.0352; //  wheel radius in [m]
     std::vector<float> wlr_;
     wlr_ = std::vector<float>(2,0);

     float v = twist_[0]; // robot linear velocity
     float w = twist_[1]; // robot angular velocity

     wlr_[0] = (v - (b/2) * w)/r; //left wheel angular velocity
     wlr_[1] = (v + (b/2) * w)/r; //right wheel angular velocity

     // ROS_INFO("Left Wheel: [%f]", wlr_[0]); working
     // ROS_INFO("Right angular: [%f]", wlr_[1]); working

     return wlr_;
    }

    // controller
    std::vector<int> controller (std::vector<float> wlr_, std::vector<int> actual_pwm)
    {

     std::vector<int> pwm_;
     pwm_ = std::vector<int>(2,0);

     std::vector<float> estimated_w_;
     estimated_w_ = std::vector<float>(2,0);

     float wl = wlr_[0]; // left wheel angular velocity
     float wr = wlr_[1]; // right wheel angular velocity
     float T = control_frequency; // Control @ 10 Hz
     int ticks = 360; //ticks_per_rev
     float pi = 3.14159265359;
     float alpha = 3; // P gain

     //motor_controller motor_controller_node;

     float delta_encoder1 = (float) motor_controller::encoder_[2];
     float delta_encoder2 = (float) motor_controller::encoder_[3];


     /*
       estimated_w = (delta_encoder*2*pi*control_frequency)/(ticks_per_rev)
       pwm = pwm + alpha*(desired_w - estimated_w)
     */

     estimated_w_[0] = (delta_encoder1*2*pi*T)/ticks;
     estimated_w_[1] = (delta_encoder2*2*pi*T)/ticks;

     pwm_[0] = (int) (actual_pwm[0] + alpha * (wl - estimated_w_[0])); //PWM1

     if (pwm_[0]>255)
     {
         pwm_[0] = 255;
     }

     if (pwm_[0]<-255)
     {
         pwm_[0] = -255;
     }

     pwm_[1] = (int) (actual_pwm[1] + alpha * (wr - estimated_w_[1])); //PWM2

     if (pwm_[1]>255)
     {
      pwm_[1] = 255;
     }

     if (pwm_[1]<-255)
     {
      pwm_[1] = -255;
     }

     return pwm_;
    }

private:
/*
    std::vector<float> twist_;
    std::vector<float> encoder_;
*/
};



int main(int argc, char **argv)
{

  std::vector<float> ww_;
  ww_ = std::vector<float>(2,0);
/*
  std::vector<int> PWM_;
  PWM_ = std::vector<int>(2,0);
  PWM_[0] = 0; // (PWM_1)
  PWM_[1] = 0; // (PWM_2)

  std::vector<int> rPWM_;
  rPWM_ = std::vector<int>(2,0);
  rPWM_[0] = 50; //reference (ref PWM_1)
  rPWM_[1] = 50; //reference (refl PWM_2)
 */

  std::vector<int> PWM_;
  PWM_ = std::vector<int>(2,0);
  PWM_[0] = 50; //reference (ref PWM_1)
  PWM_[1] = 50; //reference (ref PWM_2)

  ros::init(argc, argv, "motor_controller");

  motor_controller motor_controller_node;

  ros::Rate loop_rate(motor_controller::control_frequency);

  ras_arduino_msgs::PWM msg;

  while (ros::ok())
  {

    ww_ = motor_controller_node.wheels_ang_vel(motor_controller_node.twist_);

    PWM_ = motor_controller_node.controller(ww_,PWM_);

    msg.PWM1 = PWM_[0];
    msg.PWM2 = PWM_[1];

    motor_controller_node.pwm_publisher_.publish(msg);

    ros::spinOnce();
    loop_rate.sleep();

  }


  return 0;
}
