import argparse
import curses
import zenoh
import json
from dataclasses import dataclass
from datetime import datetime
from pycdr2 import IdlStruct
from pycdr2.types import int8, int32, uint32, uint8, float64, float32, sequence, array
from typing import List
import threading
import traceback
import time
from multi_robot_datatype import BatteryState, OverViewState, JointStates, UWBState, Vector3, Twist
from uwb_manager import UWBLocalizationSystem
import socket
import numpy as np
from SocketDatatype import UnityCMD

class SocketClient():

    def __init__(self, addr, port):

        self.robot_manager = RobotStatusManager(False, robot_num=2)
        self.robot_manager.activeStatusManager()

        self.server_address = addr
        self.server_port = port
        self.connection_success = False
        self.force_quit = False 
        self.DEFAULT_LEN = 70

        self.unity_info = UnityCMD()
        self.connectToServer()
    
    def connectToServer(self):
        if not self.connection_success:
            try:
                self.unity_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.unity_socket.connect((self.server_address, self.server_port))
                print("Successful Connect to Unity Server")
                self.connection_success = True
                self.process_client_thread = threading.Thread(target=self.processData)
            except ConnectionRefusedError:
                self.connection_success = False
    
    def processData(self):
        while not self.force_quit:
            msg_data = self.unity_socket.recv(2048)
            if len(msg_data) < self.DEFAULT_LEN:
                self.decodeData(msg_data)
                self.robot_manager.sendRobotCMD(self.unity_info)
    
    def decodeData(self, msg_data):
        data = json.loads(msg_data.decode('utf-8'))
        robot_id = data.get('ID')
        l_cmd = data.get('Linear')
        a_cmd = data.get('Angular')
        print("ID: {} linear_cmd: {} angular_cmd: {}".format(robot_id, l_cmd, a_cmd))
        self.unity_info.robot_id = robot_id
        self.unity_info.linear_cmd = l_cmd
        self.unity_info.angular_cmd = a_cmd

    def getCMDInfo(self):
        return self.unity_info
    
    def terminateClient(self):
        if self.process_client_thread.is_alive():
            self.force_quit = True
    
    def closeSocketServer(self):
        self.unity_socket.close()
        self.robot_manager.closeStatusManager()



class TurtlebotState():

    def __init__(self, id, zenoh_config):
        
        self.id = str(id)
        self.input_prefix = 'turtlebot' + self.id + '/rt'

        self.battery_state = None
        self.joint_state = None
        self.battery_state_sub_ = None
        self.joint_state_sub_ = None

        self.zenoh_session = zenoh.open(zenoh_config)
        self.createTopicSub()

    def createTopicSub(self):
        self.battery_state_sub_ = self.zenoh_session.declare_subscriber('{}/battery_state'.format(self.input_prefix), self.batteryStateListener)
        self.joint_state_sub_ = self.zenoh_session.declare_subscriber('{}/joint_states'.format(self.input_prefix), self.jointStateListener)
    
    def pubTwistCMD(self, linear_cmd, angular_cmd):
        cmd_topic = self.input_prefix + '/cmd_vel'
        cmd = Twist(linear= Vector3(x=linear_cmd, y=0.0, z=0.0),
                    angular=Vector3(x=0.0, y=0.0, z=angular_cmd))
        self.zenoh_session.put(cmd_topic, cmd.serialize())

    def batteryStateListener(self, sample):
        self.battery_state = BatteryState.deserialize(sample.payload)
        # print('[ voltage: {}, capacity: {}, percentage: {}]'.format(self.battery_state.voltage,
        #                                 self.battery_state.capacity, self.battery_state.percentage))
    
    def jointStateListener(self, sample):
        self.joint_state = JointStates.deserialize(sample.payload)
        # print('[name: {}, velocity: {}]'.format(self.joint_state.name, self.joint_state.velocity))

    def getJointState(self):
        if self.joint_state is not None:
            return self.joint_state
    
    def getBatteryState(self):
        if self.battery_state is not None:
            return self.battery_state
        
    def closeConnect(self):
        if self.battery_state_sub_ is not None:
            self.battery_state_sub_.undeclare()
        if self.joint_state_sub_ is not None:
            self.joint_state_sub_.undeclare()


