#pragma once

#include "glomap/estimators/optimization_base.h"
#include "glomap/scene/types_sfm.h"
#include "glomap/types.h"

#include <colmap/geometry/sim3.h>

namespace glomap {

struct GlobalPositionerOptions : public OptimizationBaseOptions {
  // Defines a bounding box with the min-corner and max-corner coordinates for
  // generating random positions.
  using AxisAlignedBoundingBox = std::pair<Eigen::Vector3d, Eigen::Vector3d>;

  // ONLY_POINTS is recommended
  enum ConstraintType {
    // only include camera to point constraints
    ONLY_POINTS,
    // only include camera to camera constraints
    ONLY_CAMERAS,
    // the points and cameras are reweighted to have similar total contribution
    POINTS_AND_CAMERAS_BALANCED,
    // treat each contribution from camera to point and camera to camera equally
    POINTS_AND_CAMERAS,
  };

  // Whether initialize the reconstruction randomly
  bool generate_random_positions = true;
  bool generate_random_points = true;
  bool generate_scales = true;  // Now using fixed 1 as initializaiton

  // Flags for which parameters to optimize
  bool optimize_positions = true;
  bool optimize_points = true;
  bool optimize_scales = true;

  bool use_gpu = true;
  std::string gpu_index = "-1";
  int min_num_images_gpu_solver = 50;

  // Constrain the minimum number of views per track
  int min_num_view_per_track = 3;

  // Random seed
  unsigned seed = 1;

  AxisAlignedBoundingBox cameras_bbox = {Eigen::Vector3d::Constant(-100),
                                         Eigen::Vector3d::Constant(100)};
  AxisAlignedBoundingBox points_bbox = {Eigen::Vector3d::Constant(-100),
                                        Eigen::Vector3d::Constant(100)};

  // the type of global positioning
  ConstraintType constraint_type = ONLY_POINTS;
  double constraint_reweight_scale =
      1.0;  // only relevant for POINTS_AND_CAMERAS_BALANCED

  GlobalPositionerOptions() : OptimizationBaseOptions() {
    thres_loss_function = 1e-1;
  }

  std::shared_ptr<ceres::LossFunction> CreateLossFunction() {
    return std::make_shared<ceres::HuberLoss>(thres_loss_function);
  }
};

class GlobalPositioner {
 public:
  explicit GlobalPositioner(const GlobalPositionerOptions& options);

  // Returns true if the optimization was a success, false if there was a
  // failure.
  // Assume tracks here are already filtered
  bool Solve(const ViewGraph& view_graph,
             std::unordered_map<camera_t, Camera>& cameras,
             std::unordered_map<image_t, Image>& images,
             std::unordered_map<track_t, Track>& tracks);

  GlobalPositionerOptions& GetOptions() { return options_; }

 protected:
  void SetupProblem(const ViewGraph& view_graph,
                    const std::unordered_map<track_t, Track>& tracks);

  // Initialize all cameras to be random.
  void InitializeRandomPositions(const ViewGraph& view_graph,
                                 std::unordered_map<image_t, Image>& images,
                                 std::unordered_map<track_t, Track>& tracks);

  // Creates camera to camera constraints from relative translations. (3D)
  void AddCameraToCameraConstraints(const ViewGraph& view_graph,
                                    std::unordered_map<image_t, Image>& images);

  // Add tracks to the problem
  void AddPointToCameraConstraints(
      std::unordered_map<camera_t, Camera>& cameras,
      std::unordered_map<image_t, Image>& images,
      std::unordered_map<track_t, Track>& tracks);

  // Add a single track to the problem
  void AddTrackToProblem(track_t track_id,
                         std::unordered_map<camera_t, Camera>& cameras,
                         std::unordered_map<image_t, Image>& images,
                         std::unordered_map<track_t, Track>& tracks);

  // Set the parameter groups
  void AddCamerasAndPointsToParameterGroups(
      std::unordered_map<image_t, Image>& images,
      std::unordered_map<track_t, Track>& tracks);

  // Parameterize the variables, set some variables to be constant if desired
  void ParameterizeVariables(std::unordered_map<image_t, Image>& images,
                             std::unordered_map<track_t, Track>& tracks);

  // During the optimization, the camera translation is set to be the camera
  // center Convert the results back to camera poses
  void ConvertResults(std::unordered_map<image_t, Image>& images,
                      std::unordered_map<track_t, Track>& tracks);

  GlobalPositionerOptions options_;

  // Transformation that adjusts prior positions to align with the
  // camera's bounding box.
  colmap::Sim3d cameras_bbox_from_prior_frame_;

  std::mt19937 random_generator_;
  std::unique_ptr<ceres::Problem> problem_;

  // Loss functions for reweighted terms.
  std::shared_ptr<ceres::LossFunction> loss_function_;
  std::shared_ptr<ceres::LossFunction> loss_function_ptcam_uncalibrated_;
  std::shared_ptr<ceres::LossFunction> loss_function_ptcam_calibrated_;

  // Auxiliary scale variables.
  std::vector<double> scales_;
};

}  // namespace glomap
