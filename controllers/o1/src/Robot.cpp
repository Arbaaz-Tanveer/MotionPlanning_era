#include "Robot.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <boost/asio.hpp>


using namespace robot;


//initializing ports for arduino communication
boost::asio::io_service io;
boost::asio::serial_port serial1(io, "/dev/ttyUSB0"); // Replace with your serial port name for Arduino 1
boost::asio::serial_port serial2(io, "/dev/ttyUSB1"); // Replace with your serial port name for Arduino 2

void configureSerialPorts() {
    serial1.set_option(boost::asio::serial_port_base::baud_rate(9600));
    serial1.set_option(boost::asio::serial_port_base::character_size(8));
    serial1.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
    serial1.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
    serial1.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));

    serial2.set_option(boost::asio::serial_port_base::baud_rate(9600));
    serial2.set_option(boost::asio::serial_port_base::character_size(8));
    serial2.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
    serial2.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
    serial2.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));
}


RobotKinematic *RobotKinematic::instance = 0;

RobotKinematic *RobotKinematic::getInstance() {
    if (instance == 0) {
        instance = new RobotKinematic();
    }
    return instance;
}

void RobotKinematic::setInitialPosition(double x, double y, double theta) {
    pos.x = x;
    pos.y = y;
    pos.theta = theta;
}

void RobotKinematic::calculateOdometry(const double ori) {
    for (int i = 0; i < 4; i++) {
        double tick = (encData[i] - prevEnc[i]);
        Venc[i] = tick * (circumference / (2 * M_PI));
    }

    double b = sqrt(2);
    Point2D output;

    // For encoder:
    // output.x = ((-b * Venc[0]) - (b * Venc[1]) + (b * Venc[2]) + (b *
    // Venc[3])) / 4; output.y = ((b * Venc[0]) - (b * Venc[1]) - (b * Venc[2])
    // + (b * Venc[3])) / 4; output.theta = (Venc[0] + Venc[1] + Venc[2] +
    // Venc[3]) / (4 * L);

    forwardKinematics(output, Venc[0], Venc[1], Venc[2], Venc[3]);

    double angleNorm = angleNormalize(pos.theta);
    pos.theta = angleNorm;

    Point2D velGlobal;

    // If use orientation from odometry
    // velGlobal.x = (std::cos(angleNorm) * output.x) - (std::sin(angleNorm) *
    // output.y); velGlobal.y = (std::cos(angleNorm) * output.y) +
    // (std::sin(angleNorm) * output.x);

    // If using IMU
    velGlobal.x = (std::cos(ori) * output.x) - (std::sin(ori) * output.y);
    velGlobal.y = (std::cos(ori) * output.y) + (std::sin(ori) * output.x);

    // Position from Odometry
    pos.x += velGlobal.x / 100;
    pos.y += velGlobal.y / 100;
    // pos.theta += output.theta;

    // From IMU for orientation
    pos.theta = ori;

    prevEnc = encData;
}

double RobotKinematic::angleNormalize(double angle) {
    if (angle > M_PI) angle -= 2 * M_PI;
    if (angle < -M_PI) angle += 2 * M_PI;

    return angle;
}

Point2D RobotKinematic::getPos() { return pos; }

void RobotKinematic::forwardKinematics(Point2D &outForward, double s1,
                                       double s2, double s3, double s4) {
    double b = sqrt(2);
    outForward.x = ((-b * s1) - (b * s2) + (b * s3) + (b * s4)) / 4;
    outForward.y = ((b * s1) - (b * s2) - (b * s3) + (b * s4)) / 4;
    outForward.theta = (s1 + s2 + s3 + s4) / (4 * L);
}

std::string encode_w(double w1, double w2)
{
    w1*=(900/M_PI);
    w2*=(900/M_PI);
    std::string str1 = std::to_string((int)abs(w1)), str2 = std::to_string((int)abs(w2));

    if (str1.length() < 4) {
        str1 = std::string(4 - str1.length(), '0') + str1;
    }
    if (str2.length() < 4) {
        str2 = std::string(4 - str2.length(), '0') + str2;
    }

    return str1 + str2 + std::to_string(w1<0) + std::to_string(w2<0) + '\n';
}


void sendData(boost::asio::serial_port& serial, const std::string& data) {
    try {
        boost::asio::write(serial, boost::asio::buffer(data));
        std::cout << "Data sent: " << data << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Error sending data: " << e.what() << std::endl;
    }
}


void RobotKinematic::inverseKinematics(wheelAngularVel &outputInverse,
                                       double velglobal_x, double velglobal_y,
                                       double velglobal_theta) {
    outputInverse.w1 =
        (-cos(a * M_PI / 180) * velglobal_x +
         sin(a * M_PI / 180) * velglobal_y + L * velglobal_theta) /
        r_wheel;
    outputInverse.w2 =
        (-cos(a * M_PI / 180) * velglobal_x -
         sin(a * M_PI / 180) * velglobal_y + L * velglobal_theta) /
        r_wheel;
    outputInverse.w3 =
        (cos(a * M_PI / 180) * velglobal_x - sin(a * M_PI / 180) * velglobal_y +
         L * velglobal_theta) /
        r_wheel;
    outputInverse.w4 =
        (cos(a * M_PI / 180) * velglobal_x + sin(a * M_PI / 180) * velglobal_y +
         L * velglobal_theta) /
        r_wheel;
    
    //sending data to arduino
    std::string data1 = encode_w(outputInverse.w1, outputInverse.w2);
    std::string data2 = encode_w(outputInverse.w3, outputInverse.w4);
    sendData(serial1, data1);
    sendData(serial2, data2);

    std::cout << encode_w(outputInverse.w1, outputInverse.w2) + " " + encode_w(outputInverse.w3, outputInverse.w4) + '\n';
    
    return;
}