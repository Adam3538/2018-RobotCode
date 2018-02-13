#include <iostream>
#include <memory>
#include <string>
#include "math.h"
#include <algorithm>

// And So It Begins...
#include "RJ_RobotMap.h"

class Robot: public frc::TimedRobot {

	// Robot Hardware Setup
	RJ_RobotMap IO;

	// Built-In Drive code for teleop
	DifferentialDrive Adrive { IO.DriveBase.MotorsLeft, IO.DriveBase.MotorsRight };

	// Drive Input Filter
	float OutputX = 0.0, OutputY = 0.0;

	// Teleop Elevator Position
	double CurrentElevPos =0.0;

	// create pdp variable
	PowerDistributionPanel *pdp = new PowerDistributionPanel();

	// State Variables
	bool driveButtonYPrev = false;
	bool intakeDeployed = false;
	bool XYDeployed = false;
	bool ElevHold = false;
	bool NotHome = true;
	int DpadMove = -1;
	int dpadvalue=-1;
	double ElevIError =0;

	//Autonomous Variables
	int isWaiting = 0;
	Timer AutonTimer;
	std::string gameData, autoDelay, autoSelected, DriveEncoder;
	int AutoVal, modeState, DriveState, TurnState, ScaleState, NearSwitch,
			AutoSpot, LeftMode;
	bool AutonOverride, AutoDelayActive;

	void RobotInit() {
		//disable drive watchdogs
		Adrive.SetSafetyEnabled(false);
	}

	void RobotPeriodic() {
		// Update Smart Dash
		SmartDashboardUpdate();
		IO.NavXDebugger();
		gameData = frc::DriverStation::GetInstance().GetGameSpecificMessage();
	}

	void DisabledPeriodic() {
		// NOP
	}

	void TeleopInit() {
		// drive command averaging filter
		OutputX = 0, OutputY = 0;
		// Teleop Elevator Position
			while(!elevatorHome());
			CurrentElevPos =0.0;
			DpadMove = -1;
			dpadvalue=-1;
			ElevIError =0;

	}

#define ElevDeadband (0.125)	// deadband for elevator gears and motors, value to move elevator up

