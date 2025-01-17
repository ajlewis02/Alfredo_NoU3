#include "esp32-hal-ledc.h"
#include "Arduino.h"
#include "Alfredo_NoU3.h"

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_NOU3)
NoU_Agent NoU3;
#endif

uint8_t RSL::state = RSL_OFF;

float fmap(float val, float in_min, float in_max, float out_min, float out_max) {
    return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

NoU_Motor::NoU_Motor(uint8_t motorPort)
{
    this->motorPort = motorPort;
    set(0);
}

void NoU_Motor::set(float output) {
	if(!inverted) NoU3.setMotor(this->motorPort, applyCurve(output));
	else NoU3.setMotor(this->motorPort, -1 * applyCurve(output));
}

float NoU_Motor::applyCurve(float input) {
    return fmap((fabs(input) < deadband ? 0 : 1) // apply deadband
                * pow(max(fmap(constrain(fabs(input), -1, 1), deadband, 1, 0, 1), 0.0f), exponent), // account for deadband, apply exponent
                0, 1, minimumOutput, maximumOutput) // apply minimum and maximum output limits
            * (input == 0 ? 0 : input > 0 ? 1 : -1) // apply original sign
			* (inverted ? -1 : 1); // account for inversion
}

void NoU_Motor::setMinimumOutput(float minimumOutput) {
    minimumOutput = constrain(minimumOutput, 0, maximumOutput);
    this->minimumOutput = minimumOutput;
}

void NoU_Motor::setMaximumOutput(float maximumOutput) {
    maximumOutput = constrain(maximumOutput, minimumOutput, 1);
    this->maximumOutput = maximumOutput;
}

void NoU_Motor::setDeadband(float deadband) {
    deadband = constrain(deadband, 0, 1);
    this->deadband = deadband;
}

void NoU_Motor::setExponent(float exponent) {
    exponent = max(0.0f, exponent);
    this->exponent = exponent;
}

void NoU_Motor::setInverted(boolean inverted) {
    this->inverted = inverted;
}

NoU_Servo::NoU_Servo(uint8_t servoPort, uint16_t minPulse, uint16_t maxPulse) {
    switch (servoPort) {
        case 1:
            pin = PIN_SERVO_1;
            channel = CHANNEL_SERVO_1;
            break;
        case 2:
            pin = PIN_SERVO_2;
            channel = CHANNEL_SERVO_2;
            break;
        case 3:
            pin = PIN_SERVO_3;
            channel = CHANNEL_SERVO_3;
            break;
        case 4:
            pin = PIN_SERVO_4;
            channel = CHANNEL_SERVO_4;
            break;
    }
    this->minPulse = minPulse;
    this->maxPulse = maxPulse;
    ledcSetup(channel, SERVO_PWM_FREQ, SERVO_PWM_RES);
    ledcAttachPin(pin, channel);
}

void NoU_Servo::write(float degrees) {
    writeMicroseconds(fmap(degrees, 0, 180, minPulse, maxPulse));
}

void NoU_Servo::writeMicroseconds(uint16_t pulseLength) {
    this->pulse = pulseLength;
    ledcWrite(channel, fmap(pulseLength, 0, 20000, 0, (1 << SERVO_PWM_RES) - 1));
}

void NoU_Servo::setMinimumPulse(uint16_t minPulse) {
    this->minPulse = minPulse;
}

void NoU_Servo::setMaximumPulse(uint16_t maxPulse) {
    this->maxPulse = maxPulse;
}

uint16_t NoU_Servo::getMicroseconds() {
    return pulse;
}

float NoU_Servo::getDegrees() {
    return fmap(pulse, minPulse, maxPulse, 0, 180);
}

NoU_Drivetrain::NoU_Drivetrain(NoU_Motor* leftMotor, NoU_Motor* rightMotor)
    : frontLeftMotor(leftMotor), frontRightMotor(rightMotor),
        rearLeftMotor(), rearRightMotor(), drivetrainType(TWO_MOTORS)
{ }

NoU_Drivetrain::NoU_Drivetrain(NoU_Motor* frontLeftMotor, NoU_Motor* frontRightMotor,
                                NoU_Motor* rearLeftMotor, NoU_Motor* rearRightMotor)
    : frontLeftMotor(frontLeftMotor), frontRightMotor(frontRightMotor),
        rearLeftMotor(rearLeftMotor), rearRightMotor(rearRightMotor), drivetrainType(FOUR_MOTORS)
{ }

float NoU_Drivetrain::applyInputCurve(float input) {
    return (fabs(input) < inputDeadband ? 0 : 1) // apply deadband
            * pow(max(fmap(constrain(fabs(input), -1, 1), inputDeadband, 1, 0, 1), 0.0f), inputExponent) // account for deadband, apply exponent
            * (input > 0 ? 1 : -1); // apply original sign
}

void NoU_Drivetrain::setMotors(float frontLeftPower, float frontRightPower, float rearLeftPower, float rearRightPower) {
    switch (drivetrainType) {
        case FOUR_MOTORS:
            rearLeftMotor->set(rearLeftPower);
            rearRightMotor->set(rearRightPower);
        case TWO_MOTORS:
            frontLeftMotor->set(frontLeftPower);
            frontRightMotor->set(frontRightPower);
    }
}

void NoU_Drivetrain::tankDrive(float leftPower, float rightPower) {
    leftPower = applyInputCurve(leftPower);
    rightPower = applyInputCurve(rightPower);
    setMotors(leftPower, rightPower, leftPower, rightPower);
}

void NoU_Drivetrain::arcadeDrive(float throttle, float rotation, boolean invertedReverse) {
    throttle = applyInputCurve(throttle);
    rotation = applyInputCurve(rotation);
    float leftPower = 0;
    float rightPower = 0;
    float maxInput = (throttle > 0 ? 1 : -1) * max(fabs(throttle), fabs(rotation));
    if (throttle > 0) {
        if (rotation > 0) {
            leftPower = maxInput;
            rightPower = throttle - rotation;
        }
        else {
            leftPower = throttle + rotation;
            rightPower = maxInput;
        }
    } else {
        if (rotation > 0) {
            leftPower = invertedReverse ? maxInput : throttle + rotation;
            rightPower = invertedReverse ? throttle + rotation : maxInput;
        }
        else {
            leftPower = invertedReverse ? throttle - rotation : maxInput;
            rightPower = invertedReverse ? maxInput : throttle - rotation;
        }
    }
    setMotors(leftPower, rightPower, leftPower, rightPower);
}

void NoU_Drivetrain::curvatureDrive(float throttle, float rotation, boolean isQuickTurn) {
    throttle = applyInputCurve(throttle);
    rotation = applyInputCurve(rotation);

    float angularPower;
    boolean overPower;

    if (isQuickTurn) {
        if (fabs(throttle) < quickStopThreshold) {
            quickStopAccumulator = (1 - quickStopAlpha) * quickStopAccumulator + quickStopAlpha * rotation * 2;
        }
        overPower = true;
        angularPower = rotation;
    }
    else {
        overPower = false;
        angularPower = fabs(throttle) * rotation - quickStopAccumulator;

        if (quickStopAccumulator > 1) quickStopAccumulator--;
        else if (quickStopAccumulator < -1) quickStopAccumulator++;
        else quickStopAccumulator = 0;
    }

    float leftPower;
    float rightPower;

    leftPower = throttle + angularPower;
    rightPower = throttle - angularPower;

    if (overPower) {
        if (leftPower > 1) {
            rightPower -= leftPower - 1;
            leftPower = 1;
        } else if (rightPower > 1) {
            leftPower -= rightPower - 1;
            rightPower = 1;
        } else if (leftPower < -1) {
            rightPower -= leftPower + 1;
            leftPower = -1;
        } else if (rightPower < -1) {
            leftPower -= rightPower + 1;
            rightPower = -1;
        }
    }
    float maxMagnitude = max(fabs(leftPower), fabs(rightPower));
    if (maxMagnitude > 1) {
        leftPower /= maxMagnitude;
        rightPower /= maxMagnitude;
    }
    setMotors(leftPower, rightPower, leftPower, rightPower);
}

void NoU_Drivetrain::holonomicDrive(float xVelocity, float yVelocity, float rotation) {
    if (drivetrainType == TWO_MOTORS) return;
    xVelocity = applyInputCurve(xVelocity);
    yVelocity = applyInputCurve(yVelocity);
    rotation = applyInputCurve(rotation);
    float frontLeftPower = xVelocity + yVelocity + rotation;
    float frontRightPower = -xVelocity + yVelocity - rotation;
    float rearLeftPower = -xVelocity + yVelocity + rotation;
    float rearRightPower = xVelocity + yVelocity - rotation;
    float maxMagnitude = max(fabs(frontLeftPower), max(fabs(frontRightPower), max(fabs(rearLeftPower), fabs(rearRightPower))));
    if (maxMagnitude > 1) {
        frontLeftPower /= maxMagnitude;
        frontRightPower /= maxMagnitude;
        rearLeftPower /= maxMagnitude;
        rearRightPower /= maxMagnitude;
    }
    setMotors(frontLeftPower, frontRightPower, rearLeftPower, rearRightPower);
}

void NoU_Drivetrain::setMinimumOutput(float minimumOutput) {
    switch (drivetrainType) {
        case FOUR_MOTORS:
            rearLeftMotor->setMinimumOutput(minimumOutput);
            rearRightMotor->setMinimumOutput(minimumOutput);
        case TWO_MOTORS:
            frontLeftMotor->setMinimumOutput(minimumOutput);
            frontRightMotor->setMinimumOutput(minimumOutput);
    }
}

void NoU_Drivetrain::setMaximumOutput(float maximumOutput) {
    switch (drivetrainType) {
        case FOUR_MOTORS:
            rearLeftMotor->setMaximumOutput(maximumOutput);
            rearRightMotor->setMaximumOutput(maximumOutput);
        case TWO_MOTORS:
            frontLeftMotor->setMaximumOutput(maximumOutput);
            frontRightMotor->setMaximumOutput(maximumOutput);
    }
}

void NoU_Drivetrain::setInputExponent(float inputExponent) {
    inputExponent = max(0.0f, inputExponent);
    this->inputExponent = inputExponent;
}

void NoU_Drivetrain::setInputDeadband(float inputDeadband) {
    inputDeadband = constrain(inputDeadband, 0, 1);
    this->inputDeadband = inputDeadband;
}

void RSL::initialize() {
    ledcSetup(RSL_CHANNEL, RSL_PWM_FREQ, RSL_PWM_RES);
    ledcAttachPin(RSL_PIN, RSL_CHANNEL);
}

void RSL::setState(uint8_t state) {
    RSL::state = state;
}

void RSL::update() {
    switch (state) {
        case RSL_OFF:
            ledcWrite(RSL_CHANNEL, 1);
            break;
        case RSL_ON:
            ledcWrite(RSL_CHANNEL, (1 << RSL_PWM_RES) - 1);
            break;
        case RSL_ENABLED:
            ledcWrite(RSL_CHANNEL, millis() % 1000 < 500 ? (millis() % 500) * 2 : (500 - (millis() % 500)) * 2);
            break;
        case RSL_DISABLED:
            ledcWrite(RSL_CHANNEL, (1 << RSL_PWM_RES) - 1);
            break;
    }
}
