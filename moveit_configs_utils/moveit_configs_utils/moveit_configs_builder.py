"""Simplify loading moveit config parameters.

This module provides builder-pattern based class to simplify loading moveit related parameters found in
robot_moveit_config package generated by moveit setup assistant.

By default it expects the following structure for the moveit configs package

robot_name_moveit_config/
    .setup_assistant -> Used to retrieve information about the SRDF file and
                        the URDF file used when generating the package
    config/
        kinematics.yaml -> IK solver's parameters
        joint_limits.yaml -> Overriding position/velocity/acceleration limits from the URDF file
        moveit_cpp.yaml -> MoveItCpp related parameters
        *_planning.yaml -> planning pipelines parameters
        pilz_cartesian_limits.yaml -> Pilz planner parameters
        moveit_controllers.yaml -> trajectory execution manager's parameters
        ...

Example:
    moveit_configs = MoveItConfigsBuilder("robot_name").to_moveit_configs()
    ...
    moveit_configs.package_path
    moveit_configs.robot_description
    moveit_configs.robot_description_semantic
    moveit_configs.robot_description_kinematics
    moveit_configs.planning_pipelines
    moveit_configs.trajectory_execution
    moveit_configs.planning_scene_monitor
    moveit_configs.sensors_3d
    moveit_configs.move_group_capabilities
    moveit_configs.joint_limits
    moveit_configs.moveit_cpp
    moveit_configs.pilz_cartesian_limits
    # Or to get all the parameters as a dictionary
    moveit_configs.to_dict()

Each function in MoveItConfigsBuilder has a file_path as an argument which is used to override the default
path for the file

Example:
    moveit_configs = MoveItConfigsBuilder("robot_name")
                    # Relative to robot_name_moveit_configs
                    .robot_description_semantic(Path("my_config") / "my_file.srdf")
                    .to_moveit_configs()
    # Or
    moveit_configs = MoveItConfigsBuilder("robot_name")
                    # Absolute path to robot_name_moveit_config
                    .robot_description_semantic(Path.home() / "my_config" / "new_file.srdf")
                    .to_moveit_configs()
"""

from pathlib import Path
from typing import Optional, List, Dict
import logging
import re
from dataclasses import dataclass, field
from ament_index_python.packages import get_package_share_directory

from launch_param_builder import ParameterBuilder, load_yaml, load_xacro
from launch_param_builder.utils import ParameterBuilderFileNotFoundError
from moveit_configs_utils.substitutions import Xacro
from launch.some_substitutions_type import SomeSubstitutionsType
from launch_ros.parameter_descriptions import ParameterValue

moveit_configs_utils_path = Path(get_package_share_directory("moveit_configs_utils"))


def get_pattern_matches(folder, pattern):
    """Given all the files in the folder, find those that match the pattern.

    If there are groups defined, the groups are returned. Otherwise the path to the matches are returned.
    """
    matches = []
    if not folder.exists():
        return matches
    for child in folder.iterdir():
        if not child.is_file():
            continue
        m = pattern.search(child.name)
        if m:
            groups = m.groups()
            if groups:
                matches.append(groups[0])
            else:
                matches.append(child)
    return matches


