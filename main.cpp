#include "mbed.h"

#include "pm2_drivers/PESBoardPinMap.h"
#include "pm2_drivers/DebounceIn.h"
#include "pm2_drivers/Servo.h"
#include "pm2_drivers/UltrasonicSensor.h"

bool do_execute_main_task = false; // this variable will be toggled via the user button (blue button) and
                                   // decides whether to execute the main task or not
bool do_reset_all_once = false;    // this variable is used to reset certain variables and objects and
                                   // shows how you can run a code segment only once

// objects for user button (blue button) handling on nucleo board
DebounceIn user_button(USER_BUTTON); // create DebounceIn object to evaluate the user button
                                     // falling and rising edge
void toggle_do_execute_main_fcn();   // custom function which is getting executed when user
                                     // button gets pressed, definition below

// main runs as an own thread
int main()
{
    // set up states for state machine
    enum RobotState {
        INITIAL,
        EXECUTION,
        SLEEP,
        EMERGENCY
    } robot_state = RobotState::INITIAL;

    // attach button fall function address to user button object, button has a pull-up resistor
    user_button.fall(&toggle_do_execute_main_fcn);

    // while loop gets executed every main_task_period_ms milliseconds, this is a
    // simple approach to repeatedly execute main
    const int main_task_period_ms = 20; // define main task period time in ms e.g. 20 ms, there for
                                        // the main task will run 50 times per second
    Timer main_task_timer;              // create Timer object which we use to run the main task
                                        // every main_task_period_ms

    // led on nucleo board
    DigitalOut user_led(USER_LED);

    // additional led
    // create DigitalOut object to command extra led, you need to add an aditional resistor, e.g. 220...500 Ohm
    // a led has an anode (+) and a cathode (-), the cathode needs to be connected to ground via a resistor
    DigitalOut led1(PB_9);

    // mechanical button
    DigitalIn mechanical_button(PC_5); // create DigitalIn object to evaluate mechanical button, you
                                       // need to specify the mode for proper usage, see below
    mechanical_button.mode(PullUp);    // sets pullup between pin and 3.3 V, so that there
                                       // is a defined potential

    // ultra sonic sensor
    UltrasonicSensor us_sensor(PB_D3);
    float us_distance_cm = 0.0f;
    float us_distance_min = 7.0f;
    float us_distance_max = 50.0f;

    // servo
    Servo servo_D0(PB_D0);
    Servo servo_D1(PB_D1);

    // minimal pulse width and maximal pulse width obtained from the servo calibration process
    // futuba S3001
    float servo_D0_ang_min = 0.0150f;
    float servo_D0_ang_max = 0.1150f;
    // reely S0090
    float servo_D1_ang_min = 0.0325f;
    float servo_D1_ang_max = 0.1175f;

    // servo.setNormalisedPulseWidth: before calibration (0,1) -> (min pwm, max pwm)
    // servo.setNormalisedPulseWidth: after calibration (0,1) -> (servo_D0_ang_min, servo_D0_ang_max)
    servo_D0.calibratePulseMinMax(servo_D0_ang_min, servo_D0_ang_max);
    servo_D1.calibratePulseMinMax(servo_D1_ang_min, servo_D1_ang_max);

    // variables to move the servo, this is just an example
    float servo_input = 0.0f;
    int servo_counter = 0; // define servo counter, this is an additional variable
                           // used to command the servo
    const int loops_per_seconds = static_cast<int>(ceilf(1.0f / (0.001f * static_cast<float>(main_task_period_ms))));

    // start timer
    main_task_timer.start();

    // this loop will run forever
    while (true) {
        main_task_timer.reset();

        if (do_execute_main_task) {

            // visual feedback that the main task is executed, setting this once would actually be enough
            led1 = 1;

            // read us sensor distance
            us_distance_cm = us_sensor.read();
	    if (us_distance_cm < 0.0f) {
                us_distance_cm = 0.0f;
            }

            // state machine
            switch (robot_state) {
                case RobotState::INITIAL: {
                    // enable the servo
                    if (!servo_D0.isEnabled())
                        servo_D0.enable();
                    robot_state = RobotState::EXECUTION;

                    break;
		}
                case RobotState::EXECUTION: {
                    // function to map the distance to the servo movement
                    float servo_input = (1 / (us_distance_max - us_distance_min)) * us_distance_cm - (us_distance_min / (us_distance_max - us_distance_min));
                    servo_D0.setNormalisedPulseWidth(servo_input);

                    // if the measurement is outside the min and max range go to SLEEP
                    if ((us_distance_cm < us_distance_min) || (us_distance_cm > us_distance_max)) {
                        robot_state = RobotState::SLEEP;
                    }
                    if (mechanical_button.read()) {
                        robot_state = RobotState::EMERGENCY;
                    }

                    break;
		}
                case RobotState::SLEEP: {
                    // if the measurement is within the min and max range go to EXECUTION
                    if ((us_distance_cm > us_distance_min) || (us_distance_cm < us_distance_max)) {
                        robot_state = RobotState::EXECUTION;
                    }
                    if (mechanical_button.read()) {
                        robot_state = RobotState::EMERGENCY;
                    }

                    break;
		}
                case RobotState::EMERGENCY: {
                    // The transition to the emergency state actually causes
                    // the execution of the commands contained in the else statement,
                    // that is, it disables the servo and resets the values read from the sensors.
                    toggle_do_execute_main_fcn();

                    break;
		}
                default:

                    break; // do nothing
            }
        } else {
            // the following code block gets executed only once
            if (do_reset_all_once) {
                do_reset_all_once = false;

                // reset variables and objects
                led1 = 0;
                servo_D0.disable();
                us_distance_cm = 0.0f;
            }
        }

        // toggling the user led
        user_led = !user_led;

        // printing to the serial terminal
        printf("US distance cm: %f \n", us_distance_cm);

        // read timer and make the main thread sleep for the remaining time span (non blocking)
        int main_task_elapsed_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(main_task_timer.elapsed_time()).count();
        thread_sleep_for(main_task_period_ms - main_task_elapsed_time_ms);
    }
}

