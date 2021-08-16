//
//  rl.ino
//
//
//  Created by Emma Li and Brian Li in 2019.
//

#include <Servo.h>

// The control shield board has the SVG PIN mapping messed up. Below is the correct mapping:
// PIN #8 on board should actually be specified as #11 in program.
// PIN #9 on board should actually be specified as #12 in program.
// PIN #10 on board should actually be specified as #13 in program.
// PIN #11 on board should actually be specified as #8 in program.
// PIN #12 on board should actually be specified as #9 in program.
// PIN #13 on board should actually be specified as #10 in program.

// Motor related variables
Servo tiltServo;
Servo baseServo;
int basePort = 11;
int tiltPort = 13;


// RL control related variables
int tiltIncrAngle = 3; //Rotate tilt angle 3 degree at each time.
int baseIncrDelay = 50; //Set 50 milliseconds to rotate base motor ~5 degree at each time.
int prevAngle = 100; //Set initial position to 100 degree - tracker panel faces east.
int previousV = 0; //Voltage for the last exploited orientation
int sweepTiltAngleRange = 30; // Tilt angle range to sweep for sweep action
int sweepTiltIncrAngle = 6; // Incremental tilt angle change for sweep action


// Data collect related variables
int trackerInputPort = 0;
int baselineInputPort = 1;

//Set 1.5 seconds to give enough time for solar panel to settle in a new position before
//reading voltage output.
int trackerNewPositionSettleDelay = 1500;

//A pre-calibrated mapping btw analogRead value and the corresponding output voltage from
//tracker solar panel.
//This mapping is used to obtain the output voltage value based on the integer value read
//from analogRead() during data collection phase.
int trackerDigitalInt[14] = {3, 4, 7, 10, 16, 34, 65, 129, 297, 569, 665, 727, 797, 827};
float trackerAnalogOpenV[14] =
{4.02, 5.01, 6.01, 7.02, 8.00, 9.05, 10.01, 11.01, 12.01, 12.99, 14.01, 14.99, 16.02, 16.51};

//A pre-calibrated mapping btw analogRead value and the corresponding output voltage from
// baseline solar panel.
//This mapping is used to obtain the output voltage value based on the integer value read
//from analogRead() during data collection phase.
int baselineDigitalInt[14] = {15, 22, 29, 44, 68, 99, 140, 250, 436, 601, 691, 748, 806, 838};
float baselineAnalogOpenV[14] =
{4.04, 5.02, 6.00, 7.01, 8.01, 9.00, 10.01, 11.00, 12.01, 12.97, 14.00, 15.00, 16.01, 16.50};


// Interval for performing RL and collecting data in milliseconds
int interval = 15 * 60 * 1000;

// Initial setup routine auto invoked by Arduino
void setup() {
    tiltServo.attach(tiltPort);
    tiltServo.write(prevAngle);
    baseServo.attach(basePort);
    //Set base motor to stop state.
    baseServo.write(90);
    Serial.begin(9600);
    debugMsg("Starting a new trial...");
}

// Routine repeatedly auto invoked by Arduino
void loop() {
    debugMsg("Starting RL...");
    executeRL();
    debugMsg("Completed RL.");
    collectData();
    debugMsg("Collected data.");
    delay(interval);
}

// Output a message in serial monitor.
void debugMsg(char * debugMsg) {
    Serial.println(debugMsg);
}

// Output a voltage value in serial monitor.
void debugV(int v) {
    Serial.print("Voltage: ");
    Serial.println(v, DEC);
}

// Output a reward value in serial monitor.
void debugReward(int r) {
    Serial.print("Reward: ");
    Serial.println(r, DEC);
}

void debugExploitV(int v) {
    Serial.print("Exploit voltage: ");
    Serial.println(v, DEC);
}

void debugExploreV(int v) {
    Serial.print("Explore voltage: ");
    Serial.println(v, DEC);
}

// Perform tilt clockwise action.
void tiltCwAction() {
    tiltServo.write(prevAngle + tiltIncrAngle);
    prevAngle += tiltIncrAngle;
}

// Perform tilt counterclockwise action.
void tiltCcwAction() {
    tiltServo.write(prevAngle - tiltIncrAngle);
    prevAngle -= tiltIncrAngle;
}

