import numpy as np
import serial, time
import threading
import traceback
import math

class UWBManager():

    def __init__(self):

        self.serial_port_ = serial.Serial("/dev/ttyUSB0", 115200)
        self.tag_distance = [0, 0, 0, 0]
        self.active_flag_ = False
        self.data_threshold = 10

        self.serial_port_.flush()
        
    def updateSensorData(self):
        while self.active_flag_:
            if self.serial_port_.is_open:
                raw_data = self.serial_port_.readline()
                if len(raw_data) == 16:  # data length should be 16 bytes
                    self.tag_distance = self.processRawData(raw_data)
                    # print("distace: {} distance: {} distance: {} ".format(self.tag_distance[0], self.tag_distance[1], self.tag_distance[2]))
                time.sleep(0.01)
            
    def processRawData(self, raw_data) -> list:
        distance_data = [0, 0, 0, 0]
        distance_data[0] = self.stitchup(raw_data[8], raw_data[7]) # anchor0 to tag
        distance_data[1] = self.stitchup(raw_data[10], raw_data[9]) # anchor1 to tag
        distance_data[2] = self.stitchup(raw_data[12], raw_data[11]) # anchor2 to tag
        # distance_data[3] = self.stitchup(raw_data[14], raw_data[13]) # anchor3 to tag
        return distance_data

    def stitchup(self, high_byte, low_byte):
        return float((high_byte * 256 + low_byte) / 100)
    
    def getUWBDistance(self) -> list:
        return self.tag_distance
    
    def startFetchDistance(self):
        self.active_flag_ = True
        self.thread_update_data= threading.Thread(
            target=self.updateSensorData,
            daemon= True
        )
        self.thread_update_data.start()

    def closeUWBPort(self):
        self.active_flag_ = False
        self.serial_port_.close()
        if not self.serial_port_.is_open:
            print("UWB port closed complete")

class UWBLocalizationSystem():

    def __init__(self):
        self.uwb_manager_ = UWBManager()
        self.uwb_is_active_ = False

        self.tag_position = None
        self.anchor_pos_list = list() # max 4 anchor pos
        self.MAX_ANCHOR_NUM = 4
        self.activateUWBManager()
    
    def activateUWBManager(self):
        self.uwb_manager_.startFetchDistance()
        self.uwb_is_active_ = True

    def caculateTagPosition(self):
        tag_dis = self.uwb_manager_.getUWBDistance()
        if not any(tag_dis) == 0:
            self.processMLE(tag_dis)
    
    def processMLE(self, tag_dis):
        A = np.array([[2 * (self.anchor_pos_list[0][0] - self.anchor_pos_list[-1][0]),2 * (self.anchor_pos_list[0][1] - self.anchor_pos_list[-1][1])],
                      [2 * (self.anchor_pos_list[1][0] - self.anchor_pos_list[-1][0]),2 * (self.anchor_pos_list[1][1] - self.anchor_pos_list[-1][1])]])
        b = np.array([[pow(self.anchor_pos_list[0][0],2) - pow(self.anchor_pos_list[-1][0],2) + pow(self.anchor_pos_list[0][1],2) - pow(self.anchor_pos_list[-1][1],2) + pow(tag_dis[2],2) - pow(tag_dis[0],2)],
                      [pow(self.anchor_pos_list[1][0],2) - pow(self.anchor_pos_list[-1][0],2) + pow(self.anchor_pos_list[1][1],2) - pow(self.anchor_pos_list[-1][1],2) + pow(tag_dis[2],2) - pow(tag_dis[1],2)]])
        temp_X = np.dot(np.linalg.inv(np.dot(A.T,A)),A.T) 
        X = np.dot(temp_X, b)
        self.tag_position =  [X.T[0,0], X.T[0,1]]
        print("tag X: {} Y: {}".format(self.tag_position[0], self.tag_position[1]))
    
    def setAanchorPos(self, anchor_x, anchor_y):
        if len(self.anchor_pos_list) < self.MAX_ANCHOR_NUM:
            anchor_pos = [0, 0]
            anchor_pos[0] = anchor_x
            anchor_pos[1] = anchor_y
            self.anchor_pos_list.append(anchor_pos)
        else:
            print("Anchor pos num out of range")

    def getTagPosition(self):
        if self.tag_position is not None:
            return self.tag_position
    
    def processLoop(self):
        while not self.stop_localize_thread_:
            self.caculateTagPosition()
            time.sleep(0.2)

    
    def startLocalizeTag(self):
        if self.uwb_is_active_:
            self.setAanchorPos(0,0) # anchor 0
            self.setAanchorPos(1.3, 1.8) # anchor 1
            self.setAanchorPos(-1.3, 1.8) # anchor 2
            self.stop_localize_thread_ = False
            self.track_data_thread = threading.Thread(
                target=self.processLoop,
                daemon=True
            )
            self.track_data_thread.start()
        else:
            print("UWB sensor is not activate")

    def closeSystem(self):
        self.stop_localize_thread_ = True
        self.uwb_manager_.closeUWBPort()
    

        

if __name__ == "__main__":

    manager = UWBLocalizationSystem()
    manager.startLocalizeTag()
    while True:
        try:
            cmd = input("CMD: ")
            if cmd == "q":
                break
        except Exception as e:
            traceback.print_exc()
            break
    
    manager.closeSystem()

            