	void TeleopPeriodic() {
		double Control_Deadband = 0.11;
		double Drive_Deadband = 0.11;
		double DPadSpeed = 1.0;
		double Gain = 1;

		//high gear & low gear controls
		if (IO.DS.DriveStick.GetRawButton(5))
			IO.DriveBase.SolenoidShifter.Set(true); // High gear press LH bumper

		if (IO.DS.DriveStick.GetRawButton(6))
			IO.DriveBase.SolenoidShifter.Set(false); // Low gear press RH bumper

		//  Rumble code
		//  Read all motor current from PDP and display on drivers station
		//double driveCurrent = pdp->GetTotalCurrent();	// Get total current
		double driveCurrent = pdp->GetTotalCurrent();

		// rumble if current to high
		double LHThr = 0.0;		// Define value for rumble
		if (driveCurrent > 125.0)// Rumble if greater than 125 amps motor current
			LHThr = 0.5;
		Joystick::RumbleType Vibrate;				// define Vibrate variable
		Vibrate = Joystick::kLeftRumble;		// set Vibrate to Left
		IO.DS.DriveStick.SetRumble(Vibrate, LHThr); // Set Left Rumble to RH Trigger
		Vibrate = Joystick::kRightRumble;		// set vibrate to Right
		IO.DS.DriveStick.SetRumble(Vibrate, LHThr);	// Set Right Rumble to RH Trigger

		//drive controls
		double SpeedLinear = IO.DS.DriveStick.GetY(GenericHID::kLeftHand) * 1; // get Yaxis value (forward)
		double SpeedRotate = IO.DS.DriveStick.GetX(GenericHID::kRightHand) * -1; // get Xaxis value (turn)

		//Smoothing algorithm for x^3
		if (!IO.DriveBase.SolenoidShifter.Get()) {
			if (SpeedLinear > Control_Deadband)
				OutputY = Drive_Deadband + (Gain * pow(SpeedLinear, 3));
			else if (SpeedLinear < -Control_Deadband)
				OutputY = -Drive_Deadband + (Gain * pow(SpeedLinear, 3));
			else
				OutputY = 0;
		} else {
			if (SpeedLinear > Control_Deadband)
				OutputY = Drive_Deadband + (Gain * pow(SpeedLinear, 3));
			else if (SpeedLinear < -Control_Deadband)
				OutputY = -Drive_Deadband + (Gain * pow(SpeedLinear, 3));
			else
				OutputY = 0;
		}

		// Set dead band for X and Y axis
		if (fabs(SpeedLinear) < Control_Deadband)
			SpeedLinear = 0.0;
		if (fabs(SpeedRotate) < Control_Deadband)
			SpeedRotate = 0.0;

		//slow down direction changes from 1 cycle to 5
		OutputY = (0.8 * OutputY) + (0.2 * SpeedLinear);
		OutputX = (0.8 * OutputX) + (0.2 * SpeedRotate);

		// Drive Code
		Adrive.ArcadeDrive(OutputY, OutputX, true);

		// Joystick OutPuts
		SmartDashboard::PutNumber("YJoystick", SpeedLinear);
		SmartDashboard::PutNumber("XJoystick", SpeedRotate);

		SmartDashboard::PutNumber("OutputY", OutputY);
		SmartDashboard::PutNumber("OutputX", OutputX);

		/*
		 * MANIP CODE
		 */

		//Elevator manual drive
		bool SwitchElevatorUpper = IO.DriveBase.SwitchElevatorUpper.Get();
		bool SwitchElevatorLower = IO.DriveBase.SwitchElevatorLower.Get();

		// reversing controller input so up gives positive input
		double ElevatorStick = IO.DS.OperatorStick.GetY(
				frc::XboxController::kLeftHand) * -1;
		//double ElevatorStick = IO.DS.OperatorStick.GetX(frc::XboxController::kLeftHand);
		//int ElevatorDpadDown = IO.DS.OperatorStick.GetPOV(180);

		double ElevatorOutput = ElevatorStick;
		double ElevatorDeadband = ElevDeadband; // deadband for elevator gears and motors, value to move elevator up

		//Smoothing algorithm for x^3
		if (ElevatorStick > Control_Deadband)
			ElevatorOutput = ElevatorDeadband + (Gain * pow(ElevatorStick, 3));
		else if (ElevatorStick < -Control_Deadband)
			ElevatorOutput = 0 + (Gain * pow(ElevatorStick, 3)); //due to gravity deadband is not required for the elevator to move down.
		else
			ElevatorOutput =0;



			dpadvalue= IO.DS.OperatorStick.GetPOV();// Read Operator Dpad value

			if(fabs(ElevatorStick)>ElevatorDeadband)
				DpadMove = -1;
			else if (dpadvalue != -1)
				DpadMove = dpadvalue;

		if (!ElevHold) CurrentElevPos = IO.DriveBase.EncoderElevator.Get(); // Read Elevator encoder value

		if (fabs(ElevatorStick) < Control_Deadband and dpadvalue == -1) {
			elevatorPosition(CurrentElevPos,false); 	// hold elevator position.
			ElevHold = true;
		} else if (ElevatorStick > Control_Deadband and !SwitchElevatorUpper  and dpadvalue == -1) {
			elevatorPosition(CurrentElevPos, false); //  hold  top elevator position.
			ElevHold = true;
		} else if (ElevatorStick < Control_Deadband and !SwitchElevatorLower and dpadvalue == -1) {
			elevatorSpeed(0); //  hold  bottom elevator position.
			IO.DriveBase.EncoderElevator.Reset(); // Reset encoder to 0;
		}
		else {
			elevatorSpeed(ElevatorOutput);
			ElevHold = false;
		}

		// Joystick OutPuts
		SmartDashboard::PutNumber("ElevatorStick", ElevatorStick);
		SmartDashboard::PutNumber("ElevatorOutput", ElevatorOutput);
		SmartDashboard::PutBoolean("SwitchElevatorUpper", SwitchElevatorUpper);
		SmartDashboard::PutBoolean("SwitchElevatorLower", SwitchElevatorLower);

		SmartDashboard::PutNumber("dpadvalue", dpadvalue);



		// Elevator automatic drive based on dpad

		if (fabs(ElevatorStick) < Control_Deadband) {
			switch(DpadMove) {
				case 270:
					//Dpad is Pointing Left
					//Portal/Switch height
					if(elevatorPosition(1000, false)){
						DpadMove=-1;
						CurrentElevPos=1000;
						ElevHold = true;
					}
					break;
				case 90:
					//dpad is pointing to the Right
					// elevator at max height
					if(elevatorPosition(2000, false)){
						DpadMove=-1;
						CurrentElevPos=2000;
						ElevHold = true;
					}
					break;
				case 180:
					// dpad is pointing down
					// ground/intake level
					if(elevatorPosition(0, false)){
						DpadMove=-1;
						CurrentElevPos=0;
						ElevHold = true;
					}
					break;
				case 0:
					// dpad is pointing to the UP
					// this is for "scale low" whatever that means
					if(elevatorPosition(5000, false)){
						DpadMove=-1;
						CurrentElevPos=5000;
						ElevHold = true;
					}
					break;
				}
			}

		// Claw control

		bool ClawIntake = IO.DS.OperatorStick.GetBumper(frc::GenericHID::kRightHand);
		bool ClawEject = IO.DS.OperatorStick.GetBumper(frc::GenericHID::kLeftHand);

		if (ClawIntake and !ClawEject) {
			IO.DriveBase.ClawClamp.Set(frc::DoubleSolenoid::kOff);
			IO.DriveBase.Claw1.Set(1);
		} else if (ClawEject and !ClawIntake) {
			IO.DriveBase.ClawClamp.Set(frc::DoubleSolenoid::kForward);
			IO.DriveBase.Claw1.Set(1);
		} else {
			IO.DriveBase.ClawClamp.Set(frc::DoubleSolenoid::kForward);
		}


		// Wrist control
		bool WristRight = IO.DS.OperatorStick.GetBumper(
						frc::GenericHID::kRightHand);
		bool WristLeft = IO.DS.OperatorStick.GetBumper(
				frc::GenericHID::kLeftHand);

		int OperatorRightAxis = IO.DS.OperatorStick.GetTriggerAxis(frc::GenericHID::kRightHand);
		int OperatorLeftAxis = IO.DS.OperatorStick.GetTriggerAxis(frc::GenericHID::kLeftHand);

		int WristOutput_teleop=0;

		if (OperatorRightAxis > 0 and OperatorLeftAxis > 0) {
			WristOutput_teleop = 0;
		}
		else if (OperatorRightAxis > 0 and OperatorLeftAxis == 0) {
			WristOutput_teleop = OperatorRightAxis;
		}
		else if (OperatorRightAxis == 0 and OperatorLeftAxis > 0) {
			WristOutput_teleop = -OperatorLeftAxis;
		}



		int wriststatus=0;

		if (WristRight == true and WristLeft == false){
			wriststatus = 1;
		}
		else if (WristRight == false and WristLeft ==true){
			wriststatus = 2;
		}
		else if (WristRight == true and WristLeft == true){
			//do nothing
		}


		if (abs(WristOutput_teleop) > 0) {
		IO.DriveBase.Wrist1.Set(WristOutput_teleop);
		wriststatus = 0;
		}
		else {
			wristPosition(wriststatus);
		}


		//A Button to extend (Solenoid On)
		IO.TestJunk.Abutton.Set(IO.DS.OperatorStick.GetRawButton(1));

		//B Button to extend (Solenoid On)
		IO.TestJunk.Bbutton.Set(IO.DS.OperatorStick.GetRawButton(2));

		//if Left Bumper button pressed, extend (Solenoid On)
		if (IO.DS.OperatorStick.GetRawButton(5)) {
			intakeDeployed = true;
			IO.TestJunk.IntakeButton.Set(intakeDeployed);
		}

		//else Right Bumper pressed, retract (Solenoid Off)
		else if (IO.DS.OperatorStick.GetRawButton(6)) {
			intakeDeployed = false;
			IO.TestJunk.IntakeButton.Set(intakeDeployed);
		}
		//if 'X' button pressed, extend (Solenoid On)
		if (IO.DS.OperatorStick.GetRawButton(3)) {
			XYDeployed = true;
			IO.TestJunk.XYbutton.Set(XYDeployed);
		}

		//else 'Y' button pressed, retract (Solenoid Off)
		else if (IO.DS.OperatorStick.GetRawButton(4)) {
			XYDeployed = false;
			IO.TestJunk.XYbutton.Set(XYDeployed);
		}

		//dpad POV stuff
		if (IO.DS.OperatorStick.GetPOV(0) == 0) {
			IO.TestJunk.Dpad1.Set(DPadSpeed);
			IO.TestJunk.Dpad2.Set(DPadSpeed);
		} else if (IO.DS.OperatorStick.GetPOV(0) == 180) {
			IO.TestJunk.Dpad1.Set(-DPadSpeed);
			IO.TestJunk.Dpad2.Set(-DPadSpeed);
		} else {
			IO.TestJunk.Dpad1.Set(0.0);
			IO.TestJunk.Dpad2.Set(0.0);
		}

	}


