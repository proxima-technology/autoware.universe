// Copyright 2021 Tier IV, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BEHAVIOR_PATH_PLANNER__SCENE_MODULE__SAMPLING_PLANNER__SAMPLING_PLANNER_MODULE_HPP_
#define BEHAVIOR_PATH_PLANNER__SCENE_MODULE__SAMPLING_PLANNER__SAMPLING_PLANNER_MODULE_HPP_

#include "behavior_path_planner/marker_utils/utils.hpp"
#include "behavior_path_planner/scene_module/scene_module_interface.hpp"
#include "behavior_path_planner/utils/path_utils.hpp"
#include "behavior_path_planner/utils/sampling_planner/sampling_planner_parameters.hpp"
#include "behavior_path_planner/utils/sampling_planner/util.hpp"
#include "behavior_path_planner/utils/utils.hpp"
#include "bezier_sampler/bezier_sampling.hpp"
#include "frenet_planner/frenet_planner.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sampler_common/structures.hpp"
#include "sampler_common/transform/spline_transform.hpp"
#include "tier4_autoware_utils/geometry/boost_geometry.hpp"
#include "tier4_autoware_utils/ros/update_param.hpp"
#include "tier4_autoware_utils/system/stop_watch.hpp"
#include "vehicle_info_util/vehicle_info_util.hpp"

#include <rclcpp/rclcpp.hpp>
#include <sampler_common/structures.hpp>
#include <tier4_autoware_utils/geometry/boost_geometry.hpp>

#include <autoware_auto_planning_msgs/msg/path_with_lane_id.hpp>
#include <tier4_planning_msgs/msg/lateral_offset.hpp>

#include <algorithm>
#include <any>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace behavior_path_planner
{
using autoware_auto_planning_msgs::msg::TrajectoryPoint;
struct SamplingPlannerData
{
  // input
  std::vector<TrajectoryPoint> traj_points;  // converted from the input path
  std::vector<geometry_msgs::msg::Point> left_bound;
  std::vector<geometry_msgs::msg::Point> right_bound;

  // ego
  geometry_msgs::msg::Pose ego_pose;
  double ego_vel{};
};
class SamplingPlannerModule : public SceneModuleInterface
{
public:
  SamplingPlannerModule(
    const std::string & name, rclcpp::Node & node,
    const std::shared_ptr<SamplingPlannerParameters> & parameters,
    const std::unordered_map<std::string, std::shared_ptr<RTCInterface>> & rtc_interface_ptr_map);

  bool isExecutionRequested() const override;
  bool isExecutionReady() const override;
  BehaviorModuleOutput plan() override;
  CandidateOutput planCandidate() const override;
  void updateData() override;

  void updateModuleParams(const std::any & parameters) override
  {
    parameters_ = std::any_cast<std::shared_ptr<SamplingPlannerParameters>>(parameters);
  }

  void acceptVisitor(
    [[maybe_unused]] const std::shared_ptr<SceneModuleVisitor> & visitor) const override
  {
  }

private:
  SamplingPlannerData createPlannerData(const PlanResult & path);

  PlanResult generatePath();

  bool canTransitSuccessState() override { return false; }

  bool canTransitFailureState() override { return false; }

  bool canTransitIdleToRunningState() override { return false; }

  frenet_planner::SamplingParameters prepareSamplingParameters(
    const sampler_common::State & initial_state,
    const sampler_common::transform::Spline2D & path_spline, const Parameters & params);

  // member
  Parameters params_;
  std::shared_ptr<SamplingPlannerParameters> parameters_;

  vehicle_info_util::VehicleInfo vehicle_info_{};
};

}  // namespace behavior_path_planner

#endif  // BEHAVIOR_PATH_PLANNER__SCENE_MODULE__SAMPLING_PLANNER__SAMPLING_PLANNER_MODULE_HPP_