// Perform base clockwise action.
void baseCwAction() {
    baseServo.write(20);
    delay(baseIncrDelay);
    baseServo.write(90);
}

// Perform base counterclockwise action.
void baseCcwAction() {
    baseServo.write(150);
    delay(baseIncrDelay);
    baseServo.write(90);
}

// Backout tilt clockwise action.
void backoutTiltCwAction() {
    tiltCcwAction();
}

// Backout tilt counterclockwise action.
void backoutTiltCcwAction() {
    tiltCwAction();
}

// Backout base clockwise action.
void backoutBaseCwAction() {
    baseCcwAction();
}

// Backout base counterclockwise action.
void backoutBaseCcwAction() {
    baseCwAction();
}

// Composite action: baseCw + tiltCw
void compositeAction() {
    debugMsg("Start composite action.");
    baseCwAction();
    tiltCwAction();
    debugMsg("End composite action.");
}

// Backout composite action.
void backoutCompositeAction() {
    tiltCcwAction();
    baseCcwAction();
}

// Sweep action: brute force 60 degree tilt and pick the best output
// position.
void sweepAction() {
    int curAngle = prevAngle;
    int curV, maxV = -1;
    int maxAngle;
    
    debugMsg("Start sweep action.");
    
    // First explore clockwise with sweepTiltAngleRange amount.
    for (int da = 0; da <= sweepTiltAngleRange; da += sweepTiltIncrAngle) {
        tiltServo.write(curAngle + da);
        delay(trackerNewPositionSettleDelay);
        int curV = analogRead(trackerInputPort);
        if (curV > maxV) {
            maxV = curV;
            maxAngle = curAngle + da;
        }
    }
    
    // Then explore counterclockwise with sweepTiltAngleRange amount.
    for (int da = 0; da <= sweepTiltAngleRange; da += sweepTiltIncrAngle) {
        tiltServo.write(curAngle - da);
        delay(trackerNewPositionSettleDelay);
        int curV = analogRead(trackerInputPort);
        if (curV > maxV) {
            maxV = curV;
            maxAngle = curAngle - da;
        }
    }
    
    // Last, re-position panel's position to maxAngle.
    tiltServo.write(maxAngle);
    prevAngle = maxAngle;
    previousV = maxV;
    
    debugMsg("Finish sweep action.");
}

// Calculate reward.
int calculateReward() {
    // Need to let panel to settle down in a new position for some time.
    // Otherwise, the read value can be skewed by the previous position.
    delay(trackerNewPositionSettleDelay);
    int currentV = analogRead(trackerInputPort);
    int reward = currentV - previousV;
    debugReward(reward);
    // No need to update previousV when reward is <= 0 because we will backout anyway
    if (reward > 0) {
        previousV = currentV;
    }
    return reward;
}

// Exploit a next tilt angle based on current position such that
// the voltage output reward is maximum.
// Return true if a new tilt angle is exploited.
boolean exploitTiltAngle() {
    int reward = 0;
    
    tiltCwAction();
    reward = calculateReward();
    
    if (reward > 0) {
        debugMsg("To continue with cw tilt");
        do {
            tiltCwAction();
            reward = calculateReward();
        } while (reward > 0);
        backoutTiltCwAction();
        return true;
    } else {
        debugMsg("To backout cw tilt");
        int count = 0;
        backoutTiltCwAction();
        debugMsg("To ccw tilt");
        do{
            tiltCcwAction();
            reward = calculateReward();
            count ++;
        } while (reward > 0);
        debugMsg("To backout ccw tilt");
        backoutTiltCcwAction();
        if (count == 1) {
            return false;
        } else {
            return true;
        }
    }
}

// Exploit a next base angle based on current position such that
// the voltage output reward is maximum.
// Return true if a new base angle is exploited.
boolean exploitBaseAngle() {
    int reward = 0;
    debugMsg("To cw base");
    
    baseCwAction();
    reward = calculateReward();
    
    if (reward > 0) {
        debugMsg("To continue cw base");
        do {
            baseCwAction();
            reward = calculateReward();
        } while (reward > 0);
        backoutBaseCwAction();
        return true;
    } else {
        debugMsg("To back out cw base");
        // Count number of times it exploited a new base angle in ccw.
        int count = 0;
        backoutBaseCwAction();
        debugMsg("To ccw base");
        do{
            baseCcwAction();
            reward = calculateReward();
            count++;
        } while (reward > 0);
        backoutBaseCcwAction();
        if (count == 1) {
            return false;
        } else {
            return true;
        }
    }
}