void toggle_do_execute_main_fcn()
{
    // toggle do_execute_main_task if the button was pressed
    do_execute_main_task = !do_execute_main_task;
    // set do_reset_all_once to true if do_execute_main_task changed from false to true
    if (do_execute_main_task)
        do_reset_all_once = true;
}




/*

#include "mbed.h"

#include "pm2_drivers/PESBoardPinMap.h"
#include "pm2_drivers/DebounceIn.h"

// TODO: Remove this after all the includes are sorted in our libary. I just added this so everything is compiled.
#include "eigen/Dense.h"
#include "pm2_drivers/AvgFilter.h"
#include "pm2_drivers/DCMotor.h"
#include "pm2_drivers/DebounceIn.h"
#include "pm2_drivers/EncoderCounter.h"
#include "pm2_drivers/GPA.h"
#include "pm2_drivers/IIR_Filter.h"
#include "pm2_drivers/IMU.h"
#include "pm2_drivers/LinearCharacteristics3.h"
#include "pm2_drivers/LineFollower.h"
#include "pm2_drivers/Mahony.h"
#include "pm2_drivers/Motion.h"
#include "pm2_drivers/PID_Cntrl.h"
#include "pm2_drivers/SensorBar.h"
#include "pm2_drivers/Servo.h"
#include "pm2_drivers/ThreadFlag.h"
#include "pm2_drivers/UltrasonicSensor.h"

bool do_execute_main_task = false; // this variable will be toggled via the user button (blue button) and
                                   // decides whether to execute the main task or not
bool do_reset_all_once = false;    // this variable is used to reset certain variables and objects and
                                   // shows how you can run a code segment only once

// objects for user button (blue button) handling on nucleo board
DebounceIn user_button(USER_BUTTON); // create DebounceIn object to evaluate the user button
                                     // falling and rising edge
void user_button_pressed_fcn();      // custom function which is getting executed when user
                                     // button gets pressed, definition below

// main runs as an own thread
int main()
{
    // attach button fall function address to user button object, button has a pull-up resistor
    user_button.fall(&user_button_pressed_fcn);

    // while loop gets executed every main_task_period_ms milliseconds, this is a
    // simple approach to repeatedly execute main
    const int main_task_period_ms = 20; // define main task period time in ms e.g. 20 ms, there for
                                        // the main task will run 50 times per second
    Timer main_task_timer;              // create Timer object which we use to run the main task
                                        // every main_task_period_ms

    // led on nucleo board
    DigitalOut user_led(USER_LED);

    // additional led
    // create DigitalOut object to command extra led, you need to add an aditional resistor, e.g. 220...500 Ohm
    // a led has an anode (+) and a cathode (-), the cathode needs to be connected to ground via a resistor
    DigitalOut led1(PB_9);

    // start timer
    main_task_timer.start();

    // this loop will run forever
    while (true) {
        main_task_timer.reset();

        if (do_execute_main_task) {

            // visual feedback that the main task is executed, setting this once would actually be enough
            led1 = 1;
        } else {
            // the following code block gets executed only once
            if (do_reset_all_once) {
                do_reset_all_once = false;

                // reset variables and objects
                led1 = 0;
            }
        }

        // toggling the user led
        user_led = !user_led;

        // read timer and make the main thread sleep for the remaining time span (non blocking)
        int main_task_elapsed_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(main_task_timer.elapsed_time()).count();
        thread_sleep_for(main_task_period_ms - main_task_elapsed_time_ms);
    }
}

void user_button_pressed_fcn()
{
    // do_execute_main_task if the button was pressed
    do_execute_main_task = !do_execute_main_task;
    if (do_execute_main_task)
        do_reset_all_once = true;
}
*/