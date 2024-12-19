/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2019, Jens Petit
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
 *   * Neither the name of the copyright holder nor the names of its
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

/* Author: Jens Petit */

#include <moveit/collision_detection_fcl/collision_detector_allocator_fcl.hpp>
#include <moveit/collision_detection/test_collision_common_panda.hpp>

INSTANTIATE_TYPED_TEST_SUITE_P(FCLCollisionCheckPanda, CollisionDetectorPandaTest,
                               collision_detection::CollisionDetectorAllocatorFCL);

// FCL < 0.6 incorrectly reports distance results in the object coordinate frame.
// See: https://github.com/flexible-collision-library/fcl/issues/171
//  and https://github.com/flexible-collision-library/fcl/pull/288.
// So only execute the full distance test suite on FCL >= 0.6.
#if MOVEIT_FCL_VERSION >= FCL_VERSION_CHECK(0, 6, 0)
INSTANTIATE_TYPED_TEST_SUITE_P(FCLDistanceCheckPanda, DistanceFullPandaTest,
                               collision_detection::CollisionDetectorAllocatorFCL);
#ifdef GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DistanceCheckPandaTest);
#endif
#else
INSTANTIATE_TYPED_TEST_SUITE_P(FCLDistanceCheckPanda, DistanceCheckPandaTest,
                               collision_detection::CollisionDetectorAllocatorFCL);
#ifdef GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DistanceFullPandaTest);
#endif
#endif

int main(int argc, char* argv[])
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
