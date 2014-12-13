#include "epos_hardware/epos_hardware.h"
#include <boost/foreach.hpp>

namespace epos_hardware {

Epos::Epos(XmlRpc::XmlRpcValue& config_xml, EposFactory* epos_factory)
  : config_xml_(config_xml), epos_factory_(epos_factory) {
  ROS_ASSERT(config_xml_.getType() == XmlRpc::XmlRpcValue::TypeStruct);

  ROS_ASSERT(config_xml_["name"].getType() == XmlRpc::XmlRpcValue::TypeString);
  name_ = static_cast<std::string>(config_xml_["name"]);

  ROS_ASSERT(config_xml_["serial_number"].getType() == XmlRpc::XmlRpcValue::TypeString);
  ROS_ASSERT(SerialNumberFromHex(static_cast<std::string>(config_xml_["serial_number"]), &serial_number_));

  ROS_ASSERT(config_xml_["operation_mode"].getType() == XmlRpc::XmlRpcValue::TypeInt);
  operation_mode_ = (OperationMode)static_cast<int>(config_xml_["operation_mode"]);
}
Epos::~Epos() {
  unsigned int error_code;
  VCS_SetDisableState(node_handle_->device_handle->ptr, node_handle_->node_id, &error_code);
}

bool Epos::init() {
  ROS_INFO_STREAM("Initializing: 0x" << std::hex << serial_number_);
  unsigned int error_code;
  node_handle_ = epos_factory_->CreateNodeHandle("EPOS2", "MAXON SERIAL V2", "USB", serial_number_, &error_code);
  if(!node_handle_)
    return false;
  ROS_INFO_STREAM("Found Motor");

  if(!VCS_SetProtocolStackSettings(node_handle_->device_handle->ptr, 1000000, 500, &error_code)){
    return false;
  }

  if(!VCS_SetOperationMode(node_handle_->device_handle->ptr, node_handle_->node_id, operation_mode_, &error_code))
    return false;

  {
    XmlRpc::XmlRpcValue& motor_xml = config_xml_["motor"];
    ROS_ASSERT(motor_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);

    ROS_ASSERT(motor_xml["type"].getType() == XmlRpc::XmlRpcValue::TypeInt);
    if(!VCS_SetMotorType(node_handle_->device_handle->ptr, node_handle_->node_id, static_cast<int>(motor_xml["type"]), &error_code))
      return false;

    if(motor_xml.hasMember("dc_motor")) {
      XmlRpc::XmlRpcValue& dc_motor_xml = motor_xml["dc_motor"];
      ROS_ASSERT(dc_motor_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(dc_motor_xml["nominal_current"].getType() == XmlRpc::XmlRpcValue::TypeDouble);
      ROS_ASSERT(dc_motor_xml["max_output_current"].getType() == XmlRpc::XmlRpcValue::TypeDouble);
      ROS_ASSERT(dc_motor_xml["thermal_time_constant"].getType() == XmlRpc::XmlRpcValue::TypeDouble);
      if(!VCS_SetDcMotorParameter(node_handle_->device_handle->ptr, node_handle_->node_id,
				  1000 * static_cast<double>(dc_motor_xml["nominal_current"]), // A -> mA
				  1000 * static_cast<double>(dc_motor_xml["max_output_current"]), // A -> mA
				  10 * static_cast<double>(dc_motor_xml["thermal_time_constant"]), // s -> 100ms
				  &error_code))
	return false;
    }

    if(motor_xml.hasMember("ec_motor")) {
      XmlRpc::XmlRpcValue& ec_motor_xml = motor_xml["ec_motor"];
      ROS_ASSERT(ec_motor_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(ec_motor_xml["nominal_current"].getType() == XmlRpc::XmlRpcValue::TypeDouble);
      ROS_ASSERT(ec_motor_xml["max_output_current"].getType() == XmlRpc::XmlRpcValue::TypeDouble);
      ROS_ASSERT(ec_motor_xml["thermal_time_constant"].getType() == XmlRpc::XmlRpcValue::TypeDouble);
      ROS_ASSERT(ec_motor_xml["number_of_pole_pairs"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetEcMotorParameter(node_handle_->device_handle->ptr, node_handle_->node_id,
				  1000 * static_cast<double>(ec_motor_xml["nominal_current"]), // A -> mA
				  1000 * static_cast<double>(ec_motor_xml["max_output_current"]), // A -> mA
				  10 * static_cast<double>(ec_motor_xml["thermal_time_constant"]), // s -> 100ms
				  static_cast<int>(ec_motor_xml["number_of_pole_pairs"]),
				  &error_code))
	return false;
    }
  }

  {
    XmlRpc::XmlRpcValue& sensor_xml = config_xml_["sensor"];
    ROS_ASSERT(sensor_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);

    ROS_ASSERT(sensor_xml["type"].getType() == XmlRpc::XmlRpcValue::TypeInt);
    if(!VCS_SetSensorType(node_handle_->device_handle->ptr, node_handle_->node_id, static_cast<int>(sensor_xml["type"]), &error_code))
      return false;

    if(sensor_xml.hasMember("incremental_encoder")) {
      XmlRpc::XmlRpcValue& inc_encoder_xml = sensor_xml["incremental_encoder"];
      ROS_ASSERT(inc_encoder_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(inc_encoder_xml["resolution"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(inc_encoder_xml["inverted_polarity"].getType() == XmlRpc::XmlRpcValue::TypeBoolean);
      if(!VCS_SetIncEncoderParameter(node_handle_->device_handle->ptr, node_handle_->node_id,
				     static_cast<int>(inc_encoder_xml["resolution"]),
				     static_cast<bool>(inc_encoder_xml["inverted_polarity"]),
				     &error_code))
	return false;
    }

    if(sensor_xml.hasMember("hall_sensor")) {
      XmlRpc::XmlRpcValue& hall_sensor_xml = sensor_xml["hall_sensor"];
      ROS_ASSERT(hall_sensor_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(hall_sensor_xml["inverted_polarity"].getType() == XmlRpc::XmlRpcValue::TypeBoolean);
      if(!VCS_SetHallSensorParameter(node_handle_->device_handle->ptr, node_handle_->node_id,
				     static_cast<bool>(hall_sensor_xml["inverted_polarity"]),
				     &error_code))
	return false;
    }

    if(sensor_xml.hasMember("ssi_absolute_encoder")) {
      XmlRpc::XmlRpcValue& ssi_absolute_encoder_xml = sensor_xml["ssi_absolute_encoder"];
      ROS_ASSERT(ssi_absolute_encoder_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(ssi_absolute_encoder_xml["data_rate"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(ssi_absolute_encoder_xml["number_of_multiturn_bits"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(ssi_absolute_encoder_xml["number_of_singleturn_bits"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(ssi_absolute_encoder_xml["inverted_polarity"].getType() == XmlRpc::XmlRpcValue::TypeBoolean);
      if(!VCS_SetSsiAbsEncoderParameter(node_handle_->device_handle->ptr, node_handle_->node_id,
					static_cast<int>(ssi_absolute_encoder_xml["data_rate"]),
					static_cast<int>(ssi_absolute_encoder_xml["number_of_multiturn_bits"]),
					static_cast<int>(ssi_absolute_encoder_xml["number_of_singleturn_bits"]),
					static_cast<bool>(ssi_absolute_encoder_xml["inverted_polarity"]),
					&error_code))
	return false;
    }

  }

  if(config_xml_.hasMember("safety")) {
    XmlRpc::XmlRpcValue& safety_xml = config_xml_["safety"];
    ROS_ASSERT(safety_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
    if(safety_xml.hasMember("max_following_error")) {
      ROS_ASSERT(safety_xml["max_following_error"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetMaxFollowingError(node_handle_->device_handle->ptr, node_handle_->node_id,
				  static_cast<int>(safety_xml["max_following_error"]),
				  &error_code))
	return false;
    }

    if(safety_xml.hasMember("max_profile_velocity")) {
      ROS_ASSERT(safety_xml["max_profile_velocity"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetMaxProfileVelocity(node_handle_->device_handle->ptr, node_handle_->node_id,
				  static_cast<int>(safety_xml["max_profile_velocity"]),
				  &error_code))
	return false;
    }

    if(safety_xml.hasMember("max_acceleration")) {
      ROS_ASSERT(safety_xml["max_acceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetMaxAcceleration(node_handle_->device_handle->ptr, node_handle_->node_id,
				  static_cast<int>(safety_xml["max_acceleration"]),
				  &error_code))
	return false;
    }
  }

  if(config_xml_.hasMember("position_regulator")) {
    XmlRpc::XmlRpcValue& position_xml = config_xml_["position_regulator"];
    ROS_ASSERT(position_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
    if(position_xml.hasMember("gain")) {
      XmlRpc::XmlRpcValue& gain_xml = position_xml["gain"];
      ROS_ASSERT(gain_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(gain_xml["p"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(gain_xml["i"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(gain_xml["d"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetPositionRegulatorGain(node_handle_->device_handle->ptr, node_handle_->node_id,
				       static_cast<int>(gain_xml["p"]),
				       static_cast<int>(gain_xml["i"]),
				       static_cast<int>(gain_xml["d"]),
				       &error_code))
	return false;
    }

    if(position_xml.hasMember("feed_forward")) {
      XmlRpc::XmlRpcValue& feed_forward_xml = position_xml["feed_forward"];
      ROS_ASSERT(feed_forward_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(feed_forward_xml["velocity"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(feed_forward_xml["acceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetPositionRegulatorFeedForward(node_handle_->device_handle->ptr, node_handle_->node_id,
					      static_cast<int>(feed_forward_xml["velocity"]),
					      static_cast<int>(feed_forward_xml["acceleration"]),
					      &error_code))
	return false;
    }

  }

  if(config_xml_.hasMember("velocity_regulator")) {
    XmlRpc::XmlRpcValue& velocity_xml = config_xml_["velocity_regulator"];
    ROS_ASSERT(velocity_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
    if(velocity_xml.hasMember("gain")) {
      XmlRpc::XmlRpcValue& gain_xml = velocity_xml["gain"];
      ROS_ASSERT(gain_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(gain_xml["p"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(gain_xml["i"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetVelocityRegulatorGain(node_handle_->device_handle->ptr, node_handle_->node_id,
				       static_cast<int>(gain_xml["p"]),
				       static_cast<int>(gain_xml["i"]),
				       &error_code))
	return false;
    }

    if(velocity_xml.hasMember("feed_forward")) {
      XmlRpc::XmlRpcValue& feed_forward_xml = velocity_xml["feed_forward"];
      ROS_ASSERT(feed_forward_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(feed_forward_xml["velocity"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(feed_forward_xml["acceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetVelocityRegulatorFeedForward(node_handle_->device_handle->ptr, node_handle_->node_id,
					      static_cast<int>(feed_forward_xml["velocity"]),
					      static_cast<int>(feed_forward_xml["acceleration"]),
					      &error_code))
	return false;
    }

  }


  if(config_xml_.hasMember("velocity_regulator")) {
    XmlRpc::XmlRpcValue& velocity_xml = config_xml_["velocity_regulator"];
    ROS_ASSERT(velocity_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
    if(velocity_xml.hasMember("gain")) {
      XmlRpc::XmlRpcValue& gain_xml = velocity_xml["gain"];
      ROS_ASSERT(gain_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(gain_xml["p"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(gain_xml["i"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetVelocityRegulatorGain(node_handle_->device_handle->ptr, node_handle_->node_id,
				       static_cast<int>(gain_xml["p"]),
				       static_cast<int>(gain_xml["i"]),
				       &error_code))
	return false;
    }

    if(velocity_xml.hasMember("feed_forward")) {
      XmlRpc::XmlRpcValue& feed_forward_xml = velocity_xml["feed_forward"];
      ROS_ASSERT(feed_forward_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(feed_forward_xml["velocity"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(feed_forward_xml["acceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetVelocityRegulatorFeedForward(node_handle_->device_handle->ptr, node_handle_->node_id,
					      static_cast<int>(feed_forward_xml["velocity"]),
					      static_cast<int>(feed_forward_xml["acceleration"]),
					      &error_code))
	return false;
    }

  }


  if(config_xml_.hasMember("velocity_regulator")) {
    XmlRpc::XmlRpcValue& velocity_xml = config_xml_["velocity_regulator"];
    ROS_ASSERT(velocity_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
    if(velocity_xml.hasMember("gain")) {
      XmlRpc::XmlRpcValue& gain_xml = velocity_xml["gain"];
      ROS_ASSERT(gain_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(gain_xml["p"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(gain_xml["i"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetVelocityRegulatorGain(node_handle_->device_handle->ptr, node_handle_->node_id,
				       static_cast<int>(gain_xml["p"]),
				       static_cast<int>(gain_xml["i"]),
				       &error_code))
	return false;
    }

    if(velocity_xml.hasMember("feed_forward")) {
      XmlRpc::XmlRpcValue& feed_forward_xml = velocity_xml["feed_forward"];
      ROS_ASSERT(feed_forward_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(feed_forward_xml["velocity"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(feed_forward_xml["acceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetVelocityRegulatorFeedForward(node_handle_->device_handle->ptr, node_handle_->node_id,
					      static_cast<int>(feed_forward_xml["velocity"]),
					      static_cast<int>(feed_forward_xml["acceleration"]),
					      &error_code))
	return false;
    }

  }

  if(config_xml_.hasMember("current_regulator")) {
    XmlRpc::XmlRpcValue& current_xml = config_xml_["current_regulator"];
    ROS_ASSERT(current_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
    if(current_xml.hasMember("gain")) {
      XmlRpc::XmlRpcValue& gain_xml = current_xml["gain"];
      ROS_ASSERT(gain_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(gain_xml["p"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(gain_xml["i"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      if(!VCS_SetCurrentRegulatorGain(node_handle_->device_handle->ptr, node_handle_->node_id,
				      static_cast<int>(gain_xml["p"]),
				      static_cast<int>(gain_xml["i"]),
				      &error_code))
	return false;
    }
  }

  if(config_xml_.hasMember("position_profile")) {
    XmlRpc::XmlRpcValue& position_xml = config_xml_["position_profile"];
    ROS_ASSERT(position_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
    ROS_ASSERT(position_xml["velocity"].getType() == XmlRpc::XmlRpcValue::TypeInt);
    ROS_ASSERT(position_xml["acceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
    ROS_ASSERT(position_xml["deceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
    if(!VCS_SetPositionProfile(node_handle_->device_handle->ptr, node_handle_->node_id,
			       static_cast<int>(position_xml["velocity"]),
			       static_cast<int>(position_xml["acceleration"]),
			       static_cast<int>(position_xml["deceleration"]),
			       &error_code))
      return false;
    if(position_xml.hasMember("window")) {
      XmlRpc::XmlRpcValue& window_xml = position_xml["window"];
      ROS_ASSERT(window_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(window_xml["window"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(window_xml["time"].getType() == XmlRpc::XmlRpcValue::TypeDouble);
      if(!VCS_EnablePositionWindow(node_handle_->device_handle->ptr, node_handle_->node_id,
				   static_cast<int>(window_xml["window"]),
				   1000 * static_cast<double>(window_xml["time"]), // s -> ms
				   &error_code))
	return false;
    }
  }



  if(config_xml_.hasMember("velocity_profile")) {
    XmlRpc::XmlRpcValue& velocity_xml = config_xml_["velocity_profile"];
    ROS_ASSERT(velocity_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
    ROS_ASSERT(velocity_xml["acceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
    ROS_ASSERT(velocity_xml["deceleration"].getType() == XmlRpc::XmlRpcValue::TypeInt);
    if(!VCS_SetVelocityProfile(node_handle_->device_handle->ptr, node_handle_->node_id,
				    static_cast<int>(velocity_xml["acceleration"]),
				    static_cast<int>(velocity_xml["deceleration"]),
				    &error_code))
      return false;
    if(velocity_xml.hasMember("window")) {
      XmlRpc::XmlRpcValue& window_xml = velocity_xml["window"];
      ROS_ASSERT(window_xml.getType() == XmlRpc::XmlRpcValue::TypeStruct);
      ROS_ASSERT(window_xml["window"].getType() == XmlRpc::XmlRpcValue::TypeInt);
      ROS_ASSERT(window_xml["time"].getType() == XmlRpc::XmlRpcValue::TypeDouble);
      if(!VCS_EnableVelocityWindow(node_handle_->device_handle->ptr, node_handle_->node_id,
				   static_cast<int>(window_xml["window"]),
				   1000 * static_cast<double>(window_xml["time"]), // s -> ms
				   &error_code))
	return false;
    }
  }



  unsigned char num_errors;
  if(!VCS_GetNbOfDeviceError(node_handle_->device_handle->ptr, node_handle_->node_id, &num_errors, &error_code))
    return false;
  for(int i = 1; i<= num_errors; ++i) {
    unsigned int error_number;
    if(!VCS_GetDeviceErrorCode(node_handle_->device_handle->ptr, node_handle_->node_id, i, &error_number, &error_code))
      return false;
    ROS_WARN_STREAM("EPOS Device Error: 0x" << std::hex << error_number);
  }

  if(config_xml_.hasMember("clear_faults")) {
    ROS_ASSERT(config_xml_["clear_faults"].getType() == XmlRpc::XmlRpcValue::TypeBoolean);
    if(static_cast<bool>(config_xml_["clear_faults"])){
      if(!VCS_ClearFault(node_handle_->device_handle->ptr, node_handle_->node_id, &error_code))
	return false;
      else
	ROS_INFO_STREAM("Cleared faults");
    }
  }

  ROS_INFO_STREAM("Enabling Motor");
  if(!VCS_SetEnableState(node_handle_->device_handle->ptr, node_handle_->node_id, &error_code))
    return false;

  return true;
}
void Epos::read() {
  unsigned int error_code;
  int position;
  int velocity;
  short current;
  VCS_GetPositionIs(node_handle_->device_handle->ptr, node_handle_->node_id, &position, &error_code);
  VCS_GetVelocityIs(node_handle_->device_handle->ptr, node_handle_->node_id, &velocity, &error_code);
  VCS_GetCurrentIs(node_handle_->device_handle->ptr, node_handle_->node_id, &current, &error_code);
  //ROS_INFO("Motor: %d, %d, %d", position, velocity, current);
}

void Epos::write() {
  unsigned int error_code;
  VCS_MoveWithVelocity(node_handle_->device_handle->ptr, node_handle_->node_id, 100, &error_code);
}

EposHardware::EposHardware(ros::NodeHandle& nh, ros::NodeHandle& pnh) {
  XmlRpc::XmlRpcValue motors_xml;
  if(pnh.getParam("motors", motors_xml)) {
    ROS_ASSERT(motors_xml.getType() == XmlRpc::XmlRpcValue::TypeArray);

    for (int32_t i = 0; i < motors_xml.size(); ++i) {
      XmlRpc::XmlRpcValue motor_xml = motors_xml[i];
      boost::shared_ptr<Epos> motor(new Epos(motor_xml, &epos_factory));
      motors.push_back(motor);
    }
  }
  else {
    ROS_FATAL("No motors defined");
  }
}

void EposHardware::init() {
  BOOST_FOREACH(const boost::shared_ptr<Epos>& motor, motors) {
    if(!motor->init())
      ROS_ERROR("Could not configure motor");
  }
}

void EposHardware::read() {
  BOOST_FOREACH(const boost::shared_ptr<Epos>& motor, motors) {
    motor->read();
  }
}

void EposHardware::write() {
  BOOST_FOREACH(const boost::shared_ptr<Epos>& motor, motors) {
    motor->write();
  }
}

}