@dataclass(slots=True)
class MoveItConfigs:
    """Class containing MoveIt related parameters."""

    # A pathlib Path to the moveit config package
    package_path: Optional[str] = None
    # A dictionary that has the contents of the URDF file.
    robot_description: Dict = field(default_factory=dict)
    # A dictionary that has the contents of the SRDF file.
    robot_description_semantic: Dict = field(default_factory=dict)
    # A dictionary IK solver specific parameters.
    robot_description_kinematics: Dict = field(default_factory=dict)
    # A dictionary that contains the planning pipelines parameters.
    planning_pipelines: Dict = field(default_factory=dict)
    # A dictionary contains parameters for trajectory execution & moveit controller managers.
    trajectory_execution: Dict = field(default_factory=dict)
    # A dictionary that has the planning scene monitor's parameters.
    planning_scene_monitor: Dict = field(default_factory=dict)
    # A dictionary that has the sensor 3d configuration parameters.
    sensors_3d: Dict = field(default_factory=dict)
    # A dictionary containing move_group's non-default capabilities.
    move_group_capabilities: Dict = field(
        default_factory=lambda: {"capabilities": "", "disable_capabilities": ""}
    )
    # A dictionary containing the overridden position/velocity/acceleration limits.
    joint_limits: Dict = field(default_factory=dict)
    # A dictionary containing MoveItCpp related parameters.
    moveit_cpp: Dict = field(default_factory=dict)
    # A dictionary containing the cartesian limits for the Pilz planner.
    pilz_cartesian_limits: Dict = field(default_factory=dict)

    def to_dict(self):
        parameters = {}
        parameters.update(self.robot_description)
        parameters.update(self.robot_description_semantic)
        parameters.update(self.robot_description_kinematics)
        parameters.update(self.planning_pipelines)
        parameters.update(self.trajectory_execution)
        parameters.update(self.planning_scene_monitor)
        parameters.update(self.sensors_3d)
        parameters.update(self.joint_limits)
        parameters.update(self.moveit_cpp)
        # Update robot_description_planning with pilz cartesian limits
        if self.pilz_cartesian_limits:
            parameters["robot_description_planning"].update(
                self.pilz_cartesian_limits["robot_description_planning"]
            )
        return parameters


