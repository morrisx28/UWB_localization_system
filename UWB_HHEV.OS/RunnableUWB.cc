
#include "acfw/execute/EventBasedRunnable.h"
#include "acfw/log/Logger.h"
#include "acfw/execute/Timer.h"

#include "TinyJson.h"
#include "UWB_manager.h"

#include "services/UWB.h"

using namespace acfw::execute;
using namespace acfw::cm;
using namespace UWB;

class UWB_Runnable : public EventBasedRunnable
{
    services::UWBProvider m_uwb_stateProvider;

    Timer m_timer;
    UWBLocalizeSystem uwb_system;

public:
    UWB_Runnable()
    {
        // Register message handlers (on() methods below)
        registerMsg<Service::State, services::UWBProvider&>(*this);
    }

    void on(const Service::State& state, services::UWBProvider& instance)
    {
        // TODO: Handle state change here
        (void)instance;
        InfoLog("services::UWBProvider state changed to %d", (int)state);
    }


    bool init(const std::string& config) noexcept override
    {
        EventBasedRunnable::init(config);
        // Create services
        if (m_uwb_stateProvider.create(id(), R"(
            {
                "class": "event",
                "role": "server",
                "type": "services::UWB",
                "endpoints": [
                    {
                        "type": "dds",
                        "config": {
                            "topic": "uwb_state"
                        }
                    }
                ],
                "name": "uwb_state"
            }
            )") != Result::OK)
        {
            ErrorLog("Create m_uwb_stateProvider failed");
            return false;
        }

        // Start services
        uwb_system.activeUWBSystem();
        
        if (m_uwb_stateProvider.start() != Result::OK)
        {
            ErrorLog("Start m_uwb_stateProvider failed");
            return false;
        }

        m_timer.reset(Millisec(200), [this]() {
            this->onTimer();
        });
        m_timer.startAfter(Millisec(200));

        return true;
    }

    void onTimer()
    {
        if (m_uwb_stateProvider.getState() == Service::State::STARTED)
        {
            services::UWB evt;
            TAGState uwb_data = uwb_system.getTagState();
            printf("tag X: %f tag Y: %f tag ID: %ld \n", uwb_data.tag_x, uwb_data.tag_y, uwb_data.tag_id);
            evt.position_x = (double)uwb_data.tag_x;
            evt.position_y = (double)uwb_data.tag_y;
            auto r     = m_uwb_stateProvider.sendEvent(&evt);
            InfoLog("Provider send event pos_X=%f pos_Y=%f result=%d",
                evt.position_x,
                evt.position_y,
                (int)r);
        }
    }
};

DEFINE_RUNNABLE_PLUGIN(UWB_Runnable)