	void AutonomousInit() {
		//AutoProgram.Initalize();

		modeState = 1;
		isWaiting = 0;							/////***** Rename this.

		AutonTimer.Reset();
		AutonTimer.Start();
		// Encoder based auton
		IO.DriveBase.EncoderLeft.Reset();
		IO.DriveBase.EncoderRight.Reset();
		// Turn off drive motors
		IO.DriveBase.MotorsLeft.Set(0);

		IO.DriveBase.MotorsRight.Set(0);

		//zeros the navX
		IO.DriveBase.ahrs.ZeroYaw();

		//forces robot into low gear
		IO.DriveBase.SolenoidShifter.Set(false);

		//makes sure claw clamps shut
		IO.DriveBase.ClawClamp.Set(DoubleSolenoid::Value::kForward);

	}

#define caseLeft 1
#define caseRight 2

	void AutonomousPeriodic() {

		SmartDashboard::PutString("gameData", gameData);
		if (gameData[0] == 'L')
			NearSwitch = caseLeft;
		else
			NearSwitch = caseRight;
		// Set far switch game state
		if (gameData[1] == 'L')
			ScaleState = caseLeft;
		else
			ScaleState = caseRight;

		if (autoDelay == IO.DS.sAutoDelay3 and AutonTimer.Get() < 3) {
			AutoDelayActive = true;
		} else if (autoDelay == IO.DS.sAutoDelay5 and AutonTimer.Get() < 5) {
			AutoDelayActive = true;
		} else if (AutoDelayActive) {
			AutoDelayActive = false;
			autoDelay = IO.DS.sAutoDelayOff;
			AutonTimer.Reset();
		}

		if (autoSelected == IO.DS.AutoLeftSpot and NearSwitch == caseLeft
				and !AutoDelayActive) {
			AutoLeftSwitchLeft();
		} else if (autoSelected == IO.DS.AutoLeftSpot
				and NearSwitch == caseRight and !AutoDelayActive) {
			AutoLeftSwitchRight();
		} else if (autoSelected == IO.DS.AutoCenterSpot and !AutoDelayActive)
			AutoCenter();
		else if (autoSelected == IO.DS.AutoRightSpot and NearSwitch == caseLeft
				and !AutoDelayActive)
			AutoRightSwitchLeft();
		else if (autoSelected == IO.DS.AutoRightSpot and NearSwitch == caseRight
				and !AutoDelayActive)
			AutoRightSwitchRight();

	}

