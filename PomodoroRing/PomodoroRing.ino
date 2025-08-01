//
// Created by Joel Neumann on 14.07.25.
//

//#include "ArduinoBLE.h" // === BLE additions ===
#include "PomodoroRingML_inferencing.h"
#include "LSM6DS3.h"
#include <Wire.h>


#define MOTOR_PIN D2
#define CONVERT_G_TO_MS2    9.80665f
#define MAX_ACCEPTED_RANGE  2.0f

static LSM6DS3 myIMU(I2C_MODE, 0x6A);


struct VibePattern {
    const int* sequence;
    int length;
};


enum CommandType {
    CMD_START,
    CMD_PAUSE,
    CMD_RESUME,
    CMD_SKIP,
    CMD_STOP,
    CMD_STATE,
    CMD_TIME,
    CMD_INVALID
};

CommandType parseCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "start")  return CMD_START;
    if (cmd == "pause")  return CMD_PAUSE;
    if (cmd == "resume") return CMD_RESUME;
    if (cmd == "skip")   return CMD_SKIP;
    if (cmd == "stop")   return CMD_STOP;
    if (cmd == "state")  return CMD_STATE;
    if (cmd == "time")   return CMD_TIME;

    return CMD_INVALID;
}


const int EXTREME_LONG = 1500;
const int LONG = 500;
const int MIDDLE = 200;
const int SHORT = 100;

const int pattern_error_raw[] = {MIDDLE, LONG};
VibePattern pattern_error = {pattern_error_raw, 2};
const int pattern_start_session_raw[] = {EXTREME_LONG, LONG};
VibePattern pattern_start_session = {pattern_start_session_raw, 2};
const int pattern_stop_session_raw[] = {EXTREME_LONG, SHORT, MIDDLE, LONG};
VibePattern pattern_stop_session = {pattern_stop_session_raw, 4};
const int pattern_short_break_raw[] = {LONG, SHORT, MIDDLE, SHORT, MIDDLE, SHORT, MIDDLE, LONG};
VibePattern pattern_short_break = {pattern_short_break_raw, 8};
const int pattern_long_break_raw[] = {LONG, SHORT, MIDDLE, SHORT, LONG, SHORT, LONG, LONG};
VibePattern pattern_long_break = {pattern_long_break_raw, 8};
const int pattern_detect_gesture_raw[] = {MIDDLE, SHORT, MIDDLE, LONG};
VibePattern pattern_detect_gesture = {pattern_detect_gesture_raw, 4};
const int pattern_pause_raw[] = {MIDDLE, SHORT, LONG, SHORT, MIDDLE, LONG};
VibePattern pattern_pause = {pattern_pause_raw, 6};
const int pattern_state_raw[] = {LONG, SHORT, MIDDLE, LONG};
VibePattern pattern_state = {pattern_state_raw, 4};



void playPattern(const VibePattern& pattern) {
    for (int i = 0; i < pattern.length; i++) {
        digitalWrite(MOTOR_PIN, (i % 2 == 0) ? HIGH : LOW);
        delay(pattern.sequence[i]);
    }
    digitalWrite(MOTOR_PIN, LOW);
}

// Pomodoro timings
const unsigned long WORK_DURATION = 25UL * 60 * 1000;
const unsigned long SHORT_BREAK = 5UL * 60 * 1000;
const unsigned long LONG_BREAK = 10UL * 60 * 1000;

enum State { IDLE, WORK, BREAK_SHORT, BREAK_LONG };
State state = IDLE;

unsigned long sessionStart = 0;
unsigned long pausedTime = 0;
bool isPaused = false;
int completedSessions = 0;

/*
// === BLE additions ===
BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic rxChar("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWriteWithoutResponse | BLEWrite, 512);
BLECharacteristic txChar("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLERead | BLENotify, 512);
// === End BLE additions ===

void sendBLE(const String& msg) {
    txChar.writeValue(msg.c_str(), msg.length());
}*/

void print(const String& msg){
    //sendBLE(msg);
    Serial.println(msg);
}

void vibrateMinutes(int minutes) {
    int tens = minutes / 10;
    int ones = minutes % 10;

    print("Remaining minutes: " + String(minutes));

    for (int i = 0; i < tens; i++) {
        digitalWrite(MOTOR_PIN, HIGH); delay(LONG); digitalWrite(MOTOR_PIN, LOW); delay(SHORT);
    }
    delay(LONG);
    for (int i = 0; i < ones; i++) {
        digitalWrite(MOTOR_PIN, HIGH); delay(MIDDLE); digitalWrite(MOTOR_PIN, LOW); delay(SHORT);
    }
}

