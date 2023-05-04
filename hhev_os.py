from multi_robot_datatype import PoseStamped
import zenoh
import argparse
import json
import traceback


class ZenohTestLauncher():

    def __init__(self):
        
        self.arg_parser_ = argparse.ArgumentParser(prog='zenoh-test',
                                                description='zenoh test launcher')
        self.arg_parser_.add_argument('-e', '--connect', type=str, metavar='ENDPOINT', action='append')
        self.arg_parser_.add_argument('-m', '--mode', type=str, default='client')
        self.arg_parser_.add_argument('-l', '--listen', type=str, metavar='ENDPOINT', action='append')
        self.arg_parser_.add_argument('-c', '--config', type=str, metavar='FILE')
        self.zenoh_config_ = None
        self.zenoh_session_ = None

        self.hhevos_state_sub_ = None
        self.hhevos_state = None

        self.input_prefix_ = 'rt'

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
    
    def hhevosListener(self, sample):
        self.hhevos_state = PoseStamped.deserialize(sample.payload) 
        """ For testing and show in screen """
        print('point x: {} point y: {} point z: {} '.format(self.hhevos_state.point_x, 
                                                            self.hhevos_state.point_y, 
                                                            self.hhevos_state.point_z))

    def activeTestLauncher(self):
        zenoh.init_logger()
        print("Initial Zenoh...")
        self.zenoh_session_ = zenoh.open(self.zenoh_config_)
        self.hhevos_state_sub_ = self.zenoh_session_.declare_subscriber('{}/pose_output'.format(self.input_prefix_), self.hhevosListener)
    
    def closeStatusManager(self):
        self.hhevos_state_sub_.undeclare()
        self.zenoh_session_.close()

if __name__ == "__main__":

    zenoh_test = ZenohTestLauncher()
    zenoh_test.activeTestLauncher()

    while True:
        try:
            cmd = input("CMD: ")
            if cmd == "q":
                break
        except Exception as e:
            traceback.print_exc()
            break
    
    zenoh_test.closeStatusManager()