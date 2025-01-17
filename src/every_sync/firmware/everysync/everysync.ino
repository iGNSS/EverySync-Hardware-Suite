// Import all settings for the chosen sensor configuration.
#include "every_sync_configuration.h"

#include <math.h>
#include <ros.h>
#include <std_msgs/UInt8.h>

#include "Arduino.h"

#ifdef USE_ADIS16445
#include <ADIS16445.h>
#elif defined(USE_ADIS16448AMLZ)
#include <ADIS16448AMLZ.h>
#elif defined(USE_ADIS16448BMLZ)
#include <ADIS16448BMLZ.h>
#elif defined(USE_ADIS16460)
#include <ADIS16460.h>
#elif defined(USE_VN100)
#include <VN100.h>
#endif
#include <Camera.h>
#include <Timer.h>
#include <helper.h>
// added 2023.5.1 by Kangkang
#include <Lidar.h>
#include <TriggerableIMU.h>
#include <GNSS.h>

// pps_time_test
#include <ros/time.h>
#include <tf/transform_broadcaster.h>
geometry_msgs::TransformStamped t;
tf::TransformBroadcaster broadcaster;
// Added for pps


static void resetCb(const std_msgs::Bool & /*msg*/) { NVIC_SystemReset(); }

#ifdef ILLUMINATION_MODULE
static void pwmCb(const std_msgs::UInt8 &msg) {
  analogWrite(ILLUMINATION_PWM_PIN, msg.data);
}
#endif


/* ----- ROS ----- */
ros::NodeHandle nh;
ros::Subscriber<std_msgs::Bool> reset_sub("/realvis/reset", &resetCb);
#ifdef ILLUMINATION_MODULE
ros::Subscriber<std_msgs::UInt8> pwm_sub("/realvis/illumination_pwm", &pwmCb);
#endif

/* ----- Timers ----- */
// In the current setup: TC5 -> IMU, TCC0 -> cam0, TCC1 -> cam1, TC3 -> cam2
// (TCC2 is used for pwm on pin 11).
// Be careful, there is NO bookkeeping whether the timer is already used or
// not. Only use a timer once, otherwise there will be unexpected behavior.

// Timer timer_cam0 = Timer((Tcc *)TCC0);
// Timer timer_imu = Timer((TcCount16 *)TC4);
Timer timer_trig_imu = Timer((Tcc *)TCC0);
Timer timer_cam1 = Timer((Tcc *)TCC1);
Timer timer_cam2 = Timer((TcCount16 *)TC3);
Timer timer_lidar = Timer((TcCount16 *)TC5);
Timer timer_gnss = Timer((Tcc *)TCC2);


/* ----- IMU ----- */
#ifdef USE_ADIS16445
ADIS16445 imu(&nh, IMU_TOPIC, IMU_RATE, timer_imu, 10, 2, 9);
#elif defined(USE_ADIS16448AMLZ)
ADIS16448AMLZ imu(&nh, IMU_TOPIC, IMU_RATE, timer_imu, 10, 2, 9);
#elif defined(USE_ADIS16448BMLZ)
ADIS16448BMLZ imu(&nh, IMU_TOPIC, IMU_RATE, timer_imu, 10, 2, 9);
#elif defined(USE_ADIS16460)
ADIS16460 imu(&nh, IMU_TOPIC, IMU_RATE, timer_imu, 10, 2, 9);
#elif defined(USE_VN100)
VN100 imu(&nh, IMU_TOPIC, IMU_RATE, timer_imu);
#elif defined(USE_BMI_088)
Bmi088 imu(&nh, IMU_TOPIC, IMU_RATE, timer_imu,SPI,10,38);
#elif defined(USE_ICM_42688)
ICM42688 imu(&nh, IMU_TOPIC, IMU_RATE, timer_imu,SPI,10);
#endif

