%bcond_without tests
%bcond_without weak_deps

%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')
%global __provides_exclude_from ^/opt/ros/iron/.*$
%global __requires_exclude_from ^/opt/ros/iron/.*$

Name:           ros-iron-moveit-core
Version:        2.7.3
Release:        1%{?dist}%{?release_suffix}
Summary:        ROS moveit_core package

License:        BSD-3-Clause
URL:            http://moveit.ros.org
Source0:        %{name}-%{version}.tar.gz

Requires:       assimp-devel
Requires:       boost-devel
Requires:       boost-python%{python3_pkgversion}-devel
Requires:       bullet-devel
Requires:       eigen3-devel
Requires:       fcl-devel
Requires:       ros-iron-angles
Requires:       ros-iron-common-interfaces
Requires:       ros-iron-eigen-stl-containers
Requires:       ros-iron-eigen3-cmake-module
Requires:       ros-iron-generate-parameter-library
Requires:       ros-iron-geometric-shapes
Requires:       ros-iron-geometry-msgs
Requires:       ros-iron-kdl-parser
Requires:       ros-iron-moveit-common
Requires:       ros-iron-moveit-msgs
Requires:       ros-iron-octomap
Requires:       ros-iron-octomap-msgs
Requires:       ros-iron-pluginlib
Requires:       ros-iron-pybind11-vendor
Requires:       ros-iron-random-numbers
Requires:       ros-iron-rclcpp
Requires:       ros-iron-ruckig
Requires:       ros-iron-sensor-msgs
Requires:       ros-iron-shape-msgs
Requires:       ros-iron-srdfdom
Requires:       ros-iron-std-msgs
Requires:       ros-iron-tf2
Requires:       ros-iron-tf2-eigen
Requires:       ros-iron-tf2-geometry-msgs
Requires:       ros-iron-tf2-kdl
Requires:       ros-iron-trajectory-msgs
Requires:       ros-iron-urdf
Requires:       ros-iron-urdfdom
Requires:       ros-iron-urdfdom-headers
Requires:       ros-iron-visualization-msgs
Requires:       ros-iron-ros-workspace
BuildRequires:  assimp-devel
BuildRequires:  boost-devel
BuildRequires:  boost-python%{python3_pkgversion}-devel
BuildRequires:  bullet-devel
BuildRequires:  eigen3-devel
BuildRequires:  fcl-devel
BuildRequires:  pkgconfig
BuildRequires:  ros-iron-ament-cmake
BuildRequires:  ros-iron-angles
BuildRequires:  ros-iron-common-interfaces
BuildRequires:  ros-iron-eigen-stl-containers
BuildRequires:  ros-iron-eigen3-cmake-module
BuildRequires:  ros-iron-generate-parameter-library
BuildRequires:  ros-iron-geometric-shapes
BuildRequires:  ros-iron-geometry-msgs
BuildRequires:  ros-iron-kdl-parser
BuildRequires:  ros-iron-moveit-common
BuildRequires:  ros-iron-moveit-msgs
BuildRequires:  ros-iron-octomap
BuildRequires:  ros-iron-octomap-msgs
BuildRequires:  ros-iron-pluginlib
BuildRequires:  ros-iron-pybind11-vendor
BuildRequires:  ros-iron-random-numbers
BuildRequires:  ros-iron-rclcpp
BuildRequires:  ros-iron-ruckig
BuildRequires:  ros-iron-sensor-msgs
BuildRequires:  ros-iron-shape-msgs
BuildRequires:  ros-iron-srdfdom
BuildRequires:  ros-iron-std-msgs
BuildRequires:  ros-iron-tf2
BuildRequires:  ros-iron-tf2-eigen
BuildRequires:  ros-iron-tf2-geometry-msgs
BuildRequires:  ros-iron-tf2-kdl
BuildRequires:  ros-iron-trajectory-msgs
BuildRequires:  ros-iron-urdf
BuildRequires:  ros-iron-urdfdom
BuildRequires:  ros-iron-urdfdom-headers
BuildRequires:  ros-iron-visualization-msgs
BuildRequires:  ros-iron-ros-workspace
Provides:       %{name}-devel = %{version}-%{release}
Provides:       %{name}-doc = %{version}-%{release}
Provides:       %{name}-runtime = %{version}-%{release}

%if 0%{?with_tests}
BuildRequires:  ros-iron-ament-cmake-gmock
BuildRequires:  ros-iron-ament-cmake-gtest
BuildRequires:  ros-iron-ament-index-cpp
BuildRequires:  ros-iron-ament-lint-auto
BuildRequires:  ros-iron-ament-lint-common
BuildRequires:  ros-iron-moveit-resources-panda-moveit-config
BuildRequires:  ros-iron-moveit-resources-pr2-description
BuildRequires:  ros-iron-orocos-kdl-vendor
%endif

%description
Core libraries used by MoveIt

%prep
%autosetup -p1

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/iron/setup.sh" ]; then . "/opt/ros/iron/setup.sh"; fi
mkdir -p .obj-%{_target_platform} && cd .obj-%{_target_platform}
%cmake3 \
    -UINCLUDE_INSTALL_DIR \
    -ULIB_INSTALL_DIR \
    -USYSCONF_INSTALL_DIR \
    -USHARE_INSTALL_PREFIX \
    -ULIB_SUFFIX \
    -DCMAKE_INSTALL_PREFIX="/opt/ros/iron" \
    -DAMENT_PREFIX_PATH="/opt/ros/iron" \
    -DCMAKE_PREFIX_PATH="/opt/ros/iron" \
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
if [ -f "/opt/ros/iron/setup.sh" ]; then . "/opt/ros/iron/setup.sh"; fi
%make_install -C .obj-%{_target_platform}

%if 0%{?with_tests}
%check
# Look for a Makefile target with a name indicating that it runs tests
TEST_TARGET=$(%__make -qp -C .obj-%{_target_platform} | sed "s/^\(test\|check\):.*/\\1/;t f;d;:f;q0")
if [ -n "$TEST_TARGET" ]; then
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree and source it.  It will set things like
# CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/iron/setup.sh" ]; then . "/opt/ros/iron/setup.sh"; fi
CTEST_OUTPUT_ON_FAILURE=1 \
    %make_build -C .obj-%{_target_platform} $TEST_TARGET || echo "RPM TESTS FAILED"
else echo "RPM TESTS SKIPPED"; fi
%endif

%files
/opt/ros/iron

%changelog
* Mon Apr 24 2023 Henning Kayser <henningkayser@picknik.ai> - 2.7.3-1
- Autogenerated by Bloom

* Thu Apr 20 2023 Henning Kayser <henningkayser@picknik.ai> - 2.7.2-2
- Autogenerated by Bloom

* Tue Apr 18 2023 Henning Kayser <henningkayser@picknik.ai> - 2.7.2-1
- Autogenerated by Bloom

* Thu Mar 23 2023 Henning Kayser <henningkayser@picknik.ai> - 2.7.1-1
- Autogenerated by Bloom

* Tue Mar 21 2023 Henning Kayser <henningkayser@picknik.ai> - 2.7.0-2
- Autogenerated by Bloom

