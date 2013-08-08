/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2013, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of Willow Garage, Inc. nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#ifndef MOVEIT_CORE_ROBOT_STATE_
#define MOVEIT_CORE_ROBOT_STATE_

#include <moveit/robot_model/robot_model.h>
#include <sensor_msgs/JointState.h>
//#include <moveit/robot_state/attached_body.h>

namespace moveit
{
namespace core
{

MOVEIT_CLASS_FORWARD(RobotState); 

class RobotState
{
public:
  
  enum AllocComponents
    {
      ALLOC_POSITION = 1,
      ALLOC_VELOCITY = 2,
      ALLOC_ACCELERATION = 4,
      ALLOC_TRANSFORMS = 8,
      ALLOC_POSITION_AND_VELOCITY = ALLOC_POSITION | ALLOC_VELOCITY,
      ALLOC_ALL = ALLOC_POSITION | ALLOC_VELOCITY | ALLOC_ACCELERATION | ALLOC_TRANSFORMS
    };
  
  RobotState(const RobotModelConstPtr &robot_model, AllocComponents alloc_components = ALLOC_POSITION);  
  ~RobotState();
  
  RobotState(const RobotState &other);
  
  /** \defgroup setVariablePositionGroup Setting variable position
   *  @{
   */
  
  void setVariablePositions(const double *position)
  {
    // assume everything is in order in terms of array lengths (for efficiency reasons)
    memcpy(position_, position, robot_model_->getVariableCount() * sizeof(double));
    
    // the full state includes mimic joint values, so no need to update mimic here
    
    // the index of the root joint of the system is 0. Since all joint values have potentially
    // changed, we will need to do FK from the root (if FK is ever required)
    dirty_fk_ = 0;
  }
  
  void setVariablePositions(const std::vector<double> &position)
  {
    assert(robot_model_->getVariableCount() <= position.size()); // checked only in debug mode
    setVariablePositions(&position[0]);
  }
  
  void setVariablePositions(const std::map<std::string, double> &variable_map)
  {
    for (std::map<std::string, double>::const_iterator it = variable_map.begin(), end = variable_map.end() ; it != end ; ++it)
    {
      int index = robot_model_->getVariableIndex(it->first);
      position_[index] = it->second;
      updateMimicPosition(index);
      dirty_fk_ = dirty_fk_ < 0 ? index : robot_model_->getCommonRoot(dirty_fk_, index);
    }
  }
    
  void setVariablePositions(const std::vector<std::string>& variable_names, const std::vector<double>& variable_position)
  {
    for (std::size_t i = 0 ; i < variable_names.size() ; ++i)
    { 
      std::size_t index = robot_model_->getVariableIndex(variable_names[i]);
      position_[index] = variable_position[i];  
      updateMimicPosition(index);
      dirty_fk_ = dirty_fk_ < 0 ? index : robot_model_->getCommonRoot(dirty_fk_, index);
    }
  }
  
  void setVariablePosition(const std::string &variable, double value)
  {
    setVariablePosition(robot_model_->getVariableIndex(variable), value);
  }
  
  void setVariablePosition(std::size_t variable_index, double value)
  {
    position_[variable_index] = value;
    updateMimicPosition(variable_index);
    dirty_fk_ = dirty_fk_ < 0 ? variable_index : robot_model_->getCommonRoot(dirty_fk_, variable_index);
  }
  
  const double* getVariablePositions() const
  {
    return position_;
  }  
  
  const double* getVariablePositions(const std::string &variable) const
  {
    return position_ + robot_model_->getVariableIndex(variable);
  }
  
  const double* getVariablePositions(std::size_t index) const
  {
    return position_ + index;
  }
  
  /** @} */
  
  /** \defgroup setJointPositionGroup Getting and setting joint positions
   *  @{
   */
  void setJointPositions(const std::string &joint_name, const double *position)
  {  
    setJointPositions(robot_model_->getJointModel(joint_name), position);
  }
  
  void setJointPositions(const std::string &joint_name, const std::vector<double> &position)
  {
    setJointPositions(robot_model_->getJointModel(joint_name), &position[0]);
  }
  
  void setJointPositions(const JointModel *joint, const std::vector<double> &position)
  {
    setJointPositions(joint, &position[0]);
  }
  