void startSession() {
    if (state == IDLE){
        state = WORK;
        sessionStart = millis();
        isPaused = false;
        pausedTime = 0;
        print("Session started.");
        playPattern(pattern_start_session);
    }else{
        print("Session already running.");
        playPattern(pattern_error);
    }
}

void stopSession() {
    if (state != IDLE){
        state = IDLE;
        isPaused = false;
        print("Pomodoro stopped. Back to IDLE.");
        playPattern(pattern_stop_session);
        digitalWrite(MOTOR_PIN, LOW);
    }else{
        print("Session already stopped.");
        playPattern(pattern_error);
    }
}

void nextSession() {
    state = WORK;
    sessionStart = millis();
    isPaused = false;
    pausedTime = 0;
    print("Next session started.");
    playPattern(pattern_start_session);
}

void startBreak() {
    if (completedSessions % 4 == 0 && completedSessions != 0) {
        state = BREAK_LONG;
        print("Starting long break.");
        playPattern(pattern_long_break);
    } else {
        state = BREAK_SHORT;
        print("Starting short break.");
        playPattern(pattern_short_break);
    }
    sessionStart = millis();
    isPaused = false;
}

void resumeSession() {
    if (isPaused) {
        sessionStart = millis() - pausedTime;
        isPaused = false;
        print("Resumed.");
        playPattern(pattern_pause);
    }else{
        print("Session not paused.");
        playPattern(pattern_error);
    }
}

void pauseSession() {
    if (!isPaused) {
        pausedTime = millis() - sessionStart;
        isPaused = true;
        print("Paused.");
        playPattern(pattern_pause);
    }else{
        print("Session already paused.");
        playPattern(pattern_error);
    }
}

void skipPhase() {
    if (state == WORK) {
        completedSessions++;
        print("Skipped work session. Starting break.");
        startBreak();
    } else if (state == BREAK_SHORT || state == BREAK_LONG) {
        print("Skipped break. Starting next session.");
        nextSession();
    } else if (state == IDLE) {
        print("Nothing to skip. No active session.");
    } else {
        print("Invalid state for skipping.");
    }
}


void reportMinutes(){
    if (state == IDLE) {
        print("No session/break running.");
        return;
    }
    unsigned long now = millis();
    unsigned long elapsed = isPaused ? pausedTime : (now - sessionStart);
    unsigned long total = (state == WORK) ? WORK_DURATION :
                          (state == BREAK_SHORT) ? SHORT_BREAK :
                          (state == BREAK_LONG) ? LONG_BREAK : 0;
    int minutesLeft = max(0, (int)((total - elapsed) / 60000));
    vibrateMinutes(minutesLeft);
}

void reportState() {
    String stateMsg;
    VibePattern pattern = pattern_error;
    switch (state) {
        case IDLE:
            stateMsg = "Idle (no active session)";
            pattern = pattern_error;
            break;
        case WORK:
            stateMsg = isPaused ? "Paused (work session)" : "Work session";
            pattern = pattern_start_session;
            break;
        case BREAK_SHORT:
            stateMsg = isPaused ? "Paused (short break)" : "Short break";
            pattern = pattern_short_break;
            break;
        case BREAK_LONG:
            stateMsg = isPaused ? "Paused (long break)" : "Long break";
            pattern = pattern_long_break;
            break;
        default:
            stateMsg = "Unknown state";
    }
    print("Current state: " + stateMsg);
    playPattern(pattern_state);
    playPattern(pattern);
}

void handleCommand(String cmd) {
    CommandType command = parseCommand(cmd);
    if(command == CMD_INVALID) {
        print("Invalid Message");
    }

    print("Command received: " + cmd);
    playPattern(pattern_detect_gesture);

    switch (command) {
        case CMD_START:
            startSession();
            break;

        case CMD_STOP:
            stopSession();
            break;

        case CMD_PAUSE:
            pauseSession();
            break;

        case CMD_RESUME:
            resumeSession();
            break;

        case CMD_SKIP:
            skipPhase();
            break;

        case CMD_STATE:
            reportState();
            break;

        case CMD_TIME:
            reportMinutes();
            break;
    }
}