	void AutoLeftSwitchLeft(void) {

		switch (modeState) {
		case 1:
			if (timedDrive(1, 0.5, 0.5)) {
				modeState = 2;
				AutonTimer.Reset();
			}
			break;
		case 2:
			if (timedDrive(1, -0.5, -0.5)) {
				modeState = 3;
				AutonTimer.Reset();
			}
			break;
		case 3:
			AutonTimer.Reset();
			AutonTimer.Stop();
			stopMotors();
			break;
		default:
			stopMotors();
		}
		return;
	}

	void AutoLeftSwitchRight(void) {
		switch (modeState) {
		case 1:
			if (timedDrive(1, -0.5, -0.5)) {
				modeState = 2;
				AutonTimer.Reset();
			}
			break;
		case 2:
			if (timedDrive(1, 0.5, 0.5)) {
				modeState = 3;
				AutonTimer.Reset();
			}
			break;
		case 3:
			AutonTimer.Reset();
			AutonTimer.Stop();
			stopMotors();
			break;
		default:
			stopMotors();

		}
		return;
	}

	void AutoRightSwitchLeft(void) {

		switch (modeState) {
		case 1:
			stopMotors();
			break;
		case 2:
			stopMotors();
			break;
		default:
			stopMotors();
		}

	}

	void AutoRightSwitchRight(void) {

		switch (modeState) {
		case 1:
			stopMotors();
			break;
		case 2:
			stopMotors();
			break;
		default:
			stopMotors();
		}
	}