// Explore a different angle with the following actions in sequence:
//    1. Composite action: baseCW + tiltCW
//    2. Sweep action if composite action reward is negative or zero.
//
// If composite action yields positive reward, re-launch executeRL.
void exploreRL() {
    int reward = 0;
    debugMsg("To launch explore RL.");
    
    // Record voltage before kick off explore actions
    int exploitV = previousV;
    debugExploitV(exploitV);
    
    compositeAction();
    reward = calculateReward();
    if (reward > 0) {
        exploitRL();
    } else {
        backoutCompositeAction();
        sweepAction();
    }
    
    // Record voltage after explore actions,
    debugExploreV(previousV);
    
    debugMsg("Done with explore RL.");
}

// Basic RL without exploration.
// Return true means new position being exploited.
boolean exploitRL() {
    boolean baseReturnValue;
    boolean newAngle = false;
    
    debugMsg("To launch basic RL.");
    do {
        if (exploitTiltAngle()) {
            newAngle = true;
        }
        baseReturnValue = exploitBaseAngle();
        if (baseReturnValue) {
            newAngle = true;
        }
    } while (baseReturnValue);
    
    debugMsg("Done with basic RL.");
    
    return newAngle;
}

// Launch reinforcement learning routine.
void executeRL() {
    //Add delay because the panel gets re-positioned to initial place in setup().
    delay(trackerNewPositionSettleDelay);
    
    //Reset our starting voltage measurement because panel gets re-positioned
    //to initial place in setup().
    previousV = analogRead(trackerInputPort);
    
    // Launch basic RL first.
    if (!exploitRL()) {
        // In case no new position, kick off exploration.
        exploreRL();
    }
    
    debugMsg("Done with RL");
}

// Obtained output voltage from tracker solar panel based on the integer read from
// analogRead() function. It first identifies the adjacent digitalInt elements
// that enclose "digitalV"; then perform linear interpolation to determine
// the corresponding output voltage value based on the tracker's pre-calibrated mapping.
float convertTrackerV(int digitalV) {
    int i = 0;
    for (i = 0; i < 14; i++) {
        if (digitalV <= trackerDigitalInt[i]) {
            break;
        }
    }
    if (i == 0) {
        return trackerAnalogOpenV[0];
    }
    if (i == 14) {
        return trackerAnalogOpenV[13];
    }
    float ratio = (trackerAnalogOpenV[i] - trackerAnalogOpenV[i-1]) /
    (trackerDigitalInt[i] - trackerDigitalInt[i-1]);
    return trackerAnalogOpenV[i-1] + ratio * (digitalV - trackerDigitalInt[i-1]);
}

// Obtained output voltage from baseline solar panel based on the integer read from
// analogRead() function. It first identifies the adjacent digitalInt elements
// that enclose "digitalV"; then perform linear interpolation to determine
// the corresponding output voltage value based on the baseline's pre-calibrated mapping.
float convertBaselineV(int digitalV) {
    int i = 0;
    for (i = 0; i < 14; i++) {
        if (digitalV <= baselineDigitalInt[i]) {
            break;
        }
    }
    if (i == 0) {
        return baselineAnalogOpenV[0];
    }
    if (i == 14) {
        return baselineAnalogOpenV[13];
    }
    float ratio = (baselineAnalogOpenV[i] - baselineAnalogOpenV[i-1]) /
    (baselineDigitalInt[i] - baselineDigitalInt[i-1]);
    return baselineAnalogOpenV[i-1] + ratio * (digitalV - baselineDigitalInt[i-1]);
}

// Collect output voltage data from both baseline and tracker panels.
// The timestamp is provided by serial monitor.
void collectData() {
    float tracker = convertTrackerV(analogRead(trackerInputPort));
    Serial.print(tracker);
    Serial.print(',');
    Serial.print(' ');
    
    float baseline = convertBaselineV(analogRead(baselineInputPort));
    Serial.print(baseline);
    Serial.println(' ');
}
