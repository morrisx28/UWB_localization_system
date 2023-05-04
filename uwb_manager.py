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
                if len(raw_data) == 16:
                    self.tag_distance = self.processRawData(raw_data)
                    print("distace: {} distance: {} distance: {} ".format(self.tag_distance[0], self.tag_distance[1], self.tag_distance[2]))
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

        self.const_anchor_dis = [2.1, 2.1, 1.5]
        self.base_angle_ = 0
        self.tag_position = [0, 0]

        self.activateUWBManager()
    
    def activateUWBManager(self):
        self.uwb_manager_.startFetchDistance()
        self.uwb_is_active_ = True
    
    def setAnchorDistance(self, a0_a1_dis, a0_a2_dis, a1_a2_dis):
        """                          const_anchor_distance                                 """
        """ [anchor0_anchor1_distance, anchor0_anchor2_distance, anchor1_anchor2_distance] """
        self.const_anchor_dis[0] = a0_a1_dis
        self.const_anchor_dis[1] = a0_a2_dis
        self.const_anchor_dis[2] = a1_a2_dis

    def caculateCosAngle(self, dis_a, dis_b, dis_c):
        sum1 = dis_b**2 + dis_c**2 - dis_a**2
        sum2 = 2 * dis_b * dis_c
        return math.acos(abs(sum1) / abs(sum2))

    def initMapInfo(self):
        anchor_angle = self.caculateCosAngle(self.const_anchor_dis[2], self.const_anchor_dis[1], self.const_anchor_dis[0])
        self.base_angle_ = (math.pi - anchor_angle) / 2

    def caculateTagPosition(self):
        tag_dis = self.uwb_manager_.getUWBDistance()
        if sum(tag_dis) > 0:
            anchor2_tag_ang = self.caculateCosAngle(tag_dis[2], self.const_anchor_dis[1], tag_dis[0])
            anchor1_tag_ang = self.caculateCosAngle(tag_dis[1], self.const_anchor_dis[0], tag_dis[0])
            if anchor1_tag_ang > anchor2_tag_ang:
                theta = anchor1_tag_ang + self.base_angle_
            elif anchor1_tag_ang < anchor2_tag_ang:
                theta = anchor2_tag_ang + self.base_angle_
            else:
                theta = math.pi / 2
            print("tag_dis: {} theta: {}".format(tag_dis[0],theta))
            self.tag_position[0] = tag_dis[0] * math.cos(theta)
            self.tag_position[1] = tag_dis[0] * math.sin(theta)
            print("tag X: {} Y: {}".format(self.tag_position[0], self.tag_position[1]))

    def getTagPosition(self):
        return self.tag_position

    
    def startLocalizeTag(self):
        if self.uwb_is_active_:
            self.stop_localize_thread_ = False
            self.initMapInfo()
            while not self.stop_localize_thread_:
                self.caculateTagPosition()
                time.sleep(0.1)
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

            