	void AutoCenter(void) {

		if (NearSwitch == caseLeft) {
			switch (modeState) {
			case 1:
				if (timedDrive(1.0, 0.5, 0.5)) {
					modeState = 2;
					AutonTimer.Reset();
				}
				break;
			case 2:
				if (autonTurn(90)) {
					modeState = 3;
					AutonTimer.Reset();
				}
				break;
			case 3:
				if (timedDrive(0.5, 0.5, 0.5)) {
					modeState = 4;
					AutonTimer.Reset();
				}
				break;
			case 4:
				if (autonTurn(0)) {
					modeState = 5;
					AutonTimer.Reset();
				}
				break;
			case 5:
				if (timedDrive(0.5, 0.5, 0.5)) {
					AutonTimer.Reset();
					modeState = 6;
				}
				break;
			case 6:
				AutonTimer.Reset();
				AutonTimer.Stop();
				stopMotors();
				break;
			default:
				stopMotors();
			}
		} else if (NearSwitch == caseRight) {
			switch (modeState) {
			case 1:
				if (timedDrive(1, 0.5, 0.5)) {
					modeState = 2;
					AutonTimer.Reset();
				}
				break;
			case 2:
				if (autonTurn(-90)) {
					modeState = 3;
					AutonTimer.Reset();
				}
				break;
			case 3:
				if (timedDrive(0.5, 0.5, 0.5)) {
					modeState = 4;
					AutonTimer.Reset();
				}
				break;
			case 4:
				if (autonTurn(0)) {
					modeState = 5;
					AutonTimer.Reset();
				}
				break;
			case 5:
				if (timedDrive(0.5, 0.5, 0.5)) {
					AutonTimer.Reset();
					modeState = 6;
				}
				break;
			case 6:
				AutonTimer.Reset();
				AutonTimer.Stop();
				stopMotors();
				break;
			default:
				stopMotors();

			}
		}
		return;
	}

#define AB1_INIT 1
#define AB1_FWD 2
#define	AB1_TURN90 3
#define AB1_FWD2 4
#define AB1_SCORE 5
#define AB1_BACK 6
#define AB1_END 7
	void autoBlue1(void) {
		//blue side code
		//Starts from the center and drives to put the cube in the switch
		switch (modeState) {
		case AB1_INIT:
			// This uses state 1 for initialization.
			// This keeps the initialization and the code all in one place.
			IO.DriveBase.ahrs.ZeroYaw();
			modeState = AB1_FWD;
			break;
		case AB1_FWD:
			// Drives forward off the wall to perform the turn
			// TODO: adjust value
			if (forward(71.0)) {
				AutonTimer.Reset();
				modeState = AB1_TURN90;
			}
			break;
		case AB1_TURN90:
			// Turns 90 in the direction of the switch goal
			if (autonTurn(90)) {
				AutonTimer.Reset();
				modeState = AB1_FWD2;
			}
			break;
		case AB1_FWD2:
			// Drives forward to the switch goal
			// TODO: adjust value
			// TODO: should also be adjusting elevator height and claw location during this move
			if (forward(71.0)) {
				modeState = AB1_SCORE;
			}
			break;
		case AB1_SCORE:
			// Opens the claw to drop the pre-loaded cube
			// TODO: confirm directionality
			// TODO: consider separate function for clamp opening to coordinate wheel motion
			if (1) {
				IO.DriveBase.ClawClamp.Set(DoubleSolenoid::Value::kForward);
				modeState = AB1_BACK;
			}
			break;
		case AB1_BACK:
			if (timedDrive(5.0, 0.3, 0.3)) {
				AutonTimer.Reset();
				modeState = AB1_END;
			}
			break;

		default:
			stopMotors();
		}
		return;
	}

