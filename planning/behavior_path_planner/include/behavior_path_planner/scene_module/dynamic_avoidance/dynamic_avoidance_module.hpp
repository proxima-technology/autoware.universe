// Copyright 2023 TIER IV, Inc.
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

#ifndef BEHAVIOR_PATH_PLANNER__SCENE_MODULE__DYNAMIC_AVOIDANCE__DYNAMIC_AVOIDANCE_MODULE_HPP_
#define BEHAVIOR_PATH_PLANNER__SCENE_MODULE__DYNAMIC_AVOIDANCE__DYNAMIC_AVOIDANCE_MODULE_HPP_

#include "behavior_path_planner/scene_module/scene_module_interface.hpp"

#include <rclcpp/rclcpp.hpp>

#include <autoware_auto_perception_msgs/msg/predicted_object.hpp>
#include <autoware_auto_planning_msgs/msg/path_with_lane_id.hpp>
#include <autoware_auto_vehicle_msgs/msg/turn_indicators_command.hpp>
#include <tier4_planning_msgs/msg/avoidance_debug_msg.hpp>
#include <tier4_planning_msgs/msg/avoidance_debug_msg_array.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace behavior_path_planner
{
struct DynamicAvoidanceParameters
{
  // common
  bool enable_debug_info{true};

  // obstacle types to avoid
  bool avoid_car{true};
  bool avoid_truck{true};
  bool avoid_bus{true};
  bool avoid_trailer{true};
  bool avoid_unknown{false};
  bool avoid_bicycle{false};
  bool avoid_motorcycle{false};
  bool avoid_pedestrian{false};
  double min_obstacle_vel{0.0};
  int successive_num_to_entry_dynamic_avoidance_condition{0};

  double min_obj_lat_offset_to_ego_path{0.0};
  double max_obj_lat_offset_to_ego_path{0.0};

  double min_time_to_start_cut_in{0.0};
  double min_lon_offset_ego_to_cut_in_object{0.0};
  double max_front_object_angle{0.0};
  double min_crossing_object_vel{0.0};
  double max_crossing_object_angle{0.0};

  // drivable area generation
  double lat_offset_from_obstacle{0.0};
  double max_lat_offset_to_avoid{0.0};

  double max_time_to_collision_overtaking_object{0.0};
  double start_duration_to_avoid_overtaking_object{0.0};
  double end_duration_to_avoid_overtaking_object{0.0};
  double duration_to_hold_avoidance_overtaking_object{0.0};

  double max_time_to_collision_oncoming_object{0.0};
  double start_duration_to_avoid_oncoming_object{0.0};
  double end_duration_to_avoid_oncoming_object{0.0};
};

class DynamicAvoidanceModule : public SceneModuleInterface
{
public:
  struct DynamicAvoidanceObject
  {
    DynamicAvoidanceObject(
      const PredictedObject & predicted_object, const double arg_vel, const double arg_lat_vel,
      const bool arg_is_collision_left, const double arg_time_to_collision)
    : uuid(tier4_autoware_utils::toHexString(predicted_object.object_id)),
      pose(predicted_object.kinematics.initial_pose_with_covariance.pose),
      shape(predicted_object.shape),
      vel(arg_vel),
      lat_vel(arg_lat_vel),
      is_collision_left(arg_is_collision_left),
      time_to_collision(arg_time_to_collision)
    {
      for (const auto & path : predicted_object.kinematics.predicted_paths) {
        predicted_paths.push_back(path);
      }
    }

    std::string uuid;
    geometry_msgs::msg::Pose pose;
    autoware_auto_perception_msgs::msg::Shape shape;
    double vel;
    double lat_vel;
    bool is_collision_left;
    double time_to_collision;
    std::vector<autoware_auto_perception_msgs::msg::PredictedPath> predicted_paths{};
  };
  struct DynamicAvoidanceObjectCandidate
  {
    DynamicAvoidanceObject object;
    int alive_counter;

    static std::optional<DynamicAvoidanceObjectCandidate> getObjectFromUuid(
      const std::vector<DynamicAvoidanceObjectCandidate> & objects, const std::string & target_uuid)
    {
      const auto itr = std::find_if(objects.begin(), objects.end(), [&](const auto & object) {
        return object.object.uuid == target_uuid;
      });

      if (itr == objects.end()) {
        return std::nullopt;
      }
      return *itr;
    }
  };

  DynamicAvoidanceModule(
    const std::string & name, rclcpp::Node & node,
    std::shared_ptr<DynamicAvoidanceParameters> parameters,
    const std::unordered_map<std::string, std::shared_ptr<RTCInterface>> & rtc_interface_ptr_map);

  void updateModuleParams(const std::any & parameters) override
  {
    parameters_ = std::any_cast<std::shared_ptr<DynamicAvoidanceParameters>>(parameters);
  }

  bool isExecutionRequested() const override;
  bool isExecutionReady() const override;
  ModuleStatus updateState() override;
  BehaviorModuleOutput plan() override;
  CandidateOutput planCandidate() const override;
  BehaviorModuleOutput planWaitingApproval() override;
  void updateData() override;
  void acceptVisitor(
    [[maybe_unused]] const std::shared_ptr<SceneModuleVisitor> & visitor) const override
  {
  }

private:
  struct LatLonOffset
  {
    const size_t nearest_idx;
    const double max_lat_offset;
    const double min_lat_offset;
    const double max_lon_offset;
    const double min_lon_offset;
  };

  bool isLabelTargetObstacle(const uint8_t label) const;
  std::vector<DynamicAvoidanceObjectCandidate> calcTargetObjectsCandidate();
  bool willObjectCutIn(
    const std::vector<PathPointWithLaneId> & ego_path, const PredictedPath & predicted_path,
    const double obj_tangent_vel, const LatLonOffset & lat_lon_offset) const;
  bool willObjectCutOut(
    const double obj_tangent_vel, const double obj_normal_vel, const bool is_collision_left) const;
  bool isObjectFarFromPath(
    const PredictedObject & predicted_object, const double obj_dist_to_path) const;
  double calcTimeToCollision(
    const std::vector<PathPointWithLaneId> & ego_path, const geometry_msgs::msg::Pose & obj_pose,
    const double obj_tangent_vel) const;
  std::optional<std::pair<size_t, size_t>> calcCollisionSection(
    const std::vector<PathPointWithLaneId> & ego_path, const PredictedPath & obj_path) const;
  LatLonOffset getLateralLongitudinalOffset(
    const std::vector<PathPointWithLaneId> & ego_path, const PredictedObject & object) const;

  std::pair<lanelet::ConstLanelets, lanelet::ConstLanelets> getAdjacentLanes(
    const double forward_distance, const double backward_distance) const;
  std::optional<tier4_autoware_utils::Polygon2d> calcDynamicObstaclePolygon(
    const DynamicAvoidanceObject & object) const;

  std::vector<DynamicAvoidanceModule::DynamicAvoidanceObjectCandidate>
    prev_target_objects_candidate_;
  std::vector<DynamicAvoidanceModule::DynamicAvoidanceObject> target_objects_;
  // std::vector<DynamicAvoidanceModule::DynamicAvoidanceObject> prev_target_objects_;
  std::shared_ptr<DynamicAvoidanceParameters> parameters_;

  struct ObjectsVariable
  {
    void resetCurrentUuids() { current_uuids_.clear(); }
    void addCurrentUuid(const std::string & uuid) { current_uuids_.push_back(uuid); }
    void removeCounterUnlessUpdated()
    {
      std::vector<std::string> obsolete_uuids;
      for (const auto & key_and_value : variable_) {
        if (
          std::find(current_uuids_.begin(), current_uuids_.end(), key_and_value.first) ==
          current_uuids_.end()) {
          obsolete_uuids.push_back(key_and_value.first);
        }
      }

      for (const auto & obsolete_uuid : obsolete_uuids) {
        variable_.erase(obsolete_uuid);
      }
    }

    std::optional<double> get(const std::string & uuid) const
    {
      if (variable_.count(uuid) != 0) {
        return variable_.at(uuid);
      }
      return std::nullopt;
    }
    void update(const std::string & uuid, const double new_variable)
    {
      if (variable_.count(uuid) != 0) {
        variable_.at(uuid) = new_variable;
      } else {
        variable_.emplace(uuid, new_variable);
      }
    }

    std::unordered_map<std::string, double> variable_;
    std::vector<std::string> current_uuids_;
  };
  mutable ObjectsVariable prev_objects_min_bound_lat_offset_;
};
}  // namespace behavior_path_planner

#endif  // BEHAVIOR_PATH_PLANNER__SCENE_MODULE__DYNAMIC_AVOIDANCE__DYNAMIC_AVOIDANCE_MODULE_HPP_
