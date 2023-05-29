#ifndef UWB_MANAGER_H_
#define UWB_MANAGER_H_

#include <thread>
#include <array>
#include <algorithm>
#include <math.h>

#include "serial_port.h"
#include "BlockQueue.h"

#define MAX_ANCHOR_NUMS (int) 3

using namespace std;

namespace UWB {

    struct UWBData {
        float dis_a0_tag = 0.;
        float dis_a1_tag = 0.;
        float dis_a2_tag = 0.;
        uint64_t tag_id;
    };
    
    struct AnchorPos {
        float x;
        float y;
    };

    struct TAGState {
        float tag_x;
        float tag_y;
        uint64_t tag_id;
    };

    class UWBManager {

        public:
        UWBManager();
        ~UWBManager();

        UWBData getUWBState();
        void closeUWBManager();

        private:
        bool is_serial_opened_ = false;
        BlockQueue<UWBData> *uwb_data_Q_ = new BlockQueue<UWBData>(5);
        UWBData uwb_data_;

        void openSerialPort();
        void readSerialData();
        void processRawData(vector<unsigned char> &raw_data);

        unique_ptr<mn::CppLinuxSerial::SerialPort> serial_port_;
    };

    class UWBLocalizeSystem {

        public:
        UWBLocalizeSystem();
        ~UWBLocalizeSystem();

        void setAnchorPosition(float anchor_x, float anchor_y);
        void activeUWBSystem();
        TAGState getTagState();

        private:
        bool uwb_system_is_active_ = false;
        vector<AnchorPos> anchor_pos_list_;
        BlockQueue<TAGState> *tag_state_Q_ = new BlockQueue<TAGState>(5);
        TAGState tag_state_;

        void caculateTagPosition();
        void processMLE(UWBData &uwb_data); // locate Tag position
        unique_ptr<UWBManager> uwb_manager_;
    };
} // UWB namespace

#endif // UWB_MANAGER_H_