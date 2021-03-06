#ifndef _MSC_VER

#include <unistd.h>
#include <pthread.h>

#else

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

inline void sleep(unsigned int seconds) { Sleep(seconds * 1000); }

inline void pthread_exit(void *value_ptr)
{
    ExitProcess(*(int*)value_ptr);
}

#endif
#include <string.h>

#include <osc/OscOutboundPacketStream.h>
#include <ip/UdpSocket.h>

#include "mec_app.h"
#include "midi_output.h"

#include <mec_api.h>
#include <mec_prefs.h>
#include <processors/mec_mpe_processor.h>

#define OUTPUT_BUFFER_SIZE 1024

//hacks for now
//#define VELOCITY 1.0f
//#define PB_RANGE 2.0f
//#define MPE_PB_RANGE 48.0f

class MecCmdCallback : public mec::ICallback {
public:
    virtual void mec_control(int cmd, void *other) {
        switch (cmd) {
            case mec::ICallback::SHUTDOWN: {
                LOG_0("mec requesting shutdown");
                keepRunning = 0;
                mec_notifyAll();
                break;
            }
            default: {
                break;
            }
        }
    }
};

class MecConsoleCallback : public MecCmdCallback {
public:
    MecConsoleCallback(mec::Preferences &p)
            : prefs_(p),
              throttle_(static_cast<unsigned>(p.getInt("throttle", 0))),
              valid_(true) {
        if (valid_) {
            LOG_0("mecapi_proc enabling for console output, throttle :  " << throttle_);
        }
    }

    bool isValid() { return valid_; }

    void touchOn(int touchId, float note, float x, float y, float z) {
        static std::string topic = "touchOn";
        outputMsg(topic, touchId, note, x, y, z);
    }

    void touchContinue(int touchId, float note, float x, float y, float z) {
        static unsigned long count = 0;
        count++;
        static std::string topic = "touchContinue";
        //optionally display only every N continue messages
        if (throttle_ == 0 || (count % throttle_) == 0) {
            outputMsg(topic, touchId, note, x, y, z);
        }
    }

    void touchOff(int touchId, float note, float x, float y, float z) {
        static std::string topic = "touchOff";
        outputMsg(topic, touchId, note, x, y, z);

    }

    void control(int ctrlId, float v) {
        std::cout << "control - "
                  << " ctrlId: " << ctrlId
                  << " v:" << v
                  << std::endl;
    }

    void outputMsg(std::string topic, int touchId, float note, float x, float y, float z) {
        std::cout << topic << " - "
                  << " touch: " << touchId
                  << " note: " << note
                  << " x: " << x
                  << " y: " << y
                  << " z: " << z
                  << std::endl;
    }

private:
    mec::Preferences prefs_;
    unsigned int throttle_;
    bool valid_;
};


// this is basically T3D, but doesnt have the framemessages (/t3d/frm)
class MecOSCCallback : public MecCmdCallback {
public:
    MecOSCCallback(mec::Preferences &p)
            : prefs_(p),
              transmitSocket_(IpEndpointName(p.getString("host", "127.0.0.1").c_str(), p.getInt("port", 3123))),
              valid_(true),
              touchOffset_(p.getInt("touch offset",1)),
              xOffset_(p.getDouble("x offset",0.5f)),
              yOffset_(p.getDouble("y offset",0.5f))
              {
        if (valid_) {
            LOG_0("mecapi_proc enabling for osc");
        }
    }

    bool isValid() { return valid_; }

    void touchOn(int touchId, float note, float x, float y, float z) {
        std::string topic = "/t3d/tch" + std::to_string(touchId+touchOffset_);
        sendMsg(topic, touchId, note, x+xOffset_, y +yOffset_, z);
    }

    void touchContinue(int touchId, float note, float x, float y, float z) {
        std::string topic = "/t3d/tch" + std::to_string(touchId+touchOffset_);
        sendMsg(topic, touchId, note, x+xOffset_, y +yOffset_, z);
    }

    void touchOff(int touchId, float note, float x, float y, float z) {
        std::string topic = "/t3d/tch" + std::to_string(touchId+touchOffset_);
        sendMsg(topic, touchId, note, x+xOffset_, y +yOffset_, z);
    }

    void control(int ctrlId, float v) {
        osc::OutboundPacketStream op(buffer_, OUTPUT_BUFFER_SIZE);
        op << osc::BeginBundleImmediate
           << osc::BeginMessage("/t3d/control")
           << ctrlId << v
           << osc::EndMessage
           << osc::EndBundle;
        transmitSocket_.Send(op.Data(), op.Size());
    }

