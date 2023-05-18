// Copyright 2022 PickNik Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the PickNik Inc. nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/* Author    : Andy Zelenak
   Desc      : Free functions. We keep them in a separate translation unit to reduce .o filesize
   Title     : utilities.cpp
   Project   : moveit_servo
*/

#include <moveit_servo/utilities.h>

// Disable -Wold-style-cast because all _THROTTLE macros trigger this
// It would be too noisy to disable on a per-callsite basis
#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace moveit_servo
{
namespace
{
static const rclcpp::Logger LOGGER = rclcpp::get_logger("moveit_servo.utilities");
constexpr auto ROS_LOG_THROTTLE_PERIOD = std::chrono::milliseconds(3000).count();
}  // namespace

/** \brief Helper function for converting Eigen::Isometry3d to geometry_msgs/TransformStamped **/
geometry_msgs::msg::TransformStamped convertIsometryToTransform(const Eigen::Isometry3d& eigen_tf,
                                                                const std::string& parent_frame,
                                                                const std::string& child_frame)
{
  geometry_msgs::msg::TransformStamped output = tf2::eigenToTransform(eigen_tf);
  output.header.frame_id = parent_frame;
  output.child_frame_id = child_frame;

  return output;
}

/** \brief Possibly calculate a velocity scaling factor, due to proximity of
 * singularity and direction of motion
 * @param[in] joint_model_group   The MoveIt group
 * @param[in] commanded_twist     The commanded Cartesian twist
 * @param[in] svd                 A singular value decomposition of the Jacobian
 * @param[in] pseudo_inverse      The pseudo-inverse of the Jacobian
 * @param[in] hard_stop_singularity_threshold  Halt if condition(Jacobian) > hard_stop_singularity_threshold
 * @param[in] lower_singularity_threshold      Decelerate if condition(Jacobian) > lower_singularity_threshold
 * @param[in] leaving_singularity_threshold_multiplier      Allow faster motion away from singularity
 * @param[in, out] clock          A ROS clock, for logging
 * @param[in, out] current_state  The state of the robot. Used in internal calculations.
 * @param[out] status             Singularity status
 */
double velocityScalingFactorForSingularity(const moveit::core::JointModelGroup* joint_model_group,
                                           const Eigen::VectorXd& commanded_twist,
                                           const Eigen::JacobiSVD<Eigen::MatrixXd>& svd,
                                           const Eigen::MatrixXd& pseudo_inverse,
                                           const double hard_stop_singularity_threshold,
                                           const double lower_singularity_threshold,
                                           const double leaving_singularity_threshold_multiplier, rclcpp::Clock& clock,
                                           const moveit::core::RobotStatePtr& current_state, StatusCode& status)
{
  double velocity_scale = 1;
  std::size_t num_dimensions = commanded_twist.size();

  // Find the direction away from nearest singularity.
  // The last column of U from the SVD of the Jacobian points directly toward or away from the singularity.
  // The sign can flip at any time, so we have to do some extra checking.
  // Look ahead to see if the Jacobian's condition will decrease.
  Eigen::VectorXd vector_toward_singularity = svd.matrixU().col(num_dimensions - 1);

  double ini_condition = svd.singularValues()(0) / svd.singularValues()(svd.singularValues().size() - 1);

  // This singular vector tends to flip direction unpredictably. See R. Bro,
  // "Resolving the Sign Ambiguity in the Singular Value Decomposition".
  // Look ahead to see if the Jacobian's condition will decrease in this
  // direction. Start with a scaled version of the singular vector
  Eigen::VectorXd delta_x(num_dimensions);
  double scale = 100;
  delta_x = vector_toward_singularity / scale;

  // Calculate a small change in joints
  Eigen::VectorXd new_theta;
  current_state->copyJointGroupPositions(joint_model_group, new_theta);
  new_theta += pseudo_inverse * delta_x;
  current_state->setJointGroupPositions(joint_model_group, new_theta);
  Eigen::MatrixXd new_jacobian = current_state->getJacobian(joint_model_group);

  Eigen::JacobiSVD<Eigen::MatrixXd> new_svd(new_jacobian);
  double new_condition = new_svd.singularValues()(0) / new_svd.singularValues()(new_svd.singularValues().size() - 1);
  // If new_condition < ini_condition, the singular vector does point towards a
  // singularity. Otherwise, flip its direction.
  if (ini_condition >= new_condition)
  {
    vector_toward_singularity *= -1;
  }

  // If this dot product is positive, we're moving toward singularity
  double dot = vector_toward_singularity.dot(commanded_twist);
  // see https://github.com/ros-planning/moveit2/pull/620#issuecomment-1201418258 for visual explanation of algorithm
  double upper_threshold = dot > 0 ? hard_stop_singularity_threshold :
                                     (hard_stop_singularity_threshold - lower_singularity_threshold) *
                                             leaving_singularity_threshold_multiplier +
                                         lower_singularity_threshold;
  if ((ini_condition > lower_singularity_threshold) && (ini_condition < hard_stop_singularity_threshold))
  {
    velocity_scale =
        1. - (ini_condition - lower_singularity_threshold) / (upper_threshold - lower_singularity_threshold);
    status =
        dot > 0 ? StatusCode::DECELERATE_FOR_APPROACHING_SINGULARITY : StatusCode::DECELERATE_FOR_LEAVING_SINGULARITY;
    RCLCPP_WARN_STREAM_THROTTLE(LOGGER, clock, ROS_LOG_THROTTLE_PERIOD, SERVO_STATUS_CODE_MAP.at(status));
  }

  // Very close to singularity, so halt.
  else if (ini_condition >= upper_threshold)
  {
    velocity_scale = 0;
    status = StatusCode::HALT_FOR_SINGULARITY;
    RCLCPP_WARN_STREAM_THROTTLE(LOGGER, clock, ROS_LOG_THROTTLE_PERIOD, SERVO_STATUS_CODE_MAP.at(status));
  }

  return velocity_scale;
}

bool applyJointUpdate(rclcpp::Clock& clock, const double publish_period, const Eigen::ArrayXd& delta_theta,
                      const sensor_msgs::msg::JointState& previous_joint_state,
                      sensor_msgs::msg::JointState& next_joint_state,
                      pluginlib::UniquePtr<online_signal_smoothing::SmoothingBaseClass>& smoother)
{
  // All the sizes must match
  if (next_joint_state.position.size() != static_cast<std::size_t>(delta_theta.size()) ||
      next_joint_state.velocity.size() != next_joint_state.position.size())
  {
    RCLCPP_ERROR_STREAM_THROTTLE(LOGGER, clock, ROS_LOG_THROTTLE_PERIOD,
                                 "Lengths of output and increments do not match.");
    return false;
  }

  for (std::size_t i = 0; i < next_joint_state.position.size(); ++i)
  {
    // Increment joint
    next_joint_state.position[i] += delta_theta[i];
  }

  smoother->doSmoothing(next_joint_state.position);

  // Lambda that calculates velocity using central difference.
  // (q(t + dt) - q(t - dt)) / ( 2 * dt )
  auto compute_velocity = [&](const double next_pos, const double previous_pos) {
    return (next_pos - previous_pos) / (2 * publish_period);
  };

  // Transform that applies the lambda to all joints.
  // next_joint_state contains the future position q(t + dt)
  // previous_joint_state_ contains past position q(t - dt)
  std::transform(next_joint_state.position.begin(), next_joint_state.position.end(),
                 previous_joint_state.position.begin(), next_joint_state.velocity.begin(), compute_velocity);

  return true;
}

void transformTwistToPlanningFrame(geometry_msgs::msg::TwistStamped& cmd, const std::string& planning_frame,
                                   const moveit::core::RobotStatePtr& current_state, rclcpp::Clock& clock)
{
  if (cmd.header.frame_id.empty())
  {
    RCLCPP_WARN_STREAM_THROTTLE(LOGGER, clock, ROS_LOG_THROTTLE_PERIOD,
                                "No frame specified for command ,will use planning_frame");
    cmd.header.frame_id = planning_frame;
  }

  // We solve (planning_frame -> base -> cmd.header.frame_id)
  // by computing (base->planning_frame)^-1 * (base->cmd.header.frame_id)
  const Eigen::Isometry3d tf_moveit_to_incoming_cmd_frame =
      current_state->getGlobalLinkTransform(planning_frame).inverse() *
      current_state->getGlobalLinkTransform(cmd.header.frame_id);

  // Apply the transform to linear and angular velocities
  // v' = R * v  and w' = R * w
  Eigen::Vector3d translation_vector(cmd.twist.linear.x, cmd.twist.linear.y, cmd.twist.linear.z);
  Eigen::Vector3d angular_vector(cmd.twist.angular.x, cmd.twist.angular.y, cmd.twist.angular.z);
  translation_vector = tf_moveit_to_incoming_cmd_frame.linear() * translation_vector;
  angular_vector = tf_moveit_to_incoming_cmd_frame.linear() * angular_vector;

  // Update the values of the original command message to reflect the change in frame
  cmd.header.frame_id = planning_frame;
  cmd.twist.linear.x = translation_vector(0);
  cmd.twist.linear.y = translation_vector(1);
  cmd.twist.linear.z = translation_vector(2);
  cmd.twist.angular.x = angular_vector(0);
  cmd.twist.angular.y = angular_vector(1);
  cmd.twist.angular.z = angular_vector(2);
}

geometry_msgs::msg::Pose poseFromCartesianDelta(const Eigen::VectorXd& delta_x,
                                                const Eigen::Isometry3d& base_to_tip_frame_transform)
{
  // get a transformation matrix with the desired position change &
  // get a transformation matrix with desired orientation change
  Eigen::Isometry3d tf_pos_delta(Eigen::Isometry3d::Identity());
  tf_pos_delta.translate(Eigen::Vector3d(delta_x[0], delta_x[1], delta_x[2]));

  Eigen::Isometry3d tf_rot_delta(Eigen::Isometry3d::Identity());
  Eigen::Quaterniond q = Eigen::AngleAxisd(delta_x[3], Eigen::Vector3d::UnitX()) *
                         Eigen::AngleAxisd(delta_x[4], Eigen::Vector3d::UnitY()) *
                         Eigen::AngleAxisd(delta_x[5], Eigen::Vector3d::UnitZ());
  tf_rot_delta.rotate(q);

  // Find the new tip link position without newly applied rotation
  const Eigen::Isometry3d tf_no_new_rot = tf_pos_delta * base_to_tip_frame_transform;

  // we want the rotation to be applied in the requested reference frame,
  // but we want the rotation to be about the EE point in space, not the origin.
  // So, we need to translate to origin, rotate, then translate back
  // Given T = transformation matrix from origin -> EE point in space (translation component of tf_no_new_rot)
  // and T' as the opposite transformation, EE point in space -> origin (translation only)
  // apply final transformation as T * R * T' * tf_no_new_rot
  const Eigen::Matrix<double, 3, 1> tf_translation = tf_no_new_rot.translation();
  Eigen::Isometry3d tf_neg_translation = Eigen::Isometry3d::Identity();  // T'
  tf_neg_translation(0, 3) = -tf_translation(0, 0);
  tf_neg_translation(1, 3) = -tf_translation(1, 0);
  tf_neg_translation(2, 3) = -tf_translation(2, 0);
  Eigen::Isometry3d tf_pos_translation = Eigen::Isometry3d::Identity();  // T
  tf_pos_translation(0, 3) = tf_translation(0, 0);
  tf_pos_translation(1, 3) = tf_translation(1, 0);
  tf_pos_translation(2, 3) = tf_translation(2, 0);

  // T * R * T' * tf_no_new_rot
  return tf2::toMsg(tf_pos_translation * tf_rot_delta * tf_neg_translation * tf_no_new_rot);
}

}  // namespace moveit_servo