void handleSerial() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        handleCommand(cmd);
        while (Serial.available()) Serial.read();
    }
}

/*/ === BLE handling ===
void onRxCharWritten(BLEDevice central, BLECharacteristic characteristic) {
    int len = characteristic.valueLength();
    const uint8_t* data = characteristic.value();
    String cmd = "";
    for (int i = 0; i < len; i++) cmd += (char)data[i];
    handleCommand(cmd);
}
// === End BLE handling ===
*/
void updatePomodoro() {
    if (state == IDLE || isPaused) return;
    unsigned long now = millis();
    unsigned long elapsed = now - sessionStart;

    if (state == WORK && elapsed >= WORK_DURATION) {
        completedSessions++;
        startBreak();
    } else if (state == BREAK_SHORT && elapsed >= SHORT_BREAK) {
        print("Short break over. Starting next session.");
        nextSession();
    } else if (state == BREAK_LONG && elapsed >= LONG_BREAK) {
        print("Long break over. Starting next session.");
        nextSession();
    }
}

void setup() {
    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);
    Serial.begin(115200);

    /*/ === BLE init ===
    BLE.begin();
    BLE.setLocalName("PomodoroRing");
    BLE.setDeviceName("PomodoroRing");
    BLE.setAdvertisedService(uartService);
    uartService.addCharacteristic(rxChar);
    uartService.addCharacteristic(txChar);
    BLE.addService(uartService);
    rxChar.setEventHandler(BLEWritten, onRxCharWritten);
    BLE.advertise();
    print("BLE UART Ready.");*/


    if (!myIMU.begin()) {
        print("Failed to initialize IMU!");
    }
    else {
        print("IMU initialized");
    }

    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 6) {
        print("ERR: EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME should be equal to 6 (the 6 sensor axes)");
        return;
    }
}

static float ei_get_sign(float number) {
    return (number >= 0.0) ? 1.0 : -1.0;
}

void rf_classifier(){
    float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 };

    for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 6) {
        // Determine the next tick (and then sleep later)
        uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);


        buffer[ix] = myIMU.readFloatAccelX();
        buffer[ix+1] = myIMU.readFloatAccelY();
        buffer[ix+2] = myIMU.readFloatAccelZ();

        buffer[ix+3] = myIMU.readFloatGyroX();
        buffer[ix+4] = myIMU.readFloatGyroY();
        buffer[ix+5] = myIMU.readFloatGyroZ();

        for (int i = 0; i < 3; i++) {
            if (fabs(buffer[ix + i]) > MAX_ACCEPTED_RANGE) {
                buffer[ix + i] = ei_get_sign(buffer[ix + i]) * MAX_ACCEPTED_RANGE;
            }
        }

        buffer[ix + 0] *= CONVERT_G_TO_MS2;
        buffer[ix + 1] *= CONVERT_G_TO_MS2;
        buffer[ix + 2] *= CONVERT_G_TO_MS2;

        delayMicroseconds(next_tick - micros());
    }

    // Turn the raw buffer in a signal which we can the classify
    signal_t signal;
    int err = numpy::signal_from_buffer(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
    if (err != 0) {
        //print("Failed to create signal from buffer (%d)", err);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    err = run_classifier(&signal, &result, false);
    if (err != EI_IMPULSE_OK) {
        //print("ERR: Failed to run classifier (%d)", err);
        return;
    }

    // handle the predictions
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result.classification[ix].value > 0.9f) {
            if(result.classification[ix].label == "circle_cw" || result.classification[ix].label == "circle_ccw"){
                print("Detected gesture: Circle");
                skipPhase();
            }else if(result.classification[ix].label == "double_tab_roll"){
                print("Detected gesture: Double Tap");
                if(state == IDLE){
                    startSession();
                }else {
                    stopSession();
                }
            }else if(result.classification[ix].label == "up_left"){
                print("Detected gesture: Up Left");
                reportMinutes();
            }else if(result.classification[ix].label == "up_right"){
                print("Detected gesture: Up Right");
                reportState();
            }else if(result.classification[ix].label == "triangle"){
                print("Detected gesture: Triangle");
                if(state == IDLE){
                    pauseSession();
                }else {
                    resumeSession();
                }
            }
            break;
        }
    }


#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif
}

void loop() {
    //BLE.poll();
    handleSerial();
    updatePomodoro();
    rf_classifier();
}