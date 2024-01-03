#!/usr/bin/env python3

import rospy
from melfa_driver.msg import io

IO_data= 0
def callback(io_msg: io):
    global IO_data
    IO_data =  io_msg.values

def sendToRobot(iodata:int):
    io_msgs = io() 
    io_msgs.values = 2 ** 8 * iodata
    io_msgs.sending_inputs = 0
    io_msgs.reading_inputs = 0
    pub.publish(io_msgs)
    
if __name__ == '__main__':
    rospy.init_node('GPIO_node')
    sub = rospy.Subscriber('/io_robot', io, callback=callback)
    pub = rospy.Publisher('/io_controller',io,queue_size=10)
    rate = rospy.Rate(10) # 10hz
    while not rospy.is_shutdown():
        try:
            data = int(input("Mit 1 wird der Magnet An mit 0 Aus geschaltet: "))
            if data not in [0, 1]:
                raise ValueError("Die Eingabe muss entweder 0 oder 1 sein.")
            sendToRobot(data)
        except ValueError as e:
            print(f"Fehlerhafte Eingabe: {e}")

        rate.sleep()