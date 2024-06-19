/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2012, Willow Garage, Inc.
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
 *   * Neither the name of Willow Garage nor the names of its
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

/* Author: Dave Coleman */

// SA
#include <moveit_setup_srdf_plugins/robot_poses_widget.hpp>
#include <moveit_setup_framework/qt/helper_widgets.hpp>
#include <moveit_msgs/msg/joint_limits.hpp>
// Qt
#include <QApplication>
#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QTableWidget>

namespace moveit_setup
{
namespace srdf_setup
{
// ******************************************************************************************
// Outer User Interface for MoveIt Configuration Assistant
// ******************************************************************************************
void RobotPosesWidget::onInit()
{
  // Set pointer to null so later we can tell if we need to delete it
  joint_list_layout_ = nullptr;

  // Basic widget container
  QVBoxLayout* layout = new QVBoxLayout();

  // Top Header Area ------------------------------------------------

  auto header = new HeaderWidget("Define Robot Poses",
                                 "Create poses for the robot. Poses are defined as sets of joint values for particular "
                                 "planning groups. This is useful for things like <i>home position</i>. The "
                                 "<i>first</i> listed pose will be the robot's initial pose in simulation.",
                                 this);
  layout->addWidget(header);

  // Create contents screens ---------------------------------------
  pose_list_widget_ = createContentsWidget();
  pose_edit_widget_ = createEditWidget();

  // Create stacked layout -----------------------------------------
  stacked_widget_ = new QStackedWidget(this);
  stacked_widget_->addWidget(pose_list_widget_);  // screen index 0
  stacked_widget_->addWidget(pose_edit_widget_);  // screen index 1
  layout->addWidget(stacked_widget_);

  // Finish Layout --------------------------------------------------
  setLayout(layout);
}

// ******************************************************************************************
// Create the main content widget
// ******************************************************************************************
QWidget* RobotPosesWidget::createContentsWidget()
{
  // Main widget
  QWidget* content_widget = new QWidget(this);

  // Basic widget container
  QVBoxLayout* layout = new QVBoxLayout(this);

  // Table ------------ ------------------------------------------------

  data_table_ = new QTableWidget(this);
  data_table_->setColumnCount(2);
  data_table_->setSortingEnabled(true);
  data_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  connect(data_table_, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(editDoubleClicked(int, int)));
  connect(data_table_, SIGNAL(currentCellChanged(int, int, int, int)), this, SLOT(previewClicked(int, int, int, int)));
  layout->addWidget(data_table_);

  // Set header labels
  QStringList header_list;
  header_list.append("Pose Name");
  header_list.append("Group Name");
  data_table_->setHorizontalHeaderLabels(header_list);

  // Bottom Buttons --------------------------------------------------

  QHBoxLayout* controls_layout = new QHBoxLayout();

  // Set Default Button
  QPushButton* btn_default = new QPushButton("&Show Default Pose", this);
  btn_default->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  btn_default->setMaximumWidth(300);
  connect(btn_default, SIGNAL(clicked()), this, SLOT(showDefaultPose()));
  controls_layout->addWidget(btn_default);
  controls_layout->setAlignment(btn_default, Qt::AlignLeft);

  // Set play button
  QPushButton* btn_play = new QPushButton("&MoveIt", this);
  btn_play->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  btn_play->setMaximumWidth(300);
  connect(btn_play, SIGNAL(clicked()), this, SLOT(playPoses()));
  controls_layout->addWidget(btn_play);
  controls_layout->setAlignment(btn_play, Qt::AlignLeft);

  // Spacer
  controls_layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  // Edit Selected Button
  btn_edit_ = new QPushButton("&Edit Selected", this);
  btn_edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  btn_edit_->setMaximumWidth(300);
  btn_edit_->hide();  // show once we know if there are existing poses
  connect(btn_edit_, SIGNAL(clicked()), this, SLOT(editSelected()));
  controls_layout->addWidget(btn_edit_);
  controls_layout->setAlignment(btn_edit_, Qt::AlignRight);

  // Delete
  btn_delete_ = new QPushButton("&Delete Selected", this);
  connect(btn_delete_, SIGNAL(clicked()), this, SLOT(deleteSelected()));
  controls_layout->addWidget(btn_delete_);
  controls_layout->setAlignment(btn_delete_, Qt::AlignRight);

  // Add Group Button
  QPushButton* btn_add = new QPushButton("&Add Pose", this);
  btn_add->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  btn_add->setMaximumWidth(300);
  connect(btn_add, SIGNAL(clicked()), this, SLOT(showNewScreen()));
  controls_layout->addWidget(btn_add);
  controls_layout->setAlignment(btn_add, Qt::AlignRight);

  // Add layout
  layout->addLayout(controls_layout);

  // Set layout -----------------------------------------------------
  content_widget->setLayout(layout);

  return content_widget;
}

// ******************************************************************************************
// Create the edit widget
// ******************************************************************************************
QWidget* RobotPosesWidget::createEditWidget()
{
  // Main widget
  QWidget* edit_widget = new QWidget(this);
  // Layout
  QVBoxLayout* layout = new QVBoxLayout();

  // 2 columns -------------------------------------------------------

  QHBoxLayout* columns_layout = new QHBoxLayout();
  QVBoxLayout* column1 = new QVBoxLayout();
  column2_ = new QVBoxLayout();

  // Column 1 --------------------------------------------------------

  // Simple form -------------------------------------------
  QFormLayout* form_layout = new QFormLayout();
  // form_layout->setContentsMargins( 0, 15, 0, 15 );
  form_layout->setRowWrapPolicy(QFormLayout::WrapAllRows);

  // Name input
  pose_name_field_ = new QLineEdit(this);
  // pose_name_field_->setMaximumWidth( 300 );
  form_layout->addRow("Pose Name:", pose_name_field_);

  // Group name input
  group_name_field_ = new QComboBox(this);
  group_name_field_->setEditable(false);
  // Connect the signal for changes to the drop down box
  connect(group_name_field_, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(loadJointSliders(const QString&)));
  // group_name_field_->setMaximumWidth( 300 );
  form_layout->addRow("Planning Group:", group_name_field_);

  // Indicator that robot is in collision or not
  collision_warning_ = new QLabel("<font color='red'><b>Robot in Collision State</b></font>", this);
  collision_warning_->setTextFormat(Qt::RichText);
  collision_warning_->hide();  // show later
  form_layout->addRow(" ", collision_warning_);

  column1->addLayout(form_layout);
  columns_layout->addLayout(column1);

  // Column 2 --------------------------------------------------------

  // Box to hold joint sliders
  joint_list_widget_ = new QWidget(this);

  // Create scroll area
  scroll_area_ = new QScrollArea(this);
  // scroll_area_->setBackgroundRole(QPalette::Dark);
  scroll_area_->setWidget(joint_list_widget_);

  column2_->addWidget(scroll_area_);

  columns_layout->addLayout(column2_);

  // Set columns in main layout
  layout->addLayout(columns_layout);

  // Bottom Buttons --------------------------------------------------

  QHBoxLayout* controls_layout = new QHBoxLayout();
  controls_layout->setContentsMargins(0, 25, 0, 15);

  // Spacer
  controls_layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  // Save
  btn_save_ = new QPushButton("&Save", this);
  btn_save_->setMaximumWidth(200);
  connect(btn_save_, SIGNAL(clicked()), this, SLOT(doneEditing()));
  controls_layout->addWidget(btn_save_);
  controls_layout->setAlignment(btn_save_, Qt::AlignRight);

  // Cancel
  btn_cancel_ = new QPushButton("&Cancel", this);
  btn_cancel_->setMaximumWidth(200);
  connect(btn_cancel_, SIGNAL(clicked()), this, SLOT(cancelEditing()));
  controls_layout->addWidget(btn_cancel_);
  controls_layout->setAlignment(btn_cancel_, Qt::AlignRight);

  // Add layout
  layout->addLayout(controls_layout);

  // Set layout -----------------------------------------------------
  edit_widget->setLayout(layout);

  return edit_widget;
}

// ******************************************************************************************
// Show edit screen for creating a new pose
// ******************************************************************************************
void RobotPosesWidget::showNewScreen()
{
  // Switch to screen - do this before clearEditText()
  stacked_widget_->setCurrentIndex(1);

  // Remember that this is a new pose
  current_edit_pose_ = nullptr;

  // Manually send the load joint sliders signal
  if (!group_name_field_->currentText().isEmpty())
    loadJointSliders(group_name_field_->currentText());

  // Clear previous data
  pose_name_field_->setText("");

  // Announce that this widget is in modal mode
  Q_EMIT setModalMode(true);
}

// ******************************************************************************************
// Edit whatever element is selected
// ******************************************************************************************
void RobotPosesWidget::editDoubleClicked(int /*row*/, int /*column*/)
{
  // We'll just base the edit on the selection highlight
  editSelected();
}

// ******************************************************************************************
// Preview whatever element is selected
// ******************************************************************************************
void RobotPosesWidget::previewClicked(int row, int /*column*/, int /*previous_row*/, int /*previous_column*/)
{
  QTableWidgetItem* name = data_table_->item(row, 0);
  QTableWidgetItem* group = data_table_->item(row, 1);

  // nullptr check before dereferencing
  if (name && group)
  {
    // Find the selected in datastructure
    srdf::Model::GroupState* pose = setup_step_.findPoseByName(name->text().toStdString(), group->text().toStdString());

    showPose(*pose);
  }
}

// ******************************************************************************************
// Show the robot in the current pose
// ******************************************************************************************
void RobotPosesWidget::showPose(const srdf::Model::GroupState& pose)
{
  // Set the joints based on the SRDF pose
  moveit::core::RobotState& robot_state = setup_step_.getState();
  for (const auto& key_value_pair : pose.joint_values_)
  {
    robot_state.setJointPositions(key_value_pair.first, key_value_pair.second);
  }

  // Update the joints
  updateStateAndCollision(robot_state);

  // Unhighlight all links
  rviz_panel_->unhighlightAll();

  // Highlight group
  rviz_panel_->highlightGroup(pose.group_);
}

// ******************************************************************************************
// Show the robot in its default joint positions
// ******************************************************************************************
void RobotPosesWidget::showDefaultPose()
{
  moveit::core::RobotState& robot_state = setup_step_.getState();
  robot_state.setToDefaultValues();

  // Update the joints
  updateStateAndCollision(robot_state);

  // Unhighlight all links
  rviz_panel_->unhighlightAll();
}

// ******************************************************************************************
// Play through the poses
// ******************************************************************************************
void RobotPosesWidget::playPoses()
{
  using namespace std::chrono_literals;

  // Loop through each pose and play them
  for (const srdf::Model::GroupState& pose : setup_step_.getGroupStates())
  {
    RCLCPP_INFO_STREAM(setup_step_.getLogger(), "Showing pose " << pose.name_);
    showPose(pose);
    rclcpp::sleep_for(50000000ns);  // 0.05 s
    QApplication::processEvents();
    rclcpp::sleep_for(450000000ns);  // 0.45 s
  }
}

// ******************************************************************************************
// Edit whatever element is selected
// ******************************************************************************************
void RobotPosesWidget::editSelected()
{
  const auto& ranges = data_table_->selectedRanges();
  if (ranges.empty())
    return;
  edit(ranges[0].bottomRow());
}

// ******************************************************************************************
// Edit pose
// ******************************************************************************************
void RobotPosesWidget::edit(int row)
{
  const std::string& name = data_table_->item(row, 0)->text().toStdString();
  const std::string& group = data_table_->item(row, 1)->text().toStdString();

  // Find the selected in datastruture
  srdf::Model::GroupState* pose = setup_step_.findPoseByName(name, group);
  current_edit_pose_ = pose;

  // Set pose name
  pose_name_field_->setText(pose->name_.c_str());

  // Set group:
  int index = group_name_field_->findText(pose->group_.c_str());
  if (index == -1)
  {
    QMessageBox::critical(this, "Error Loading", "Unable to find group name in drop down box");
    return;
  }
  group_name_field_->setCurrentIndex(index);

  showPose(*pose);

  // Switch to screen - do this before setCurrentIndex
  stacked_widget_->setCurrentIndex(1);

  // Announce that this widget is in modal mode
  Q_EMIT setModalMode(true);

  // Manually send the load joint sliders signal
  loadJointSliders(QString(pose->group_.c_str()));
}

// ******************************************************************************************
// Populate the combo dropdown box with avail group names
// ******************************************************************************************
void RobotPosesWidget::loadGroupsComboBox()
{
  // Remove all old groups
  group_name_field_->clear();

  // Add all group names to combo box
  for (const std::string& group_name : setup_step_.getGroupNames())
  {
    group_name_field_->addItem(group_name.c_str());
  }
}

// ******************************************************************************************
// Load the joint sliders based on group selected from combo box
// ******************************************************************************************
void RobotPosesWidget::loadJointSliders(const QString& selected)
{
  // Ignore this event if the combo box is empty. This occurs when clearing the combo box and reloading with the
  // newest groups. Also ignore if we are not on the edit screen
  if (!group_name_field_->count() || selected.isEmpty() || stacked_widget_->currentIndex() == 0)
    return;

  // Get group name from input
  const std::string group_name = selected.toStdString();

  std::vector<const moveit::core::JointModel*> joint_models;

  try
  {
    joint_models = setup_step_.getSimpleJointModels(group_name);
  }
  catch (const std::runtime_error& e)
  {
    QMessageBox::critical(this, "Error Loading", QString(e.what()));
    return;
  }

  // Delete old sliders from joint_list_layout_ if this has been loaded before
  if (joint_list_layout_)
  {
    delete joint_list_layout_;
    qDeleteAll(joint_list_widget_->children());
  }

  // Create layout again
  joint_list_layout_ = new QVBoxLayout();
  joint_list_widget_->setLayout(joint_list_layout_);
  //  joint_list_widget_->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
  joint_list_widget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  joint_list_widget_->setMinimumSize(50, 50);  // w, h

  // Get list of associated joints
  const auto& robot_state = setup_step_.getState();

  // Iterate through the joints
  for (const moveit::core::JointModel* joint_model : joint_models)
  {
    double init_value = robot_state.getVariablePosition(joint_model->getVariableNames()[0]);

    // For each joint in group add slider
    SliderWidget* sw = new SliderWidget(this, joint_model, init_value);
    joint_list_layout_->addWidget(sw);

    // Connect value change event
    connect(sw, SIGNAL(jointValueChanged(const std::string&, double)), this,
            SLOT(updateRobotModel(const std::string&, double)));
  }

  // Copy the width of column 2 and manually calculate height from number of joints
  joint_list_widget_->resize(300, joint_models.size() * 70);  // w, h

  // Update the robot model in Rviz with newly selected joint values
  updateStateAndCollision(robot_state);

  // Highlight the planning group
  rviz_panel_->unhighlightAll();
  rviz_panel_->highlightGroup(group_name);
}

// ******************************************************************************************
// Delete currently editing item
// ******************************************************************************************
void RobotPosesWidget::deleteSelected()
{
  const auto& ranges = data_table_->selectedRanges();
  if (ranges.empty())
    return;
  int row = ranges[0].bottomRow();

  const std::string& name = data_table_->item(row, 0)->text().toStdString();
  const std::string& group = data_table_->item(row, 1)->text().toStdString();

  // Confirm user wants to delete group
  if (QMessageBox::question(this, "Confirm Pose Deletion",
                            QString("Are you sure you want to delete the pose '").append(name.c_str()).append("'?"),
                            QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
  {
    return;
  }

  setup_step_.removePoseByName(name, group);

  // Reload main screen table
  loadDataTable();
}

// ******************************************************************************************
// Save editing changes
// ******************************************************************************************
void RobotPosesWidget::doneEditing()
{
  // Get a reference to the supplied strings
  const std::string& name = pose_name_field_->text().toStdString();
  const std::string& group = group_name_field_->currentText().toStdString();

  // Used for editing existing groups
  srdf::Model::GroupState* searched_data = nullptr;

  // Check that name field is not empty
  if (name.empty())
  {
    QMessageBox::warning(this, "Error Saving", "A name must be given for the pose!");
    pose_name_field_->setFocus();
    return;
  }
  // Check that a group was selected
  if (group.empty())
  {
    QMessageBox::warning(this, "Error Saving", "A planning group must be chosen!");
    group_name_field_->setFocus();
    return;
  }

  // If creating a new pose, check if the (name, group) pair already exists
  if (!current_edit_pose_)
  {
    searched_data = setup_step_.findPoseByName(name, group);
    if (searched_data != current_edit_pose_)
    {
      if (QMessageBox::warning(this, "Warning Saving", "A pose already exists with that name! Overwrite?",
                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
        return;
    }
  }
  else
    searched_data = current_edit_pose_;  // overwrite edited pose

  // Save the new pose name or create the new pose ----------------------------
  bool is_new = false;

  if (searched_data == nullptr)  // create new
  {
    is_new = true;
    searched_data = new srdf::Model::GroupState();
  }

  // Copy name data ----------------------------------------------------
  searched_data->name_ = name;
  searched_data->group_ = group;

  setup_step_.setToCurrentValues(*searched_data);

  // Insert new poses into group state vector --------------------------
  if (is_new)
  {
    setup_step_.getGroupStates().push_back(*searched_data);
    delete searched_data;
  }

  // Finish up ------------------------------------------------------

  // Reload main screen table
  loadDataTable();

  // Switch to screen
  stacked_widget_->setCurrentIndex(0);

  // Announce that this widget is done with modal mode
  Q_EMIT setModalMode(false);
}

// ******************************************************************************************
// Cancel changes
// ******************************************************************************************
void RobotPosesWidget::cancelEditing()
{
  // Switch to screen
  stacked_widget_->setCurrentIndex(0);

  // Announce that this widget is done with modal mode
  Q_EMIT setModalMode(false);
}

// ******************************************************************************************
// Load the robot poses into the table
// ******************************************************************************************
void RobotPosesWidget::loadDataTable()
{
  // Disable Table
  data_table_->setUpdatesEnabled(false);  // prevent table from updating until we are completely done
  data_table_->setDisabled(true);         // make sure we disable it so that the cellChanged event is not called
  data_table_->clearContents();

  std::vector<srdf::Model::GroupState>& group_states = setup_step_.getGroupStates();

  // Set size of datatable
  data_table_->setRowCount(group_states.size());

  // Loop through every pose
  int row = 0;
  for (const auto& group_state : group_states)
  {
    // Create row elements
    QTableWidgetItem* data_name = new QTableWidgetItem(group_state.name_.c_str());
    data_name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    QTableWidgetItem* group_name = new QTableWidgetItem(group_state.group_.c_str());
    group_name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    // Add to table
    data_table_->setItem(row, 0, data_name);
    data_table_->setItem(row, 1, group_name);

    // Increment counter
    ++row;
  }

  // Re-enable
  data_table_->setUpdatesEnabled(true);  // prevent table from updating until we are completely done
  data_table_->setDisabled(false);       // make sure we disable it so that the cellChanged event is not called

  // Resize header
  data_table_->resizeColumnToContents(0);
  data_table_->resizeColumnToContents(1);

  // Show edit button if applicable
  if (!group_states.empty())
    btn_edit_->show();
}

// ******************************************************************************************
// Called when setup assistant navigation switches to this screen
// ******************************************************************************************
void RobotPosesWidget::focusGiven()
{
  setup_step_.loadAllowedCollisionMatrix();

  // Show the current poses screen
  stacked_widget_->setCurrentIndex(0);

  // Load the data to the tree
  loadDataTable();

  // Load the avail groups to the combo box
  loadGroupsComboBox();
}

// ******************************************************************************************
// Call when one of the sliders has its value changed
// ******************************************************************************************
void RobotPosesWidget::updateRobotModel(const std::string& name, double value)
{
  moveit::core::RobotState& robot_state = setup_step_.getState();
  robot_state.setVariablePosition(name, value);

  // Update the robot model/rviz
  updateStateAndCollision(robot_state);
}

void RobotPosesWidget::updateStateAndCollision(const moveit::core::RobotState& robot_state)
{
  setup_step_.publishState(robot_state);

  // if in collision, show warning
  // if no collision, hide warning
  collision_warning_->setHidden(!setup_step_.checkSelfCollision(robot_state));
}

// ******************************************************************************************
// ******************************************************************************************
// Slider Widget
// ******************************************************************************************
// ******************************************************************************************

// ******************************************************************************************
// Simple widget for adjusting joints of a robot
// ******************************************************************************************
SliderWidget::SliderWidget(QWidget* parent, const moveit::core::JointModel* joint_model, double init_value)
  : QWidget(parent), joint_model_(joint_model)
{
  // Create layouts
  QVBoxLayout* layout = new QVBoxLayout();
  QHBoxLayout* row2 = new QHBoxLayout();

  // Row 1
  joint_label_ = new QLabel(joint_model_->getName().c_str(), this);
  joint_label_->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(joint_label_);

  // Row 2 -------------------------------------------------------------
  joint_slider_ = new QSlider(Qt::Horizontal, this);
  joint_slider_->setTickPosition(QSlider::TicksBelow);
  joint_slider_->setSingleStep(10);
  joint_slider_->setPageStep(500);
  joint_slider_->setTickInterval(1000);
  joint_slider_->setContentsMargins(0, 0, 0, 0);
  row2->addWidget(joint_slider_);

  // Joint Value Box ------------------------------------------------
  joint_value_ = new QLineEdit(this);
  joint_value_->setMaximumWidth(62);
  joint_value_->setContentsMargins(0, 0, 0, 0);
  connect(joint_value_, SIGNAL(editingFinished()), this, SLOT(changeJointSlider()));
  row2->addWidget(joint_value_);

  // Joint Limits ----------------------------------------------------
  const std::vector<moveit_msgs::msg::JointLimits>& limits = joint_model_->getVariableBoundsMsg();
  if (limits.empty())
  {
    QMessageBox::critical(this, "Error Loading", "An internal error has occurred while loading the joints");
    return;
  }

  // Only use the first limit, because there is only 1 variable (as checked earlier)
  moveit_msgs::msg::JointLimits joint_limit = limits[0];
  max_position_ = joint_limit.max_position;
  min_position_ = joint_limit.min_position;

  // Set the slider limits
  joint_slider_->setMaximum(max_position_ * 10000);
  joint_slider_->setMinimum(min_position_ * 10000);

  // Connect slider to joint value box
  connect(joint_slider_, SIGNAL(valueChanged(int)), this, SLOT(changeJointValue(int)));

  // Initial joint values -------------------------------------------
  int value = init_value * 10000;           // scale double to integer for slider use
  joint_slider_->setSliderPosition(value);  // set slider value
  changeJointValue(value);                  // show in textbox

  // Finish GUI ----------------------------------------
  layout->addLayout(row2);

  setContentsMargins(0, 0, 0, 0);
  // setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  setGeometry(QRect(110, 80, 120, 80));
  setLayout(layout);

  // Declare std::string as metatype so we can use it in a signal
  qRegisterMetaType<std::string>("std::string");
}

// ******************************************************************************************
// Deconstructor
// ******************************************************************************************
SliderWidget::~SliderWidget() = default;

// ******************************************************************************************
// Called when the joint value slider is changed
// ******************************************************************************************
void SliderWidget::changeJointValue(int value)
{
  // Get joint value
  const double double_value = double(value) / 10000;

  // Set textbox
  joint_value_->setText(QString("%1").arg(double_value, 0, 'f', 4));

  // Send event to parent widget
  Q_EMIT jointValueChanged(joint_model_->getName(), double_value);
}

// ******************************************************************************************
// Called when the joint value box is changed
// ******************************************************************************************
void SliderWidget::changeJointSlider()
{
  // Get joint value
  double value = joint_value_->text().toDouble();

  if (min_position_ > value || value > max_position_)
  {
    value = (min_position_ > value) ? min_position_ : max_position_;
    joint_value_->setText(QString("%1").arg(value, 0, 'f', 4));
  }

  // We assume it converts to double because of the validator
  joint_slider_->setSliderPosition(value * 10000);

  // Send event to parent widget
  Q_EMIT jointValueChanged(joint_model_->getName(), value);
}

}  // namespace srdf_setup
}  // namespace moveit_setup

#include <pluginlib/class_list_macros.hpp>  // NOLINT
PLUGINLIB_EXPORT_CLASS(moveit_setup::srdf_setup::RobotPosesWidget, moveit_setup::SetupStepWidget)
