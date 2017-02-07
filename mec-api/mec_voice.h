#ifndef MEC_VOICES_H_
#define MEC_VOICES_H_

#include <math.h>
#include <vector>
#include <list>

namespace mec {

class Voices {
public:
    Voices(unsigned voiceCount = 15, unsigned velocityCount = 5)
        : maxVoices_(voiceCount), velocityCount_(velocityCount) {
        voices_.resize(maxVoices_);
        for (int i = 0; i < maxVoices_; i++) {
            voices_[i].i_ = i;
            voices_[i].state_ = Voice::INACTIVE;
            voices_[i].id_ = -1;
            freeVoices_.push_back(&voices_[i]);
        }

    };
    virtual ~Voices() {};


    struct Voice {
        int i_;
        int id_;
        float note_;
        float x_;
        float y_;
        float z_;
        float v_;
        unsigned long t_;
        enum {
            INACTIVE,
            PENDING, // velocity
            ACTIVE,
        } state_;

        //velocity, taken from velocity detector
        struct {
            unsigned vcount_;
            float sumx_, sumy_, sumxy_, sumxsq_, x_;
            float scale_, curve_; // comes from config
            float raw_;
        } vel_;
    };

    Voice*     voiceId(unsigned id) {
        for (int i = 0; i < maxVoices_; i++) {
            if (voices_[i].id_ == id)
                return &voices_[i];
        }
        return NULL;
    }

    Voice*    startVoice(unsigned id) {
        Voice* voice;
        if (freeVoices_.size() > 0) {
            voice = freeVoices_.front();
            freeVoices_.pop_front();
        } else {
            // all voices used, use oldestActiveVoice
            // if you wish to steal it
            return NULL;
        }
        voice->id_ = id;
        voice->state_ = Voice::PENDING;
        voice->v_ = 0;

        voice->vel_.scale_ = 1.0f;
        voice->vel_.curve_ = 1.0f;
        voice->vel_.vcount_ = 0;
        voice->vel_.sumx_ = voice->vel_.sumy_ = voice->vel_.sumxy_ = voice->vel_.sumxsq_ = 0.0;
        voice->vel_.x_ = 0.0;

        voice->vel_.sumx_ += voice->vel_.x_;
        voice->vel_.sumxsq_ += voice->vel_.x_ * voice->vel_.x_;
        voice->vel_.x_++;

        voice->vel_.sumx_ += voice->vel_.x_;
        voice->vel_.sumxsq_ += voice->vel_.x_ * voice->vel_.x_;
        voice->vel_.x_++;


        usedVoices_.push_back(voice);
        return voice;
    }

    void   addPressure(Voice* voice, float p) {
        if (voice->state_ == Voice::PENDING) {


            if (voice->vel_.vcount_ < velocityCount_ )
            {
                voice->vel_.sumx_   += voice->vel_.x_;
                voice->vel_.sumy_   += p;
                voice->vel_.sumxy_  += (voice->vel_.x_ * p) ;
                voice->vel_.sumxsq_ += (voice->vel_.x_ * voice->vel_.x_);
                voice->vel_.vcount_ ++;
                voice->vel_.x_ ++;

                if(p <= 1.0 ) {
                    return;
                }
                // else max pressure, so consider 'complete'
            }

            voice->state_ = Voice::ACTIVE;
            voice->vel_.raw_ = voice->vel_.scale_ *
                               (voice->vel_.x_ * voice->vel_.sumxy_ - (voice->vel_.sumx_ * voice->vel_.sumy_))
                               / (voice->vel_.x_ * voice->vel_.sumxsq_ - (voice->vel_.sumx_ * voice->vel_.sumx_));
            voice->v_  = 1 - pow ( (double) (1 - voice->vel_.raw_), (double)(voice->vel_.curve_));
        }
    }

    void   stopVoice(Voice* voice) {
        if (!voice) return;
        usedVoices_.remove(voice);
        voice->id_ = -1;
        voice->note_ = 0;
        voice->x_ = 0;
        voice->y_ = 0;
        voice->z_ = 0;
        voice->t_ = 0;
        voice->state_ = Voice::INACTIVE;
        freeVoices_.push_back(voice);
    }

    Voice* oldestActiveVoice() {
        return usedVoices_.front();
    }


private:
    std::vector<Voice> voices_;
    std::list<Voice*> freeVoices_;
    std::list<Voice*> usedVoices_;
    unsigned maxVoices_;
    unsigned velocityCount_;
};
}

#endif //MEC_VOICES_H_
