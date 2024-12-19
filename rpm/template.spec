%bcond_without tests
%bcond_without weak_deps

%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')
%global __provides_exclude_from ^/opt/ros/jazzy/.*$
%global __requires_exclude_from ^/opt/ros/jazzy/.*$

Name:           ros-jazzy-moveit-ros-visualization
Version:        2.12.1
Release:        1%{?dist}%{?release_suffix}
Summary:        ROS moveit_ros_visualization package

License:        BSD-3-Clause
URL:            http://moveit.ros.org
Source0:        %{name}-%{version}.tar.gz

Requires:       ros-jazzy-geometric-shapes
Requires:       ros-jazzy-interactive-markers
Requires:       ros-jazzy-moveit-common
Requires:       ros-jazzy-moveit-ros-planning-interface
Requires:       ros-jazzy-moveit-ros-robot-interaction
Requires:       ros-jazzy-moveit-ros-warehouse
Requires:       ros-jazzy-object-recognition-msgs
Requires:       ros-jazzy-pluginlib >= 1.11.2
Requires:       ros-jazzy-rclcpp
Requires:       ros-jazzy-rclpy
Requires:       ros-jazzy-rviz2
Requires:       ros-jazzy-tf2-eigen
Requires:       ros-jazzy-ros-workspace
BuildRequires:  eigen3-devel
BuildRequires:  pkgconfig
BuildRequires:  qt5-qtbase-devel
BuildRequires:  ros-jazzy-ament-cmake
BuildRequires:  ros-jazzy-class-loader
BuildRequires:  ros-jazzy-geometric-shapes
BuildRequires:  ros-jazzy-interactive-markers
BuildRequires:  ros-jazzy-moveit-common
BuildRequires:  ros-jazzy-moveit-ros-planning-interface
BuildRequires:  ros-jazzy-moveit-ros-robot-interaction
BuildRequires:  ros-jazzy-moveit-ros-warehouse
BuildRequires:  ros-jazzy-object-recognition-msgs
BuildRequires:  ros-jazzy-pluginlib >= 1.11.2
BuildRequires:  ros-jazzy-rclcpp
BuildRequires:  ros-jazzy-rclpy
BuildRequires:  ros-jazzy-rviz2
BuildRequires:  ros-jazzy-tf2-eigen
BuildRequires:  ros-jazzy-ros-workspace
Provides:       %{name}-devel = %{version}-%{release}
Provides:       %{name}-doc = %{version}-%{release}
Provides:       %{name}-runtime = %{version}-%{release}

%description
Components of MoveIt that offer visualization

%prep
%autosetup -p1

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/jazzy/setup.sh" ]; then . "/opt/ros/jazzy/setup.sh"; fi
mkdir -p .obj-%{_target_platform} && cd .obj-%{_target_platform}
%cmake3 \
    -UINCLUDE_INSTALL_DIR \
    -ULIB_INSTALL_DIR \
    -USYSCONF_INSTALL_DIR \
    -USHARE_INSTALL_PREFIX \
    -ULIB_SUFFIX \
    -DCMAKE_INSTALL_PREFIX="/opt/ros/jazzy" \
    -DAMENT_PREFIX_PATH="/opt/ros/jazzy" \
    -DCMAKE_PREFIX_PATH="/opt/ros/jazzy" \
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
if [ -f "/opt/ros/jazzy/setup.sh" ]; then . "/opt/ros/jazzy/setup.sh"; fi
%make_install -C .obj-%{_target_platform}

%if 0%{?with_tests}
%check
# Look for a Makefile target with a name indicating that it runs tests
TEST_TARGET=$(%__make -qp -C .obj-%{_target_platform} | sed "s/^\(test\|check\):.*/\\1/;t f;d;:f;q0")
if [ -n "$TEST_TARGET" ]; then
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/jazzy/setup.sh" ]; then . "/opt/ros/jazzy/setup.sh"; fi
CTEST_OUTPUT_ON_FAILURE=1 \
    %make_build -C .obj-%{_target_platform} $TEST_TARGET || echo "RPM TESTS FAILED"
else echo "RPM TESTS SKIPPED"; fi
%endif

%files
/opt/ros/jazzy

%changelog
* Thu Dec 19 2024 Henning Kayser <henningkayser@picknik.ai> - 2.12.1-1
- Autogenerated by Bloom

* Sun Dec 15 2024 Henning Kayser <henningkayser@picknik.ai> - 2.12.0-1
- Autogenerated by Bloom

* Wed Jun 19 2024 Henning Kayser <henningkayser@picknik.ai> - 2.10.0-2
- Autogenerated by Bloom

* Fri Jun 14 2024 Henning Kayser <henningkayser@picknik.ai> - 2.10.0-1
- Autogenerated by Bloom

* Mon Apr 22 2024 Henning Kayser <henningkayser@picknik.ai> - 2.9.0-1
- Autogenerated by Bloom

