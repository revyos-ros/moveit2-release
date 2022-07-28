%bcond_without tests
%bcond_without weak_deps

%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')
%global __provides_exclude_from ^/opt/ros/humble/.*$
%global __requires_exclude_from ^/opt/ros/humble/.*$

Name:           ros-humble-moveit-ros-occupancy-map-monitor
Version:        2.5.3
Release:        1%{?dist}%{?release_suffix}
Summary:        ROS moveit_ros_occupancy_map_monitor package

License:        BSD
URL:            http://moveit.ros.org
Source0:        %{name}-%{version}.tar.gz

Requires:       eigen3-devel
Requires:       ros-humble-eigen3-cmake-module
Requires:       ros-humble-geometric-shapes
Requires:       ros-humble-moveit-common
Requires:       ros-humble-moveit-core
Requires:       ros-humble-moveit-msgs
Requires:       ros-humble-octomap
Requires:       ros-humble-pluginlib >= 1.11.2
Requires:       ros-humble-rclcpp
Requires:       ros-humble-tf2-ros
Requires:       ros-humble-ros-workspace
BuildRequires:  eigen3-devel
BuildRequires:  ros-humble-ament-cmake
BuildRequires:  ros-humble-eigen3-cmake-module
BuildRequires:  ros-humble-geometric-shapes
BuildRequires:  ros-humble-moveit-common
BuildRequires:  ros-humble-moveit-core
BuildRequires:  ros-humble-moveit-msgs
BuildRequires:  ros-humble-octomap
BuildRequires:  ros-humble-pluginlib >= 1.11.2
BuildRequires:  ros-humble-rclcpp
BuildRequires:  ros-humble-tf2-ros
BuildRequires:  ros-humble-ros-workspace
Provides:       %{name}-devel = %{version}-%{release}
Provides:       %{name}-doc = %{version}-%{release}
Provides:       %{name}-runtime = %{version}-%{release}

%if 0%{?with_tests}
BuildRequires:  ros-humble-ament-cmake-gmock
BuildRequires:  ros-humble-ament-lint-auto
BuildRequires:  ros-humble-ament-lint-common
%endif

%description
Components of MoveIt connecting to occupancy map

%prep
%autosetup -p1

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/humble/setup.sh" ]; then . "/opt/ros/humble/setup.sh"; fi
mkdir -p .obj-%{_target_platform} && cd .obj-%{_target_platform}
%cmake3 \
    -UINCLUDE_INSTALL_DIR \
    -ULIB_INSTALL_DIR \
    -USYSCONF_INSTALL_DIR \
    -USHARE_INSTALL_PREFIX \
    -ULIB_SUFFIX \
    -DCMAKE_INSTALL_PREFIX="/opt/ros/humble" \
    -DAMENT_PREFIX_PATH="/opt/ros/humble" \
    -DCMAKE_PREFIX_PATH="/opt/ros/humble" \
    -DSETUPTOOLS_DEB_LAYOUT=OFF \
%if !0%{?with_tests}
    -DBUILD_TESTING=OFF \
%endif
    ..

%make_build

%install
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/humble/setup.sh" ]; then . "/opt/ros/humble/setup.sh"; fi
%make_install -C .obj-%{_target_platform}

%if 0%{?with_tests}
%check
# Look for a Makefile target with a name indicating that it runs tests
TEST_TARGET=$(%__make -qp -C .obj-%{_target_platform} | sed "s/^\(test\|check\):.*/\\1/;t f;d;:f;q0")
if [ -n "$TEST_TARGET" ]; then
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/humble/setup.sh" ]; then . "/opt/ros/humble/setup.sh"; fi
CTEST_OUTPUT_ON_FAILURE=1 \
    %make_build -C .obj-%{_target_platform} $TEST_TARGET || echo "RPM TESTS FAILED"
else echo "RPM TESTS SKIPPED"; fi
%endif

%files
/opt/ros/humble

%changelog
* Fri Jul 29 2022 Henning Kayser <henningkayser@picknik.ai> - 2.5.3-1
- Autogenerated by Bloom

* Mon Jul 25 2022 Henning Kayser <henningkayser@picknik.ai> - 2.5.2-1
- Autogenerated by Bloom

* Wed Jun 01 2022 Henning Kayser <henningkayser@picknik.ai> - 2.5.1-1
- Autogenerated by Bloom

* Thu May 26 2022 Henning Kayser <henningkayser@picknik.ai> - 2.5.0-1
- Autogenerated by Bloom