    void sendMsg(std::string topic, int touchId, float note, float x, float y, float z) {
        osc::OutboundPacketStream op(buffer_, OUTPUT_BUFFER_SIZE);
        op << osc::BeginBundleImmediate
           << osc::BeginMessage(topic.c_str())
           << x << y << z << note
           << osc::EndMessage
           << osc::EndBundle;
        transmitSocket_.Send(op.Data(), op.Size());
    }

private:
    mec::Preferences prefs_;
    UdpTransmitSocket transmitSocket_;
    char buffer_[OUTPUT_BUFFER_SIZE];
    bool valid_;
    unsigned touchOffset_;
    float yOffset_;
    float xOffset_;
};

class MecMidiProcessor : public mec::Midi_Processor {
public:
    MecMidiProcessor(mec::Preferences &p) : prefs_(p) {
        setPitchbendRange(static_cast<float>(p.getDouble("pitchbend range", 48.0f)));
        std::string device = prefs_.getString("device");
        int virt = prefs_.getInt("virtual", 0);
        if (output_.create(device, virt > 0)) {
            LOG_1("MecMidiProcessor enabling for midi to " << device);
        }
        if (!output_.isOpen()) {
            LOG_0("MecMidiProcessor not open, so invalid for" << device);
        }
    }

    bool isValid() { return output_.isOpen(); }

    void process(mec::Midi_Processor::MidiMsg &m) {
        if (output_.isOpen()) {
            std::vector<unsigned char> msg;

            for (int i = 0; i < m.size; i++) {
                msg.push_back((unsigned char) m.data[i]);
            }
            output_.sendMsg(msg);
        }
    }

private:
    mec::Preferences prefs_;
    MidiOutput output_;
};



class MecMpeProcessor : public mec::MPE_Processor {
public:
    MecMpeProcessor(mec::Preferences &p) : prefs_(p) {
        // p.getInt("voices", 15);
        setPitchbendRange(static_cast<float>(p.getDouble("pitchbend range", 48.0f)));
        std::string device = prefs_.getString("device");
        int virt = prefs_.getInt("virtual", 0);
        if (output_.create(device, virt > 0)) {
            LOG_1("MecMpeProcessor enabling for midi to " << device);
            LOG_1("TODO (MecMpeProcessor) :");
            LOG_1("- MPE init, including PB range");
        }
        if (!output_.isOpen()) {
            LOG_0("MecMpeProcessor not open, so invalid for" << device);
        }
    }

    bool isValid() { return output_.isOpen(); }

    void process(mec::MPE_Processor::MidiMsg &m) {
        if (output_.isOpen()) {
            std::vector<unsigned char> msg;

            for (int i = 0; i < m.size; i++) {
                msg.push_back((unsigned char) m.data[i]);
            }
            output_.sendMsg(msg);
        }
    }

private:
    mec::Preferences prefs_;
    MidiOutput output_;
};


void *mecapi_proc(void *arg) {
    static int exitCode = 0;

    LOG_0("mecapi_proc start");
    mec::Preferences prefs(arg);

    if (!prefs.exists("mec") || !prefs.exists("mec-app")) {
        exitCode = 1; // fail
        pthread_exit(&exitCode);
    }

    mec::Preferences app_prefs(prefs.getSubTree("mec-app"));
    mec::Preferences api_prefs(prefs.getSubTree("mec"));

    mec::Preferences outprefs(app_prefs.getSubTree("outputs"));

    std::unique_ptr<mec::MecApi> mecApi;
    mecApi.reset(new mec::MecApi(arg));

    if (outprefs.exists("midi")) {
        mec::Preferences cbprefs(outprefs.getSubTree("midi"));
        if(cbprefs.getBool("mpe",true)) {
            MecMpeProcessor *pCb = new MecMpeProcessor(cbprefs);
            if (pCb->isValid()) {
                mecApi->subscribe(pCb);
            } else {
                delete pCb;
            }
        } else {
            MecMidiProcessor *pCb = new MecMidiProcessor(cbprefs);
            if (pCb->isValid()) {
                mecApi->subscribe(pCb);
            } else {
                delete pCb;
            }
        }
    }
    if (outprefs.exists("osc")) {
        mec::Preferences cbprefs(outprefs.getSubTree("osc"));
        MecOSCCallback *pCb = new MecOSCCallback(cbprefs);
        if (pCb->isValid()) {
            mecApi->subscribe(pCb);
        } else {
            delete pCb;
        }
    }
    if (outprefs.exists("console")) {
        mec::Preferences cbprefs(outprefs.getSubTree("console"));
        MecConsoleCallback *pCb = new MecConsoleCallback(cbprefs);
        if (pCb->isValid()) {
            mecApi->subscribe(pCb);
        } else {
            delete pCb;
        }
    }

    mecApi->init();

    {
        mecAppLock lock;
        while (keepRunning) {
            mecApi->process();
            mec_waitFor(lock,5);
        }
    }

    // delete the api, so that it can clean up
    LOG_0("mecapi_proc stopping");
    mecApi.reset();
    sleep(1);
    LOG_0("mecapi_proc stopped");

    exitCode = 0; // success
    return nullptr;
}


