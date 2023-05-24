#include "UWB_manager.h"

namespace UWB {

    UWBManager::UWBManager() {
        openSerialPort();
    }

    UWBManager::~UWBManager() {
        delete uwb_data_Q_;
    }

    void UWBManager::openSerialPort() {
        serial_port_ = make_unique<mn::CppLinuxSerial::SerialPort>("/dev/ttyUSB0", mn::CppLinuxSerial::BaudRate::B_115200);

        serial_port_->SetTimeout(-1); // Block when reading until data received

        if (serial_port_->Open() > 0) {
            is_serial_opened_ = true;
            sleep(1); // wait for serial port
        
            thread update_data_thread(&UWBManager::readSerialData, this);
            update_data_thread.detach();
        }
        else {
            is_serial_opened_ = false;
        }
    }

    void UWBManager::closeUWBManager() {
        is_serial_opened_ = false;

        if (serial_port_)
            serial_port_->Close();
    }

    void UWBManager::processRawData(vector<unsigned char> &raw_data) {
        UWBData uwb_data;
        uwb_data.dis_a0_tag = (static_cast<float>(raw_data[7] << 8) + static_cast<float>(raw_data[6])) * 0.01;
        uwb_data.dis_a1_tag = (static_cast<float>(raw_data[9] << 8) + static_cast<float>(raw_data[8])) * 0.01;
        uwb_data.dis_a2_tag = (static_cast<float>(raw_data[11] << 8) + static_cast<float>(raw_data[10])) * 0.01;
        if (raw_data[3] == 0x0f) 
            uwb_data.tag_id = 0;
        else
            uwb_data.tag_id = static_cast<int>(raw_data[3]);
        uwb_data_Q_->push(uwb_data);
    }

    void UWBManager::readSerialData() {
        string data_string;
        vector<unsigned char> data_vector;

        while (is_serial_opened_) {

            serial_port_->Read(data_string);
            data_vector.clear();
            if (data_string.size() == 16) {
                for (int i = 0; i < data_string.size(); ++i) {
                    data_vector.emplace_back(data_string[i]);
                }
                processRawData(data_vector);
            }

            usleep(1000 * 20);
        }
    }

    UWBData UWBManager::getUWBState() {
        if (uwb_data_Q_->size() != 0) {
            uwb_data_ = uwb_data_Q_->pop();
        }
        return uwb_data_;
    }


    UWBLocalizeSystem::UWBLocalizeSystem() {

        uwb_manager_ = make_unique<UWBManager>();      
    }

    UWBLocalizeSystem::~UWBLocalizeSystem() {
        uwb_system_is_active_ = false;
        uwb_manager_->closeUWBManager();
        delete tag_state_Q_;
    }

    void UWBLocalizeSystem::activeUWBSystem() { 

        setAnchorPosition(0, 0); // set Anchor 0 position
        setAnchorPosition(-0.5, 3.65); // set Anchor 1 position
        setAnchorPosition(-4.34, 1.13); // set Anchor 2 position
        if (anchor_pos_list_.size() == 3) { // require anchor position num 
            uwb_system_is_active_ = true;
            sleep(1); //wait for uwb_manager init
            thread localize_tag_thread(&UWBLocalizeSystem::caculateTagPosition, this);
            localize_tag_thread.detach();
        }
        else {
            printf("Require Anchor Position Numbers not match \n");
        }


    }

    void UWBLocalizeSystem::setAnchorPosition(float anchor_x, float anchor_y) {

        if (anchor_pos_list_.size() < 3) {
            AnchorPos anchor_pos;
            anchor_pos.x = anchor_x;
            anchor_pos.y = anchor_y;
            anchor_pos_list_.emplace_back(anchor_pos);
        }
        else 
            printf("Anchor position num out of range");
    }

    void UWBLocalizeSystem::caculateTagPosition() {

        UWBData uwb_data;
        while (uwb_system_is_active_) {
            uwb_data = uwb_manager_->getUWBState();
            processMLE(uwb_data);
            usleep(1000 * 20);
        }
    }

    void UWBLocalizeSystem::processMLE(UWBData &uwb_data) {

        array<float, 4> A;
        array<float, 2> B;
        array<float, 4> inv_A;
        TAGState tag_state;
        A[0] = 2 * (anchor_pos_list_[0].x - anchor_pos_list_[2].x);
        A[1] = 2 * (anchor_pos_list_[0].y - anchor_pos_list_[2].y);
        A[2] = 2 * (anchor_pos_list_[1].x - anchor_pos_list_[2].x);
        A[3] = 2 * (anchor_pos_list_[1].y - anchor_pos_list_[2].y);

        B[0] = pow(anchor_pos_list_[0].x, 2) - pow(anchor_pos_list_[2].x, 2) + pow(anchor_pos_list_[0].y, 2) - pow(anchor_pos_list_[2].y, 2) + pow(uwb_data.dis_a2_tag, 2) - pow(uwb_data.dis_a0_tag, 2);
        B[1] = pow(anchor_pos_list_[1].x, 2) - pow(anchor_pos_list_[2].x, 2) + pow(anchor_pos_list_[1].y, 2) - pow(anchor_pos_list_[2].y, 2) + pow(uwb_data.dis_a2_tag, 2) - pow(uwb_data.dis_a1_tag, 2); 

        float c = 1 / ((pow(A[0], 2) + pow(A[2], 2)) * (pow(A[1], 2) + pow(A[3], 2)) - (pow(A[0] * A[1] + A[2] * A[3], 2)));

        inv_A[0] = c * (pow(A[1], 2) + pow(A[3], 2)); 
        inv_A[1] = -c * (A[0] * A[1] + A[2] * A[3]);
        inv_A[2] = -c * (A[0] * A[1] + A[2] * A[3]);
        inv_A[3] = c * (pow(A[0], 2) + pow(A[2], 2)); 

        tag_state.tag_x = inv_A[0] * (A[0] * B[0] + A[2] * B[1]) + inv_A[1] * (A[1] * B[0] + A[3] * B[1]);
        tag_state.tag_y = inv_A[2] * (A[0] * B[0] + A[2] * B[1]) + inv_A[3] * (A[1] * B[0] + A[3] * B[1]);
        tag_state.tag_id = uwb_data.tag_id;
        tag_state_Q_->push(tag_state);
    }

    TAGState UWBLocalizeSystem::getTagState() {
        if (tag_state_Q_->size() != 0) {
            tag_state_ = tag_state_Q_->pop();
        }
        return tag_state_;
    }



}