/* ----- Cameras ----- */
// Camera cam0(&nh, CAM0_TOPIC, CAM0_RATE, timer_cam0, CAM0_TYPE, CAM0_TRIGGER_PIN,
//             CAM0_EXPOSURE_PIN, false);
Camera cam1(&nh, CAM1_TOPIC, CAM1_RATE, timer_cam1, CAM1_TYPE, CAM1_TRIGGER_PIN,
            CAM1_EXPOSURE_PIN, false);
Camera cam2(&nh, CAM2_TOPIC, CAM2_RATE, timer_cam2, CAM2_TYPE, CAM2_TRIGGER_PIN,
            CAM2_EXPOSURE_PIN, false);

// Camera lidar_test(&nh, LIDAR_TOPIC, LIDAR_RATE, timer_lidar, LIDAR_TYPE, LIDAR_TRIGGER_PIN,
//             LIDAR_EXPOSURE_PIN, false);

TriggerableIMU trig_imu(&nh, TRIG_IMU_TOPIC, TRIG_IMU_RATE, timer_trig_imu, TRIG_IMU_TYPE, TRIG_IMU_TRIGGER_PIN,
            TRIG_IMU_EXPOSURE_PIN, false);

Lidar lidar(&nh, LIDAR_TOPIC, LIDAR_RATE, timer_lidar, LIDAR_TYPE, LIDAR_TRIGGER_PIN,
            LIDAR_EXPOSURE_PIN, false);

GNSS gnss(&nh, EXTCLK_TOPIC, EXTCLK_RATE, timer_gnss, TRIG_IMU_TYPE, EXTCLK_NMEA_PIN,
            EXTCLK_PPS_PIN, false);


void setup() {
  DEBUG_INIT(115200);

/* ----- Define pins and initialize. ----- */
#ifdef ADD_TRIGGERS
  pinMode(ADDITIONAL_TEST_PIN, OUTPUT);
  digitalWrite(ADDITIONAL_TEST_PIN, LOW);
#endif

#ifdef ILLUMINATION_MODULE
  pinMode(ILLUMINATION_PWM_PIN, OUTPUT);
  pinMode(ILLUMINATION_PIN, OUTPUT);
  analogWrite(ILLUMINATION_PWM_PIN, 0);
  digitalWrite(ILLUMINATION_PIN, LOW);
#endif

  // delay(1000);

/* ----- ROS ----- */
#ifndef DEBUG
  nh.getHardware()->setBaud(250000);
  nh.initNode();
  nh.subscribe(reset_sub);

  // pps_time_test
  // broadcaster.init(nh);

#ifdef ILLUMINATION_MODULE
  nh.subscribe(pwm_sub);
#endif
#else
  while (!SerialUSB) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
#endif

  DEBUG_PRINTLN(F("Main: Start setup."));

  // imu.setup();
  // cam0.setup();
  cam1.setup();
  cam2.setup();

  trig_imu.setup();

  lidar.setup();

  gnss.setup();

  /* ----- Initialize all connected cameras. ----- */
  // while (!cam0.isInitialized() || !cam1.isInitialized() ||
  //        !cam2.isInitialized() || !lidar.isInitialized()) {
  while (!trig_imu.isInitialized() || !cam1.isInitialized() ||
         !cam2.isInitialized() || !lidar.isInitialized()) {
    DEBUG_PRINTLN(F("Main: Initializing."));
    trig_imu.initialize();
    cam1.initialize();
    cam2.initialize();

    lidar.initialize();

    gnss.initialize();

#ifndef DEBUG
    nh.spinOnce();
#endif
    delay(1000);
  }

  /* -----  Declare timers ----- */
  // Enable TCC0 and TCC1 timers.
  REG_GCLK_CLKCTRL = static_cast<uint16_t>(
      GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC0_TCC1);
  while (GCLK->STATUS.bit.SYNCBUSY == 1) {
    ; // wait for sync
  }
  // Enable TCC2 (not used) and TC3 timers.
  REG_GCLK_CLKCTRL = static_cast<uint16_t>(
      GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC2_TC3);
  while (GCLK->STATUS.bit.SYNCBUSY == 1) {
    ; // wait for sync
  }

  // Enable TC4 (not used) and TC5 timers.
  REG_GCLK_CLKCTRL = static_cast<uint16_t>(
      GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TC4_TC5);
  while (GCLK->STATUS.bit.SYNCBUSY == 1) {
    ; // wait for sync
  }

  // enable InterruptVector.
  NVIC_EnableIRQ(TCC0_IRQn);
  NVIC_EnableIRQ(TCC1_IRQn);
  
  NVIC_EnableIRQ(TC3_IRQn);
  NVIC_EnableIRQ(TC5_IRQn);

  // NVIC_EnableIRQ(TCC2_IRQn);

  // imu.begin();
  trig_imu.begin();
  cam1.begin();
  cam2.begin();

  lidar.begin();

  gnss.begin();

  /* ----- Interrupt for measuring the exposure time. ----- */
  noInterrupts(); // Disable interrupts to configure them --> delay()'s
  // currently not working!

  DEBUG_PRINTLN(F("Main: Attach interrupts."));
  // attachInterrupt(digitalPinToInterrupt(trig_imu.exposurePin()), exposureEnd0,
  //                 FALLING);
  attachInterrupt(digitalPinToInterrupt(trig_imu.exposurePin()), external_clockGet,
                  RISING);

  attachInterrupt(digitalPinToInterrupt(cam1.exposurePin()), exposureEnd1,
                  FALLING);
  attachInterrupt(digitalPinToInterrupt(cam2.exposurePin()), exposureEnd2,
                  FALLING);

  attachInterrupt(digitalPinToInterrupt(gnss.exposurePin()), external_clockGet,
                  RISING);
  // 为啥不好使。。。

  // DEBUG_PRINTLN(digitalPinToInterrupt(trig_imu.exposurePin()));
  // DEBUG_PRINTLN(digitalPinToInterrupt(cam1.exposurePin()));
  // DEBUG_PRINTLN(digitalPinToInterrupt(cam2.exposurePin()));
  // DEBUG_PRINTLN(digitalPinToInterrupt(gnss.exposurePin()));

  interrupts();

  DEBUG_PRINTLN(F("Main: Setup done."));
}

