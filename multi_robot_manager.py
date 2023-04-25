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
from multi_robot_datatype import BatteryState, OverViewState, JointStates
    

class RobotStatusManager():
    
    def __init__(self):  

        """ private definition """
        self.arg_parser_ = argparse.ArgumentParser(prog='robot-manager',
                                                description='zenoh robot manager')
        self.arg_parser_.add_argument('-e', '--connect', type=str, metavar='ENDPOINT', action='append')
        self.arg_parser_.add_argument('-l', '--listen', type=str, metavar='ENDPOINT', action='append')
        self.arg_parser_.add_argument('-m', '--mode', type=str, default='client')
        self.arg_parser_.add_argument('-c', '--config', type=str, metavar='FILE')
        self.arg_parser_.add_argument('-robot', '--robot', type=str, default='turtlebot', choices=['turtlebot', 'spider'])
        self.arg_parser_.add_argument('-id', '--id', type=str, default='1')
        self.zenoh_config_ = None
        self.zenoh_session_ = None
        self.battery_state_sub_ = None
        self.joint_state_sub_ = None
        self.update_thread_enable_ = False

        self.battery_state = None
        self.joint_state = None

        self.input_prefix_ = 'rt' #default setting
        """ public definition """

        self.zenohInit()
    
    def zenohInit(self):
        zenoh_arg = self.arg_parser_.parse_args()
        self.zenoh_config_ = zenoh.config_from_file(zenoh_arg.config) if zenoh_arg.config is not None else zenoh.Config()
        if zenoh_arg.mode is not None:
            self.zenoh_config_.insert_json5(zenoh.config.MODE_KEY, json.dumps(zenoh_arg.mode))
        if zenoh_arg.connect is not None:
            self.zenoh_config_.insert_json5(zenoh.config.CONNECT_KEY, json.dumps(zenoh_arg.connect))
        if zenoh_arg.listen is not None:
            self.zenoh_config_.insert_json5(zenoh.config.LISTEN_KEY, json.dumps(zenoh_arg.listen))

        if zenoh_arg.robot == 'turtlebot':
            self.output_prefix_ = 'turtlebot' + zenoh_arg.id

    
    def batteryStateListener(self, sample):
        self.battery_state = BatteryState.deserialize(sample.payload)
        # print('[ voltage: {}, capacity: {}, percentage: {}]'.format(self.battery_state.voltage,
        #                                 self.battery_state.capacity, self.battery_state.percentage))
    
    def jointStateListener(self, sample):
        self.joint_state = JointStates.deserialize(sample.payload)
        # print('[name: {}, velocity: {}]'.format(self.joint_state.name, self.joint_state.velocity))

    def getBatteryState(self) -> BatteryState:
        if self.battery_state is not None:
            return self.battery_state
        
    def getJointState(self) -> JointStates:
        if self.joint_state is not None:
            return self.joint_state
    
    def pubOverViewState(self):
        self.zenoh_session_.put('rt/{}/overview_state'.format(self.output_prefix_), self.generateOverViewState().serialize())


    def generateOverViewState(self) -> OverViewState:
        overview_state = OverViewState(battery_voltage = float32(self.battery_state.voltage),
                                            battery_percentage = float32(self.battery_state.capacity),
                                            battery_capacity = float32(self.battery_state.capacity),
                                            joint_name = self.joint_state.name,
                                            joint_velocity = self.joint_state.velocity)
        print(overview_state)
        return overview_state

    def pubRobotStatus(self):
        print("Robot Status Start Publish....")
        self.update_thread_enable_ = True
        while self.update_thread_enable_:
            if self.battery_state is not None and self.joint_state is not None:
                self.pubOverViewState()
            time.sleep(0.01)
        print("Robot Status Publish end")
            

    def activeStatusManager(self):
        zenoh.init_logger()
        print("Initial Zenoh...")
        self.update_thread_enable = True
        self.zenoh_session_ = zenoh.open(self.zenoh_config_)
        self.battery_state_sub_ = self.zenoh_session_.declare_subscriber('{}/battery_state'.format(self.input_prefix_), self.batteryStateListener)
        self.joint_state_sub_ = self.zenoh_session_.declare_subscriber('{}/joint_states'.format(self.input_prefix_), self.jointStateListener)
        self.thread_pub_status = threading.Thread(
            target=self.pubRobotStatus,
            daemon= True
        )
        self.thread_pub_status.start()


    def closeStatusManager(self):
        if self.update_thread_enable_:
            self.update_thread_enable_ = False
        self.battery_state_sub_.undeclare()
        self.joint_state_sub_.undeclare()
        self.zenoh_session_.close()
    

if __name__ == "__main__":

    manager = RobotStatusManager()
    manager.activeStatusManager()

    while True:
        try:
            cmd = input("CMD: ")
            if cmd == "q":
                break
        except Exception as e:
            traceback.print_exc()
            break
    
    manager.closeStatusManager()