  void setJointPositions(const JointModel *joint, const double *position)
  {  
    const int index = joint->getFirstVariableIndex();
    memcpy(position_ + index, position, joint->getVariableCount() * sizeof(double));
    updateMimicPosition(index);
    dirty_fk_ = dirty_fk_ < 0 ? index : robot_model_->getCommonRoot(dirty_fk_, index);
  }
  
  void setJointPositions(const std::string &joint_name, const Eigen::Affine3d& transform)
  {
    setJointPositions(robot_model_->getJointModel(joint_name), transform);
  }
  
  void setJointPositions(const JointModel *joint, const Eigen::Affine3d& transform)
  {
    const int index = joint->getFirstVariableIndex();
    joint->computeJointStateValues(transform, position_ + index);
    updateMimicPosition(index);
    dirty_fk_ = dirty_fk_ < 0 ? index : robot_model_->getCommonRoot(dirty_fk_, index);
  }
  
  const double* getJointPositions(const std::string &joint_name) const
  {
    return getJointPositions(robot_model_->getJointModel(joint_name));
  }
  
  const double* getJointPositions(const JointModel *joint) const
  {
    return positions_ + joint->getFirstVariableIndex();
  }
  /** @} */
  
  
  /** \defgroup setGroupPositionGroup Getting and setting group positions
   *  @{
   */
  
  void setJointGroupPositions(const std::string &joint_group_name, const std::vector<double> &gstate)
  {
    setJointGroupPositions(robot_model_->getJointModelGroup(joint_group_name), gstate);
  }
  
  void setJointGroupPositions(const JointModelGroup *group, const std::vector<double> &gstate)
  {
    if (group->isContiguousWithinState())
      memcpy(positions_ + group->getFirstVariableIndex(), &gstate[0], group->getVariableCount() * sizeof(double));
    else
    {
      const std::vector<std::size_t> &index = group->getIndexList();
      for (std::size_t i = 0 ; i < index.size() ; ++i)
        position_[index[i]] = gstate[i];
    }
    const std::vector<std::size_t> &mimic = group->getMimicJoints();
    for (std::size_t i = 0 ; i < mimic.size() ; ++i)
      updateMimic(mimic[i]);
    
  }
  
  void copyJointGroupPositions(const std::string &joint_group_name, std::vector<double> &gstate) const
  {
    return copyJointGroupPositions(robot_model_->getJointModelGroup(joint_group_name), gstate);
  }
  
  void copyJointGroupPositions(const JointModelGroup *group, std::vector<double> &gstate) const
  {
    if (group->isContiguousWithinState())
    {
      std::size_t c = group->getVariableCount();
      gstate.resize(c);
      memcpy(&gstate[0], positions_ + group->getFirstVariableIndex(), c * sizeof(double));
    }
    else
    {
      std::size_t c = group->getVariableCount();
      gstate.resize(c);
      const std::vector<std::size_t> &index = group->getIndexList();
      for (std::size_t i = 0 ; i < index.size() ; ++i)
        gstate[i] = position_[index[i]];
    }
  }
  
  /** @} */
  
  void setVariableValues(const sensor_msgs::JointState& msg)
  {
    setVariablePositions(msg.name, msg.position);
    setVariableVelocities(msg.name, msg.velocity);
  }
  
  double* getStatePositions()
  {
    return position_;
  }

  const double* getStatePositions() const
  {
    return position_;
  }

  double* getStateVelocity()
  {
    return velocity_;
  }
  
  const double* getStateVelocity() const
  {
    return velocity_;
  }
  
  double* getStateAcceleration()
  {
    return acceleration_;
  }
  
  const double* getStateAcceleration() const
  {
    return acceleration_;
  }
  
  /** \defgroup RobotStateGetTransforms Updating and getting transforms
   *  @{
   */
  
  void updateCollisionBodyTransforms()
  {
    if (dirty_fk_ >= 0)
    {
      updateJointTransforms();
      updateLinkTransforms();
    }
    else
      if (dirty_link_transforms_ >= 0)
        updateLinkTransforms();
    
    if (dirty_collision_body_transforms_ >= 0)
    {
      const std::vector<const LinkModel*> &links = robot_model_->getUpdatedLinkModels(dirty_collision_body_transforms_);
      dirty_collision_body_transforms_ = -1;
      for (std::size_t i = 0 ; i < links.size() ; ++i)
	links[i]->updateCollisionBodyTransforms(&global_collision_body_transforms_->at(links[i]->getCollisionBodyIndex()));
    }
  }
  