void loop() {
  trig_imu.publish();
  cam1.publish();
  cam2.publish();

  // imu.publish();

  lidar.publish();
  gnss.publish();

  delayMicroseconds(500);
  //For Transform Buffer


#ifndef DEBUG
  nh.spinOnce();
#endif
}

void TCC0_Handler() { // Called by cam0_timer for camera 0 trigger.
  trig_imu.triggerMeasurement();
    // trig_imu.publish();
}

void TCC1_Handler() { // Called by cam1_timer for camera 1 trigger.
  cam1.triggerMeasurement();
    // cam1.publish();
}

void TC3_Handler() { // Called by imu_timer for imu trigger.
  cam2.triggerMeasurement();
    // cam2.publish();
}

void TC5_Handler() { // Create a series of Pulse for lidar with timestamps
  // lidar.triggerMeasurement();
  // lidar.pps_send_utc_time();
  lidar.pps_send_utc_time_from_100hz();
}

// void TCC2_Handler() {

// }

void exposureEnd0() {
  trig_imu.exposureEnd();
#ifdef ILLUMINATION_MODULE
  // Deactivate the LEDs with the last camera.
  if (!cam1.isExposing() && !cam2.isExposing()) {
    digitalWrite(ILLUMINATION_PIN, LOW);
  }
#endif
}

void exposureEnd1() {
  cam1.exposureEnd();
#ifdef ILLUMINATION_MODULE
  // Deactivate the LEDs with the last camera.
  if (!cam0.isExposing() && !cam2.isExposing()) {
    digitalWrite(ILLUMINATION_PIN, LOW);
  }
#endif
}

void exposureEnd2() {
  cam2.exposureEnd();
#ifdef ILLUMINATION_MODULE
  // Deactivate the LEDs with the last camera.
  if (!cam0.isExposing() && !cam1.isExposing()) {
    digitalWrite(ILLUMINATION_PIN, LOW);
  }
#endif
}

void external_clockGet()
{
  gnss.get_ext_clk_time();
}