class RobotStatusManager():
    
    def __init__(self, is_uwb_master = True, robot_num = 1):  

        """ private definition """
        self.zenoh_config_ = None
        self.zenoh_session_ = None
        self.battery_state_sub_ = None
        self.joint_state_sub_ = None
        self.update_thread_enable_ = False

        self.uwb_state = None
        self.is_uwb_master = is_uwb_master
        self.robot_id = None

        self.turtlebot_list = list()
        self.MAX_ROBOT_NUM = robot_num

        # self.input_prefix_ = 'rt' #default setting

        self.output_prefix_ = 'turtlebot'

        """ public definition """

        self.zenohInit()
        


    def setArgParser(self) -> argparse.ArgumentParser:
        arg_parser = argparse.ArgumentParser(prog='robot-manager',
                                                description='zenoh robot manager')
        arg_parser.add_argument('-e', '--connect', type=str, metavar='ENDPOINT', action='append')
        arg_parser.add_argument('-l', '--listen', type=str, metavar='ENDPOINT', action='append')
        arg_parser.add_argument('-m', '--mode', type=str, default='client')
        arg_parser.add_argument('-c', '--config', type=str, metavar='FILE')

        return arg_parser
        
        
    
    def zenohInit(self):
        zenoh_arg = self.setArgParser().parse_args()
        self.zenoh_config_ = zenoh.config_from_file(zenoh_arg.config) if zenoh_arg.config is not None else zenoh.Config()
        if zenoh_arg.mode is not None:
            self.zenoh_config_.insert_json5(zenoh.config.MODE_KEY, json.dumps(zenoh_arg.mode))
        if zenoh_arg.connect is not None:
            self.zenoh_config_.insert_json5(zenoh.config.CONNECT_KEY, json.dumps(zenoh_arg.connect))
        if zenoh_arg.listen is not None:
            self.zenoh_config_.insert_json5(zenoh.config.LISTEN_KEY, json.dumps(zenoh_arg.listen))

        self.zenoh_session_ = zenoh.open(self.zenoh_config_)

        self.initRobotCommunication()
        
    def initRobotCommunication(self):
        for id in range(self.MAX_ROBOT_NUM):
            turlebot_state = TurtlebotState(id, zenoh_config= self.zenoh_config_)
            self.turtlebot_list.append(turlebot_state)
        print("Init Robot State Successful")


    def pubUWBState(self):
        self.zenoh_session_.put('rt/{}/uwb_state'.format(self.output_prefix_), self.generateUWBState().serialize())
    
    def pubOverViewState(self):
        self.zenoh_session_.put('rt/{}/overview_state'.format(self.output_prefix_), self.generateOverViewState().serialize())


    def generateOverViewState(self) -> OverViewState:
        battery_voltage_list, battery_percentage_list, battery_capacity_list = [], [], []
        joint_name_list, joint_velocity_list = [], []
        for turtlebot in self.turtlebot_list:
            battery_voltage_list.append(turtlebot.getBatteryState().voltage)
            battery_percentage_list.append(turtlebot.getBatteryState().percentage)
            battery_capacity_list.append(turtlebot.getBatteryState().capacity)
            joint_name_list.append(turtlebot.getJointState().name)
            joint_velocity_list.append(turtlebot.getJointState().velocity)
        overview_state = OverViewState(battery_voltage = battery_voltage_list,
                                        battery_percentage = battery_percentage_list,
                                        battery_capacity = battery_capacity_list,
                                        joint_name = joint_name_list,
                                        joint_velocity = joint_velocity_list)
        return overview_state
    
    def generateUWBState(self):
        state = self.uwb_system.getTagPosition()
        uwb_state = UWBState(position_x = state[0],
                             position_y = state[1],
                             tag_id = state[2])
        return uwb_state

    def pubRobotStatus(self):
        print("Robot Status Start Publish....")
        self.update_thread_enable_ = True
        while self.update_thread_enable_:
            # self.pubOverViewState()
            if self.is_uwb_master:
                self.pubUWBState()
            time.sleep(0.01)
        print("Robot Status Publish end")
            

    def activeStatusManager(self):
        zenoh.init_logger()
        print("Initial Zenoh...")
        if self.is_uwb_master:
            self.uwb_system = UWBLocalizationSystem()
            self.uwb_system.startLocalizeTag()

        self.update_thread_enable = True
        time.sleep(1) # wait for uwb system poll data
        self.thread_pub_status = threading.Thread(
            target=self.pubRobotStatus,
            daemon= True
        )
        self.thread_pub_status.start()
    
    def sendRobotCMD(self, cmd):
        if cmd.robot_id != -1:
            self.turtlebot_list[cmd.robot_id].pubTwistCMD(cmd.linear_cmd, cmd.angular_cmd)
        else:
            for id in range(self.MAX_ROBOT_NUM):
                self.turtlebot_list[id].pubTwistCMD(0, 0)


    def closeStatusManager(self):
        if self.update_thread_enable_:
            self.update_thread_enable_ = False
        if self.is_uwb_master:
            self.uwb_system.closeSystem()
        self.zenoh_session_.close()
    
    

if __name__ == "__main__":

    server_ip = "192.168.46.215"
    server_port = 8000
    manager = SocketClient(server_ip, server_port)

    while True:
        try:
            cmd = input("CMD: ")
            if cmd == "q":
                manager.terminateClient()
                break
        except Exception as e:
            traceback.print_exc()
            break
    manager.closeSocketServer()