	void motorSpeed(double leftMotor, double rightMotor) {
		IO.DriveBase.MotorsLeft.Set(leftMotor);
		IO.DriveBase.MotorsRight.Set(rightMotor);
	}

	void elevatorSpeed(double elevMotor) {
		IO.DriveBase.Elevator1.Set(elevMotor);
		IO.DriveBase.Elevator2.Set(elevMotor);
	}

	bool elevatorHome(void) {

		bool SwitchElevHomeLower = IO.DriveBase.SwitchElevatorLower.Get();


		if  (SwitchElevHomeLower == true) {
			elevatorSpeed(-0.45);
			NotHome = true;
			return false;
		} else if ((NotHome == true) and (SwitchElevHomeLower == false)) {
			elevatorSpeed(0);
			IO.DriveBase.EncoderElevator.Reset();  // Reset encoder to 0
			NotHome = false;
			return true;
		}
		return false;
	}

#define Elevator_MAXSpeed (1)
#define Elevator_KP (0.008)
#define Elevator_KI (0.0004)
#define ElevatorHoldSpeed (0.05) // victor in brake mode
#define ElevatorPositionTol (3)
#define ElevatorLow (0)
#define ElevatorHigh (7950)
#define ElevatorITol (20)
	bool elevatorPosition(double Elev_position, bool override) {
		//TODO: Function to set elevator to a position with an override to disable it
		bool ElevatorUpperLimit = IO.DriveBase.SwitchElevatorUpper.Get();
		bool ElevatorLowerLimit = IO.DriveBase.SwitchElevatorLower.Get();

		double ElevEncoderRead = IO.DriveBase.EncoderElevator.Get();
		double ElevError = ElevEncoderRead - Elev_position;
		double ElevPro = ElevError * -Elevator_KP ; // P term
		if (fabs(ElevError) < ElevatorPositionTol) {
			ElevIError = 0;
		} else if ((fabs(ElevError) < ElevatorITol) and (fabs(ElevError) > ElevatorPositionTol)) {
			ElevIError = ElevIError + ElevError;
	//	} else if (fabs(ElevError) < ElevatorITol)  {
	//		ElevIError = ElevIError + ElevError;
		} else {
			ElevIError = 0;
		}

	//	double ElevInt = ElevIError * -Elevator_KI;  // I term
		double ElevInt = 0;    // Use to Test P term with no I term

		if (ElevInt > ElevDeadband){
			ElevInt = ElevDeadband;		//Set Max positive I term Max to min speed to move
			}
		else if (ElevInt < -(ElevDeadband)){
			ElevInt = (0);    //Set Max negative I term Max to min speed to move
			}
		double ElevCmd = ElevPro + ElevInt;   // Motor Output = P term + I term
		//Limit Elevator to Max positive and negative speeds
		if (ElevCmd > Elevator_MAXSpeed) { //If Positive speed > Max Positive speed
			ElevCmd = Elevator_MAXSpeed;    //Set to Max Positive speed
		} else if (ElevCmd < -Elevator_MAXSpeed) { ///If Negative speed < Max negative speed
			ElevCmd = -Elevator_MAXSpeed; ///Set to Max Negative speed
		}

		SmartDashboard::PutNumber("ElevInt", ElevInt);
		SmartDashboard::PutNumber("ElevCmd", ElevCmd);
		SmartDashboard::PutNumber("ElevError", ElevError);
		SmartDashboard::PutNumber("ElevIError", ElevIError);
		SmartDashboard::PutNumber("Elev_position", Elev_position);


		if (!ElevatorUpperLimit and Elev_position>ElevatorHigh) {
			elevatorSpeed(ElevatorHoldSpeed); // replace with routine to hold  top elevator position.
			ElevIError=0;
			return true;
		} else if (!ElevatorLowerLimit and Elev_position<ElevatorLow) {
			elevatorSpeed(0); // replace with routine to hold  bottom elevator position.
			ElevIError=0;
			IO.DriveBase.EncoderElevator.Reset(); // Reset encoder to 0
			return true;
		} else if (fabs(ElevError) <= ElevatorPositionTol) {
			//elevatorSpeed(ElevatorHoldSpeed);
			//elevatorSpeed(0);
			ElevIError=0;
			return true;
		}  else
			elevatorSpeed(ElevCmd);
		return false;
	}


#define Wrist_MaxSpeed (1)
#define Wrist_Idle (.4)


bool wristPosition(int position){
// Controls the wrist position.
// for now it will send the wrist to position 1 or position 2 then return true when it is in that position
	int wristOutput;
	bool switchWrist1 = IO.DriveBase.SwitchWrist1.Get();
	bool switchWrist2 = IO.DriveBase.SwitchWrist1.Get();
	bool inCorrectPosition = false;
	switch(position){
		case 1:
			///if the wrist is in position 1
			if (switchWrist1 == true) {
				//it is in position 1
				wristOutput = Wrist_Idle;
				inCorrectPosition = true;
			}
			else {
				// it isn't in position 1
				wristOutput = 1;
				inCorrectPosition = false;
			}
			break;
		case 2:
			if (switchWrist2 == true){
				wristOutput = -Wrist_Idle;
				inCorrectPosition = true;
			}
			else {
				wristOutput = -1;
				inCorrectPosition = false;
			}
			break;
		case 0:
			inCorrectPosition = false;
			break;
		default:
			inCorrectPosition = false;
			break;
		}

		// Cap the speed to the maximum
		wristOutput = std::max(std::min(wristOutput, Wrist_MaxSpeed),-Wrist_MaxSpeed);

		IO.DriveBase.Wrist1.Set(wristOutput);

		return inCorrectPosition;


}

// Drivetrain functions

