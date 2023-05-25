import numpy as np
import serial, time, datetime
import threading
import traceback
import math
import pickle

class UWBManager():

    def __init__(self):

        self.serial_port_ = serial.Serial("/dev/ttyUSB0", 115200)
        self.tag_data = None
        self.active_flag_ = False

        self.serial_port_.flush()
  
    def updateSensorData(self):
        while self.active_flag_:
            if self.serial_port_.is_open:
                raw_data = self.serial_port_.readline()
                if len(raw_data) == 16:  # data length should be 16 bytes
                    self.tag_data = self.processRawData(raw_data)
                    # print("distace: {} distance: {} distance: {} ".format(self.tag_distance[0], self.tag_distance[1], self.tag_distance[2]))
            
    def processRawData(self, raw_data) -> list:
        distance_data = [0, 0, 0, 0]  
        distance_data[0] = self.stitchup(raw_data[8], raw_data[7]) # anchor0 to tag
        distance_data[1] = self.stitchup(raw_data[10], raw_data[9]) # anchor1 to tag
        distance_data[2] = self.stitchup(raw_data[12], raw_data[11]) # anchor2 to tag
        if raw_data[4] == 0x0f:
            distance_data[3] = 0 # Master Tag ID
        else:
            distance_data[3] = raw_data[4] # Slave Tag ID
        return distance_data

    def stitchup(self, high_byte, low_byte):
        return float((high_byte * 256 + low_byte) / 100)
    
    def getUWBDistance(self) -> list:
        return self.tag_data
    
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

        self.tag_data = None
        self.anchor_pos_list = list() # max 4 anchor pos
        self.save_pos_x = list()
        self.save_pos_y = list()
        self.time_out_count = 0
        self.MAX_ANCHOR_NUM = 4
        self.TIME_OUT_COUNT = 200
        self.activateUWBManager()
    
    def activateUWBManager(self):
        self.uwb_manager_.startFetchDistance()
        self.uwb_is_active_ = True

    def caculateTagPosition(self):
        tag_data = self.uwb_manager_.getUWBDistance()
        if tag_data is None:
            return False
        if not any(tag_data) == 0:
            self.processMLE(tag_data)
        return True
    
    def processMLE(self, tag_dis):
        A = np.array([[2 * (self.anchor_pos_list[0][0] - self.anchor_pos_list[-1][0]),2 * (self.anchor_pos_list[0][1] - self.anchor_pos_list[-1][1])],
                      [2 * (self.anchor_pos_list[1][0] - self.anchor_pos_list[-1][0]),2 * (self.anchor_pos_list[1][1] - self.anchor_pos_list[-1][1])]])
        b = np.array([[pow(self.anchor_pos_list[0][0],2) - pow(self.anchor_pos_list[-1][0],2) + pow(self.anchor_pos_list[0][1],2) - pow(self.anchor_pos_list[-1][1],2) + pow(tag_dis[2],2) - pow(tag_dis[0],2)],
                      [pow(self.anchor_pos_list[1][0],2) - pow(self.anchor_pos_list[-1][0],2) + pow(self.anchor_pos_list[1][1],2) - pow(self.anchor_pos_list[-1][1],2) + pow(tag_dis[2],2) - pow(tag_dis[1],2)]])
        temp_X = np.dot(np.linalg.inv(np.dot(A.T,A)),A.T) 
        X = np.dot(temp_X, b)
        self.tag_data =  [X.T[0,0], X.T[0,1], tag_dis[3]]
        print("tag X: {} Y: {} ID: {}".format(self.tag_data[0], self.tag_data[1], self.tag_data[2]))
        # For Data Collect 
        # self.save_pos_x.append(self.tag_data[0])
        # self.save_pos_y.append(self.tag_data[1])

    def setAanchorPos(self, anchor_x, anchor_y):
        if len(self.anchor_pos_list) < self.MAX_ANCHOR_NUM:
            anchor_pos = [0, 0]
            anchor_pos[0] = anchor_x
            anchor_pos[1] = anchor_y
            self.anchor_pos_list.append(anchor_pos)
        else:
            print("Anchor pos num out of range")

    def getTagPosition(self):
        if self.tag_data is not None:
            return self.tag_data
        else:
            self.tag_data = [0, 0, 0]
            return self.tag_data
    
    def processLoop(self):
        while not self.stop_localize_thread_:
            connect_ok = self.caculateTagPosition()
            if not connect_ok:
                self.checkTimeOut()
            time.sleep(0.01)
    
    def checkTimeOut(self):
        self.time_out_count += 1
        if self.time_out_count == self.TIME_OUT_COUNT:
            self.closeSystem()
            print(" UWB Master Connection Lost ")

    
    def startLocalizeTag(self):
        if self.uwb_is_active_:
            self.setAanchorPos(0,0) # anchor 0
            self.setAanchorPos(-0.5, 3.65) # anchor 1
            self.setAanchorPos(-4.34, 1.13) # anchor 2
            self.stop_localize_thread_ = False
            self.track_data_thread = threading.Thread(
                target=self.processLoop,
                daemon=True
            )
            self.track_data_thread.start()
        else:
            print("UWB sensor is not activate")
    
    def saveData(self):
        fileName = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
        fileName = './data/' + fileName
        with open(fileName + ".x_posdata", 'wb+') as f:
            pickle.dump(self.save_pos_x,f)
        with open(fileName + ".y_posdata", 'wb+') as f:
            pickle.dump(self.save_pos_y,f)

        

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
                manager.saveData()
                break
        except Exception as e:
            traceback.print_exc()
            break
    
    manager.closeSystem()

            
