#pragma once
#include <M5Unified.h>
#include <memory>
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "launch_motion.h"
#include "orientation_estimator.h"

// One IMU owner on core 0, below the optical ADC owner's priority. UI calls
// involving M5's shared I2C use the same gate. USB/UI stalls on core 1 cannot
// hold this task; flash/cache stalls still count as gaps and invalidate captures.
class StickS3MotionAcquisition {
public:
  struct Snapshot {
    float a[3]={},g[3]={},gravity[3]={},bias[3]={},biasSigma=0;uint64_t accelUs=0,pairedUs=0,fusedUs=0;
    uint32_t accelSamples=0,pairedSamples=0,maxGapUs=0,maxPollUs=0,stackFree=0,gateMisses=0,gaps=0;
    uint32_t fusionSamples=0,fusionResets=0,maxFusionUs=0;
    bool orientationReady=false,rest=false;
  };
  bool begin() {
    if(task_)return true;
    if(!M5.Imu.isEnabled())return false;
    // A failed probe or task allocation can be retried without leaking gates
    // or creating a second owner of the same IMU and fusion state.
    if(!gate_)gate_=xSemaphoreCreateMutex();
    if(!ringGate_)ringGate_=xSemaphoreCreateMutex();
    if(!gate_ || !ringGate_)return false;
    M5.Imu.setCalibration(0,0,0);
    TaskHandle_t created=nullptr;
    if(xTaskCreatePinnedToCore(entry,"launch_imu",8192,this,2,&created,0)!=pdPASS)return false;
    task_=created;return true;
  }
  bool lock(){return !gate_ || xSemaphoreTake(gate_,pdMS_TO_TICKS(20))==pdTRUE;}
  void unlock(){if(gate_)xSemaphoreGive(gate_);}
  bool running()const{return task_!=nullptr;}
  Snapshot snapshot() {
    if(!ringGate_ || xSemaphoreTake(ringGate_,pdMS_TO_TICKS(20))!=pdTRUE)return {};
    const Snapshot s=snapshot_;xSemaphoreGive(ringGate_);return s;
  }
  bool capture(uint64_t start,uint64_t end,float rpm,uint32_t number,LaunchMotion::Trace &out) {
    auto copy=std::unique_ptr<LaunchMotion::Ring>(new(std::nothrow) LaunchMotion::Ring);
    if(!copy || !ringGate_ || xSemaphoreTake(ringGate_,pdMS_TO_TICKS(20))!=pdTRUE)return false;
    *copy=ring_;xSemaphoreGive(ringGate_);
    out=copy->build(start,end,rpm,number);return true;
  }
private:
  LaunchMotion::Ring ring_;LauncherOrientation orientation_;Snapshot snapshot_;
  SemaphoreHandle_t gate_=nullptr,ringGate_=nullptr;TaskHandle_t task_=nullptr;
  static void entry(void *self){static_cast<StickS3MotionAcquisition *>(self)->run();}
  void run() {
    TickType_t last=xTaskGetTickCount();Snapshot s;
    for(;;) {
      if(lock()) {
        const int64_t started=esp_timer_get_time();
        const auto fresh=M5.Imu.update();const auto data=M5.Imu.getImuData();unlock();
        const uint64_t now=esp_timer_get_time();s.maxPollUs=std::max(s.maxPollUs,uint32_t(now-started));
        if(fresh & m5::IMU_Class::sensor_mask_accel) {
          s.a[0]=data.accel.x;s.a[1]=data.accel.y;s.a[2]=data.accel.z;s.accelUs=now;++s.accelSamples;
        }
        const unsigned both=m5::IMU_Class::sensor_mask_accel|m5::IMU_Class::sensor_mask_gyro;
        if((unsigned(fresh)&both)==both) {
          s.g[0]=data.gyro.x;s.g[1]=data.gyro.y;s.g[2]=data.gyro.z;
          if(s.pairedUs){const uint32_t gap=uint32_t(now-s.pairedUs);s.maxGapUs=std::max(s.maxGapUs,gap);if(gap>35000)++s.gaps;}
          s.pairedUs=now;++s.pairedSamples;
          LaunchMotion::Sample sample;sample.us=now;
          for(unsigned j=0;j<3;++j){sample.a[j]=s.a[j];sample.g[j]=s.g[j];
            sample.clipped|=!std::isfinite(s.a[j]) || !std::isfinite(s.g[j]) || std::fabs(s.a[j])>=7.8f || std::fabs(s.g[j])>=1950;}
          const uint64_t fusionStart=esp_timer_get_time();
          if(xSemaphoreTake(ringGate_,pdMS_TO_TICKS(20))==pdTRUE) {
            orientation_.feed(sample,[&](const LaunchMotion::Sample &fused) {
              ring_.feed(fused);s.fusedUs=fused.us;s.orientationReady=fused.fused && fused.settled && !fused.clipped;
              const float up[3]={0,0,1};LaunchMotion::inverse(fused.q).rotate(up,s.gravity);
            });
            xSemaphoreGive(ringGate_);
          }
          s.maxFusionUs=std::max(s.maxFusionUs,uint32_t(esp_timer_get_time()-fusionStart));
          s.fusionSamples=orientation_.updates();s.fusionResets=orientation_.resets();s.rest=orientation_.rest();
          s.biasSigma=orientation_.biasDegrees(s.bias);
        }
      } else ++s.gateMisses;
      s.stackFree=uxTaskGetStackHighWaterMark(nullptr);
      if(xSemaphoreTake(ringGate_,pdMS_TO_TICKS(20))==pdTRUE){snapshot_=s;xSemaphoreGive(ringGate_);}
      vTaskDelayUntil(&last,pdMS_TO_TICKS(5));
    }
  }
};