	int stopMotors() {
		//sets motor speeds to zero
		motorSpeed(0, 0);
		return 1;
	}

#define KP_LINEAR (0.27)
#define KP_ROTATION (0.017)
#define LINEAR_SETTLING_TIME (0.1)
#define LINEAR_MAX_DRIVE_SPEED (0.75)
#define ROTATIONAL_TOLERANCE (1.0)
#define ERROR_GAIN (-0.05)
#define ROTATIONAL_SETTLING_TIME (0.5)
#define MAX_DRIVE_TIME (0.5)
#define LINEAR_TOLERANCE (0.02)

	int forward(double targetDistance) {
		//put all encoder stuff in same place
		double encoderDistance;
		bool useRightEncoder = true;

		if (useRightEncoder)
			encoderDistance = IO.DriveBase.EncoderRight.GetDistance();
		else
			encoderDistance = IO.DriveBase.EncoderLeft.GetDistance();

		double encoderError = encoderDistance - targetDistance;
		double driveCommandLinear = encoderError * KP_LINEAR;

		//limits max drive speed
		if (driveCommandLinear > LINEAR_MAX_DRIVE_SPEED) {
			driveCommandLinear = LINEAR_MAX_DRIVE_SPEED;
		} else if (driveCommandLinear < -1 * LINEAR_MAX_DRIVE_SPEED) { /////***** "-1" is a "magic number." At least put a clear comment in here.
			driveCommandLinear = -1 * LINEAR_MAX_DRIVE_SPEED; /////***** same as above.
		}

		double gyroAngle = IO.DriveBase.ahrs.GetAngle();
		double driveCommandRotation = gyroAngle * KP_ROTATION;
		//calculates and sets motor speeds
		motorSpeed(driveCommandLinear + driveCommandRotation,
				driveCommandLinear - driveCommandRotation);

		//routine helps prevent the robot from overshooting the distance
		if (isWaiting == 0) { /////***** Rename "isWaiting."  This isWaiting overlaps with the autonTurn() isWaiting.  There is nothing like 2 globals that are used for different things, but have the same name.
			if (abs(encoderError) < LINEAR_TOLERANCE) {
				isWaiting = 1;
				AutonTimer.Reset();
			}
		}
		//timed wait
		else {
			float currentTime = AutonTimer.Get();
			if (abs(encoderError) > LINEAR_TOLERANCE) {
				isWaiting = 0;					/////***** Rename
			} else if (currentTime > LINEAR_SETTLING_TIME) {
				isWaiting = 0;					/////***** Rename
				return 1;
			}
		}
		return 0;
	}