  void updateLinkTransforms()
  {
    if (dirty_fk_ >= 0)
      updateJointTransforms(); // resets dirty_fk_, makes sure memory is allocated for transforms
    if (dirty_link_transforms_ >= 0)
    {
      const std::vector<const LinkModel*> &links = robot_model_->getUpdatedLinkModels(dirty_link_transforms_);
      dirty_collision_body_transforms_ = dirty_link_transforms_;
      dirty_link_transforms_ = -1;
      for (std::size_t i = 0 ; i < links.size() ; ++i)
	links[i]->updateTransform(global_link_transforms_->at(links[i]->getIndex()));
      
      // update attached bodies tf
    }
  }
  
  
  void update()
  {
    // this actually triggers all needed updates
    updateCollisionBodyTransforms();
  }
  
  
  void updateJointTransforms()
  {
    if (dirty_fk_ >= 0)
    {
      const std::vector<const JointModel*> &joint_models = robot_model_->getUpdatedJointModels(dirty_fk_);
      dirty_link_transforms_ = dirty_fk_;
      dirty_fk_ = -1;
      if (!variable_joint_transforms_)
	allocTransforms();
      for (std::size_t i = 0 ; i < joint_models.size() ; ++i)
      {
	int index = joint_models[i]->getIndex();
	int vindex = joint_models[i]->getFirstVariableIndex();
	joint_models[i]->updateTransform(position_ + vindex, variable_joint_transforms_->at(index));
      }
    }
  }
  
  const Eigen::Affine3d& getGlobalLinkTransform(const std::string &link_name)
  {
    return getGlobalLinkTransform(robot_model_->getLinkModel(link_name));
  }
  
  const Eigen::Affine3d& getGlobalLinkTransform(const LinkModel *link)
  {
    updateLinkTransforms();
    return global_link_transforms_->at(link->getIndex());
  }
  
  const Eigen::Affine3d& getCollisionBodyTransform(const std::string &link_name, std::size_t index)
  {
    return getCollisionBodyTransform(robot_model_->getLinkModel(link_name), index);
  }
  
  const Eigen::Affine3d& getCollisionBodyTransform(const LinkModel *link, std::size_t index)
  {
    updateCollisionBodyTransforms();
    return global_link_transforms_->at(link->getCollisionBodyIndex() + index);
  }
  
  const Eigen::Affine3d& getJointTransform(const std::string &joint_name)
  {
    return getJointTransform(robot_model_->getJointModel(joint_name));
  }
  
  const Eigen::Affine3d& getJointTransform(const JointModel *joint)
  {
    updateJointTransforms();
    return variable_joint_transforms_->at(joint->getIndex());
  }
  
  
  
  const Eigen::Affine3d& getGlobalLinkTransform(const std::string &link_name) const
  {
    return getGlobalLinkTransform(robot_model_->getLinkModel(link_name));
  }
  
  const Eigen::Affine3d& getGlobalLinkTransform(const LinkModel *link) const
  {
    return global_link_transforms_->at(link->getIndex());
  }
  
  const Eigen::Affine3d& getCollisionBodyTransform(const std::string &link_name, std::size_t index) const
  {
    return getCollisionBodyTransform(robot_model_->getLinkModel(link_name), index);
  }
  
  const Eigen::Affine3d& getCollisionBodyTransform(const LinkModel *link, std::size_t index) const
  {
    return global_link_transforms_->at(link->getCollisionBodyIndex() + index);
  }
  
  const Eigen::Affine3d& getJointTransform(const std::string &joint_name) const
  {
    return getJointTransform(robot_model_->getJointModel(joint_name));
  }
  
  const Eigen::Affine3d& getJointTransform(const JointModel *joint) const
  {
    return variable_joint_transforms_->at(joint->getIndex());
  }
  
  /** \defgroup distanceFunctions Computing distances
   *  @{
   */
  
  double distance(const RobotState &other) const
  {
    return distance(other.getStatePositions());
  }
  
  double distance(const RobotState &other, const JointModel *joint) const
  {
    return distance(other.getJointPositions(joint), joint);
  }
  
  double distance(const RobotState &other, const JointModelGroup *joint_group) const;
  
  double distance(const double *state_position) const;
  double distance(const double *joint_position, const JointModel *joint) const;  
  double distance(const double *joint_group_position, const JointModelGroup *joint_group) const;
  