class MoveItConfigsBuilder(ParameterBuilder):
    __moveit_configs: MoveItConfigs
    __robot_name: str
    __urdf_package: Path
    # Relative path of the URDF file w.r.t. __urdf_package
    __urdf_file_path: Path
    # Relative path of the SRDF file  w.r.t. robot_name_moveit_config
    __srdf_file_path: Path
    # String specify the parameter name that the robot description will be loaded to, it will also be used as a prefix
    # for "_planning", "_semantic", and "_kinematics"
    __robot_description: str
    __config_dir_path = Path("config")

    # Look-up for robot_name_moveit_config package
    def __init__(
        self,
        robot_name: str,
        robot_description="robot_description",
        package_name: Optional[str] = None,
    ):
        super().__init__(package_name or (robot_name + "_moveit_config"))
        self.__moveit_configs = MoveItConfigs(package_path=self._package_path)
        self.__robot_name = robot_name
        setup_assistant_file = self._package_path / ".setup_assistant"

        self.__urdf_package = None
        self.__urdf_file_path = None
        self.__urdf_xacro_args = None
        self.__srdf_file_path = None

        modified_urdf_path = Path("config") / (self.__robot_name + ".urdf.xacro")
        if (self._package_path / modified_urdf_path).exists():
            self.__urdf_package = self._package_path
            self.__urdf_file_path = modified_urdf_path

        if setup_assistant_file.exists():
            setup_assistant_yaml = load_yaml(setup_assistant_file)
            config = setup_assistant_yaml.get("moveit_setup_assistant_config", {})

            if urdf_config := config.get("urdf", config.get("URDF")):
                if self.__urdf_package is None:
                    self.__urdf_package = Path(
                        get_package_share_directory(urdf_config["package"])
                    )
                    self.__urdf_file_path = Path(urdf_config["relative_path"])

                if xacro_args := urdf_config.get("xacro_args"):
                    self.__urdf_xacro_args = dict(
                        arg.split(":=") for arg in xacro_args.split(" ") if arg
                    )

            if srdf_config := config.get("srdf", config.get("SRDF")):
                self.__srdf_file_path = Path(srdf_config["relative_path"])

        if not self.__urdf_package or not self.__urdf_file_path:
            logging.warning(
                f"\x1b[33;21mCannot infer URDF from `{self._package_path}`. -- using config/{robot_name}.urdf\x1b[0m"
            )
            self.__urdf_package = self._package_path
            self.__urdf_file_path = self.__config_dir_path / (
                self.__robot_name + ".urdf"
            )

        if not self.__srdf_file_path:
            logging.warning(
                f"\x1b[33;21mCannot infer SRDF from `{self._package_path}`. -- using config/{robot_name}.srdf\x1b[0m"
            )
            self.__srdf_file_path = self.__config_dir_path / (
                self.__robot_name + ".srdf"
            )

        self.__robot_description = robot_description

    def robot_description(
        self,
        file_path: Optional[str] = None,
        mappings: dict[SomeSubstitutionsType, SomeSubstitutionsType] = None,
    ):
        """Load robot description.

        :param file_path: Absolute or relative path to the URDF file (w.r.t. robot_name_moveit_config).
        :param mappings: mappings to be passed when loading the xacro file.
        :return: Instance of MoveItConfigsBuilder with robot_description loaded.
        """
        if file_path is None:
            robot_description_file_path = self.__urdf_package / self.__urdf_file_path
        else:
            robot_description_file_path = self._package_path / file_path
        if (mappings is None) or all(
            (isinstance(key, str) and isinstance(value, str))
            for key, value in mappings.items()
        ):
            try:
                self.__moveit_configs.robot_description = {
                    self.__robot_description: load_xacro(
                        robot_description_file_path,
                        mappings=mappings or self.__urdf_xacro_args,
                    )
                }
            except ParameterBuilderFileNotFoundError as e:
                logging.warning(f"\x1b[33;21m{e}\x1b[0m")
                logging.warning(
                    f"\x1b[33;21mThe robot description will be loaded from /robot_description topic \x1b[0m"
                )

        else:
            self.__moveit_configs.robot_description = {
                self.__robot_description: ParameterValue(
                    Xacro(str(robot_description_file_path), mappings=mappings),
                    value_type=str,
                )
            }
        return self

    def robot_description_semantic(
        self,
        file_path: Optional[str] = None,
        mappings: dict[SomeSubstitutionsType, SomeSubstitutionsType] = None,
    ):
        """Load semantic robot description.

        :param file_path: Absolute or relative path to the SRDF file (w.r.t. robot_name_moveit_config).
        :param mappings: mappings to be passed when loading the xacro file.
        :return: Instance of MoveItConfigsBuilder with robot_description_semantic loaded.
        """

        if (mappings is None) or all(
            (isinstance(key, str) and isinstance(value, str))
            for key, value in mappings.items()
        ):
            self.__moveit_configs.robot_description_semantic = {
                self.__robot_description
                + "_semantic": load_xacro(
                    self._package_path / (file_path or self.__srdf_file_path),
                    mappings=mappings,
                )
            }
        else:
            self.__moveit_configs.robot_description_semantic = {
                self.__robot_description
                + "_semantic": ParameterValue(
                    Xacro(
                        str(self._package_path / (file_path or self.__srdf_file_path)),
                        mappings=mappings,
                    ),
                    value_type=str,
                )
            }
        return self

    def robot_description_kinematics(self, file_path: Optional[str] = None):
        """Load IK solver parameters.

        :param file_path: Absolute or relative path to the kinematics yaml file (w.r.t. robot_name_moveit_config).
        :return: Instance of MoveItConfigsBuilder with robot_description_kinematics loaded.
        """
        self.__moveit_configs.robot_description_kinematics = {
            self.__robot_description
            + "_kinematics": load_yaml(
                self._package_path
                / (file_path or self.__config_dir_path / "kinematics.yaml")
            )
        }
        return self

    def joint_limits(self, file_path: Optional[str] = None):
        """Load joint limits overrides.

        :param file_path: Absolute or relative path to the joint limits yaml file (w.r.t. robot_name_moveit_config).
        :return: Instance of MoveItConfigsBuilder with robot_description_planning loaded.
        """
        self.__moveit_configs.joint_limits = {
            self.__robot_description
            + "_planning": load_yaml(
                self._package_path
                / (file_path or self.__config_dir_path / "joint_limits.yaml")
            )
        }
        return self

    def moveit_cpp(self, file_path: Optional[str] = None):
        """Load MoveItCpp parameters.

        :param file_path: Absolute or relative path to the MoveItCpp yaml file (w.r.t. robot_name_moveit_config).
        :return: Instance of MoveItConfigsBuilder with moveit_cpp loaded.
        """
        self.__moveit_configs.moveit_cpp = load_yaml(
            self._package_path
            / (file_path or self.__config_dir_path / "moveit_cpp.yaml")
        )
        return self

    def trajectory_execution(
        self,
        file_path: Optional[str] = None,
        moveit_manage_controllers: bool = True,
    ):
        """Load trajectory execution and moveit controller managers' parameters

        :param file_path: Absolute or relative path to the controllers yaml file (w.r.t. robot_name_moveit_config).
        :param moveit_manage_controllers: Whether trajectory execution manager is allowed to switch controllers' states.
        :return: Instance of MoveItConfigsBuilder with trajectory_execution loaded.
        """
        self.__moveit_configs.trajectory_execution = {
            "moveit_manage_controllers": moveit_manage_controllers,
        }

        # Find the most likely controller params as needed
        if file_path is None:
            config_folder = self._package_path / self.__config_dir_path
            controller_pattern = re.compile("^(.*)_controllers.yaml$")
            possible_names = get_pattern_matches(config_folder, controller_pattern)
            if not possible_names:
                # Warn the user instead of raising exception
                logging.warning(
                    "\x1b[33;20mtrajectory_execution: `Parameter file_path is undefined "
                    f"and no matches for {config_folder}/*_controllers.yaml\x1b[0m"
                )
            else:
                chosen_name = None
                if len(possible_names) == 1:
                    chosen_name = possible_names[0]
                else:
                    # Try a couple other common names, in order of precedence
                    for name in ["moveit", "moveit2", self.__robot_name]:
                        if name in possible_names:
                            chosen_name = name
                            break
                    else:
                        option_str = "\n - ".join(
                            name + "_controllers.yaml" for name in possible_names
                        )
                        raise RuntimeError(
                            "trajectory_execution: "
                            f"Unable to guess which parameter file to load. Options:\n - {option_str}"
                        )
                file_path = config_folder / (chosen_name + "_controllers.yaml")

        else:
            file_path = self._package_path / file_path

        if file_path:
            self.__moveit_configs.trajectory_execution.update(load_yaml(file_path))
        return self

    def planning_scene_monitor(
        self,
        publish_planning_scene: bool = True,
        publish_geometry_updates: bool = True,
        publish_state_updates: bool = True,
        publish_transforms_updates: bool = True,
        publish_robot_description: bool = False,
        publish_robot_description_semantic: bool = False,
    ):
        self.__moveit_configs.planning_scene_monitor = {
            # TODO: Fix parameter namespace upstream -- see planning_scene_monitor.cpp:262
            # "planning_scene_monitor": {
            "publish_planning_scene": publish_planning_scene,
            "publish_geometry_updates": publish_geometry_updates,
            "publish_state_updates": publish_state_updates,
            "publish_transforms_updates": publish_transforms_updates,
            "publish_robot_description": publish_robot_description,
            "publish_robot_description_semantic": publish_robot_description_semantic,
            # }
        }
        return self

    def sensors_3d(self, file_path: Optional[str] = None):
        """Load sensors_3d parameters.

        :param file_path: Absolute or relative path to the sensors_3d yaml file (w.r.t. robot_name_moveit_config).
        :return: Instance of MoveItConfigsBuilder with robot_description_planning loaded.
        """
        sensors_path = self._package_path / (
            file_path or self.__config_dir_path / "sensors_3d.yaml"
        )
        if sensors_path.exists():
            sensors_data = load_yaml(sensors_path)
            # TODO(mikeferguson): remove the second part of this check once
            # https://github.com/ros-planning/moveit_resources/pull/141 has made through buildfarm
            if len(sensors_data["sensors"]) > 0 and sensors_data["sensors"][0]:
                self.__moveit_configs.sensors_3d = sensors_data
        return self

    def planning_pipelines(
        self,
        default_planning_pipeline: str = None,
        pipelines: List[str] = None,
        load_all: bool = True,
    ):
        """Load planning pipelines parameters.

        :param default_planning_pipeline: Name of the default planning pipeline.
        :param pipelines: List of the planning pipelines to be loaded.
        :param load_all: Only used if pipelines is None.
                         If true, loads all pipelines defined in config package AND this package.
                         If false, only loads the pipelines defined in config package.
        :return: Instance of MoveItConfigsBuilder with planning_pipelines loaded.
        """
        config_folder = self._package_path / self.__config_dir_path
        default_folder = moveit_configs_utils_path / "default_configs"

        # If no pipelines are specified, search by filename
        if pipelines is None:
            planning_pattern = re.compile("^(.*)_planning.yaml$")
            pipelines = get_pattern_matches(config_folder, planning_pattern)
            if load_all:
                for pipeline in get_pattern_matches(default_folder, planning_pattern):
                    if pipeline not in pipelines:
                        pipelines.append(pipeline)

        # Define default pipeline as needed
        if not default_planning_pipeline:
            if "ompl" in pipelines:
                default_planning_pipeline = "ompl"
            else:
                default_planning_pipeline = pipelines[0]

        if default_planning_pipeline not in pipelines:
            raise RuntimeError(
                f"default_planning_pipeline: `{default_planning_pipeline}` doesn't name any of the input pipelines "
                f"`{','.join(pipelines)}`"
            )
        self.__moveit_configs.planning_pipelines = {
            "planning_pipelines": pipelines,
            "default_planning_pipeline": default_planning_pipeline,
        }
        for pipeline in pipelines:
            parameter_file = config_folder / (pipeline + "_planning.yaml")
            if not parameter_file.exists():
                parameter_file = default_folder / (pipeline + "_planning.yaml")
            self.__moveit_configs.planning_pipelines[pipeline] = load_yaml(
                parameter_file
            )

        # Special rule to add ompl planner_configs
        if "ompl" in self.__moveit_configs.planning_pipelines:
            ompl_config = self.__moveit_configs.planning_pipelines["ompl"]
            if "planner_configs" not in ompl_config:
                ompl_config.update(load_yaml(default_folder / "ompl_defaults.yaml"))

        return self

    def pilz_cartesian_limits(self, file_path: Optional[str] = None):
        """Load cartesian limits.

        :param file_path: Absolute or relative path to the cartesian limits file (w.r.t. robot_name_moveit_config).
        :return: Instance of MoveItConfigsBuilder with pilz_cartesian_limits loaded.
        """
        deprecated_path = self._package_path / (
            self.__config_dir_path / "cartesian_limits.yaml"
        )
        if deprecated_path.exists():
            logging.warning(
                f"\x1b[33;21mcartesian_limits.yaml is deprecated, please rename to pilz_cartesian_limits.yaml\x1b[0m"
            )

        self.__moveit_configs.pilz_cartesian_limits = {
            self.__robot_description
            + "_planning": load_yaml(
                self._package_path
                / (file_path or self.__config_dir_path / "pilz_cartesian_limits.yaml")
            )
        }
        return self

    def to_moveit_configs(self):
        """Get MoveIt configs from robot_name_moveit_config.

        :return: An MoveItConfigs instance with all parameters loaded.
        """
        if not self.__moveit_configs.robot_description:
            self.robot_description()
        if not self.__moveit_configs.robot_description_semantic:
            self.robot_description_semantic()
        if not self.__moveit_configs.robot_description_kinematics:
            self.robot_description_kinematics()
        if not self.__moveit_configs.planning_pipelines:
            self.planning_pipelines()
        if not self.__moveit_configs.trajectory_execution:
            self.trajectory_execution()
        if not self.__moveit_configs.planning_scene_monitor:
            self.planning_scene_monitor()
        if not self.__moveit_configs.sensors_3d:
            self.sensors_3d()
        if not self.__moveit_configs.joint_limits:
            self.joint_limits()
        # TODO(JafarAbdi): We should have a default moveit_cpp.yaml as port of a moveit config package
        # if not self.__moveit_configs.moveit_cpp:
        #     self.moveit_cpp()
        if "pilz_industrial_motion_planner" in self.__moveit_configs.planning_pipelines:
            if not self.__moveit_configs.pilz_cartesian_limits:
                self.pilz_cartesian_limits()
        return self.__moveit_configs

    def to_dict(self, include_moveit_configs: bool = True):
        """Get loaded parameters from robot_name_moveit_config as a dictionary.

        :param include_moveit_configs: Whether to include the MoveIt config parameters or
                                       only the ones from ParameterBuilder
        :return: Dictionary with all parameters loaded.
        """
        parameters = self._parameters
        if include_moveit_configs:
            parameters.update(self.to_moveit_configs().to_dict())
        return parameters