	int autonTurn(float targetYaw) {

		float currentYaw = IO.DriveBase.ahrs.GetAngle();
		float yawError = currentYaw - targetYaw;

		motorSpeed(-1 * yawError * ERROR_GAIN, yawError * ERROR_GAIN);

		if (isWaiting == 0) {/////***** Rename "isWaiting."  This isWaiting overlaps with the forward() isWaiting.  There is nothing like 2 globals that are used for different things, but have the same name.
			if (abs(yawError) < ROTATIONAL_TOLERANCE) {
				isWaiting = 1;
				AutonTimer.Reset();
			}
		}
		//timed wait
		else {
			float currentTime = AutonTimer.Get();
			if (abs(yawError) > ROTATIONAL_TOLERANCE) {
				isWaiting = 0;
			} else if (currentTime > ROTATIONAL_SETTLING_TIME) {
				isWaiting = 0;
				return 1;
			}
		}
		return 0;
	}

	int timedDrive(double driveTime, double leftMotorSpeed,
			double rightMotorSpeed) {
		float currentTime = AutonTimer.Get();
		if (currentTime < driveTime) {
			motorSpeed(leftMotorSpeed, rightMotorSpeed);
		} else {
			stopMotors();
			return 1;
		}
		return 0;
	}

	frc::Relay::Value LEDcontrol(int LEDcontrolcode) {
		int relayoutput;
		for (int i = 7; i > -1; i = i - 1) {
			relayoutput = (LEDcontrolcode & 00000011);

			switch (relayoutput) {
			case 0:
				return frc::Relay::Value::kOff;
				break;
			case 1:
				return frc::Relay::Value::kReverse;
				break;
			case 2:
				return frc::Relay::Value::kForward;
				break;
			case 3:
				return frc::Relay::Value::kOn;
				break;
			default:
				return frc::Relay::Value::kOn;

			}

		}

		IO.DriveBase.LED0.Set(Relay::Value::kOn);
	}

	void SmartDashboardUpdate() {

		// Auto State
		SmartDashboard::PutNumber("Auto Switch (#)", AutoVal);
		SmartDashboard::PutString("Auto Program", autoSelected);
		SmartDashboard::PutNumber("Auto State (#)", modeState);
		SmartDashboard::PutNumber("Auto Timer (s)", AutonTimer.Get());

		// Drive Encoders
		SmartDashboard::PutNumber("Drive Encoder Left (RAW)",
				IO.DriveBase.EncoderLeft.GetRaw());
		SmartDashboard::PutNumber("Drive Encoder Left (Inches)",
				IO.DriveBase.EncoderLeft.GetDistance());

		SmartDashboard::PutNumber("Drive Encoder Right (RAW)",
				IO.DriveBase.EncoderRight.GetRaw());
		SmartDashboard::PutNumber("Drive Encoder Right (Inch)",
				IO.DriveBase.EncoderRight.GetDistance());

		// Elevator Encoders
		SmartDashboard::PutNumber("Elevator Encoder", IO.DriveBase.EncoderElevator.Get());


		// Gyro
		if (&IO.DriveBase.ahrs) {
			double gyroAngle = IO.DriveBase.ahrs.GetAngle();
			SmartDashboard::PutNumber("Gyro Angle", gyroAngle);
		} else {
			SmartDashboard::PutNumber("Gyro Angle", 999);
		}

	}

}
;

START_ROBOT_CLASS(Robot);