  void interpolate(const RobotState &to, double t, RobotState &state)
  {
    interpolate(to.getJointPositions(), t, state.getJointPositions());
  }
  
  void interpolate(const RobotState &to, double t, RobotState &state, const JointModel *joint)
  {
    interpolate(to.getJointPositions(joint), t, state.getJointPositions(joint), joint);
  }
  
  void interpolate(const RobotState &to, double t, RobotState &state, const JointModelGroup *joint_group);

  void interpolate(const double *to, double t, double *state);
  void interpolate(const double *to, double t, double *state, const JointModel *joint);
  void interpolate(const double *to, double t, double *state, const JointModelGroup *joint_group);

  void enforceBounds();
  void enforceBounds(const JointModel *joint);
  void enforceBounds(const JointModelGroup *joint_group);
  
  bool satisfiesBounds(double margin = 0.0) const;
  bool satisfiesBounds(const JointModel *joint, double margin = 0.0) const;
  bool satisfiesBounds(const JointModelGroup *joint_group, double margin = 0.0) const;
  
  /** @} */
  
  /** \defgroup RobotStateAttachedBodies Managing attached bodies
   *  @{
   */
  //  void getAttachedBodies(std::vector<const AttachedBody*> &attached_bodies) const;
  //  void getAttachedBodies(const std::string &link, std::vector<const AttachedBody*> &attached_bodies) const;
  
  /** @brief Get the attached body with name \e id */
  //  const AttachedBody* getAttachedBody(const std::string &id) const;
  
  /** \brief Check if an attached body named \e id exists in this group */
  //  bool hasAttachedBody(const std::string &id) const;
  /** @} */
  
private:
  
  void allocVelocity()
  {
    if (!velocity_)
    {
      called_new_for_ |= ALLOC_VELOCITY;
      velocity_ = new double[robot_model->getVariableCount()];
    }
  }
  
  void allocAcceleration()
  {
    if (!acceleration_)
    {
      if (velocity_)
      {
        called_new_for_ |= ALLOC_ACCELERATION;
	acceleration_ = new double[robot_model_->getVariableCount()];
      }
      else
      {
        called_new_for_ |= ALLOC_VELOCITY;
        velocity_ = new double[robot_model_->getVariableCount() * 2];
        acceleration_ = velocity_ + robot_model_->getVariableCount();
      }
    }
  }
  
  void allocTransforms()
  {
    if (!variable_joint_transforms_)
    {
      transforms_.resize(robot_model_->getJointCount() + robot_model_->getLinkCount() + robot_model_->getLinkGeometryCount());
      if (transforms_.size() > 0)
      {
	variable_joint_transforms_ = &transforms_[0];      
	global_link_transforms_ = variable_joint_transforms_ + robot_model_->getJointCount();
	global_collision_body_transforms_ = global_link_transforms_ + robot_model_->getLinkCount();
      }
    }
  }
  
  void updateMimicPosition(int variable_index)
  {
    const JointModel *jm = robot_model_->getJointOfVariable(variable_index);
    if (jm)
    {
      const std::vector<const JointModel*> &mim = jm->getMimicRequests();
      double v = positions_[variable_index];
      //      for (std::size_t i = 0 ; i < mim.size() ; ++i)
      //        positions_[mim[i]->getMimic  mim[i]->getMimicFactor() * v + mim[i]->getMimicOffset()
             
      
    }
  }
  
  RobotModelConstPtr robot_model_;
  AllocComponents called_new_for_;
  
  double *position_;
  double *velocity_;
  double *acceleration_;

  int dirty_fk_;
  int dirty_collision_body_transforms_;
  int dirty_link_transforms_;
  
  EigenSTL::vector_Affine3d transforms_;
  Eigen::Affine3d *variable_joint_transforms_; // this points to an element in transforms_, so it is aligned 
  Eigen::Affine3d *global_link_transforms_;  // this points to an element in transforms_, so it is aligned 
  Eigen::Affine3d *global_collision_body_transforms_;  // this points to an element in transforms_, so it is aligned 
  
  //  std::vector<AttachedBody*> attached_bodies_;
};

// for every joint index modified, we need a map of where to start FK?
// => for every pair of joint indices (matrix) common ancestor index is where we start FK.

// jointA->isAncestor(jointB) then jointA is sufficient root.
// => maintain list of dirty roots.
}
}

#endif
