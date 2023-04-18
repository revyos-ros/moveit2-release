from typing import Any

class RobotTrajectory:
    joint_model_group_name: Any
    def __init__(self, *args, **kwargs) -> None: ...
    def get_robot_trajectory_msg(self, *args, **kwargs) -> Any: ...
    def get_waypoint_durations(self, *args, **kwargs) -> Any: ...
    def unwind(self, *args, **kwargs) -> Any: ...
    def __getitem__(self, index) -> Any: ...
    def __iter__(self) -> Any: ...
    def __len__(self) -> Any: ...
    def __reverse__(self, *args, **kwargs) -> Any: ...
    @property
    def average_segment_duration(self) -> Any: ...
    @property
    def duration(self) -> Any: ...
    @property
    def robot_model(self) -> Any: ...
