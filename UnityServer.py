import socket
import json
import threading
import datetime
from SocketDatatype import UnityCMD
import pickle
import time

class R2xConnection():

    def __init__(self, conn, addr):
        self.addr = addr
        self.conn = conn
        self.terminated = False
        self.connection_lost = False
        self.conn.setblocking(False)

        # self.r2x_tx_lock = threading.Lock()
    
    def sendR2xCmd(self, cmd):
        if not self.connection_lost:
            tcp_cmd = pickle.dumps(cmd)
            self.conn.send(tcp_cmd)
        
    def renewConnection(self, conn, addr):
        self.conn, self.addr = conn, addr
        self.connection_lost = False


class UnityConnection():

    def __init__(self, conn, addr):
        self.addr = addr
        self.conn = conn
        self.connection_lost = False
        self.unity_cmd = UnityCMD()
        self.conn.setblocking(False)

        self.unity_tx_lock = threading.Lock()

    def recvUnityCMD(self):
        if not self.connection_lost:
            msg_data = self.conn.recv(2048)
            data = json.loads(msg_data.decode('utf-8'))
            self.unity_cmd.robot_id = data.get('ID')
            self.unity_cmd.linear_cmd = data.get('Linear')
            self.unity_cmd.angular_cmd = data.get('Angular')
    
    def getUnityCMD(self):
        return self.unity_cmd


class UnityServer():

    def __init__(self, address, port, robot_num = 5):
        
        self.active_robot_id_list = list()
        self.initRobotAccesstList(robot_num)

        ## Socket Setting ## 
        self.incoming_unity_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.robot_manager_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        for index, sock in enumerate([self.incoming_unity_socket, self.robot_manager_socket]):
            sock.bind((address, port + index))
            sock.listen(5)
            sock.setblocking(False)
        
        print("Unity Server activated")

    def initRobotAccesstList(self, robot_num):
        for id in range(robot_num):
            self.active_robot_id_list.append(False)

    def scanForR2xClientConnection(self):
        try:
            r2x_conn, r2x_addr = self.robot_manager_socket.accept()
            print("Connect to Robot Manager Success,  addr: {}".format(r2x_addr))

        except socket.error:
            pass
    
    def scanForUnityClientConnection(self):
        try:
            unity_client_conn, unity_client_addr = self.incoming_unity_socket.accept()
            print("Connect to Unity Client Success, addr: {}".format(unity_client_addr))

        except socket.error:
            pass

    def clientCMDHandler(self):
        pass

if __name__ == "__main__":
    server_ip = '192.168.46.245'
    port = 8000
    demo_server = UnityServer(server_ip, port)
    while True:
        demo_server.scanForUnityClientConnection()
        time.sleep(0.01)

        