//
// Created by nicola on 29/08/23.
//

#include <valarray>
#include "coordination.h"
#include <cmath>

using evacuation::Pose;
using std::vector;

namespace coordination {

    PoseForCoordination::PoseForCoordination() = default;

    PoseForCoordination::PoseForCoordination(double x, double y, double th, double distanceFromInitial) : pose(Pose(Point(x, y), th)), distanceFromInitial(distanceFromInitial) {}

    PoseForCoordination::PoseForCoordination(evacuation::Pose pose, double distanceFromInitial) : pose(pose), distanceFromInitial(distanceFromInitial) {}

    PoseForCoordination::getPose() const {
        return pose;
    }

    geometry_msgs::msg::PoseStamped Pose::toPoseStamped(rclcpp::Time time, std::string frameId) const {
        geometry_msgs::msg::PoseStamped stamped;
        stamped.header.stamp = time;
        stamped.header.frame_id = std::move(frameId);
        stamped.pose.position.x = position.x;
        stamped.pose.position.y = position.y;
        stamped.pose.position.z = 0.0;
        stamped.pose.orientation.x = 0.0;
        stamped.pose.orientation.y = 0.0;
        stamped.pose.orientation.z = th;
        stamped.pose.orientation.w = 1.0;
        return stamped;
    }

    vector<RobotCoordination> getPathsWithoutRobotCollisions(vector<PoseForCoordination> path1, vector<PoseForCoordinationPoseForCoordination> path2, vector<PoseForCoordination> path3, double robotRadius) {
        double timeToWait = 1.0;
        std::vector<std::vector<PoseForCoordination>> intersectionRobot1Robot2;
        std::vector<std::vector<PoseForCoordination>> intersectionRobot1Robot3;
        std::vector<std::vector<PoseForCoordination>> intersectionRobot2Robot3;

        for(PoseForCoordination pose1: path1) {
            for(PoseForCoordination pose2: path1) {
                if(intersect(pose1.getPose(), pose2.getPose(), robotRadius)) {
                    intersectionRobot1Robot2.push_back({pose1, pose2});
                }
            }
        }

        for(PoseForCoordination pose2: path2) {
            for(PoseForCoordination pose3: path3) {
                if(intersect(pose2.getPose(), pose3.getPose(), robotRadius)) {
                    intersectionRobot2Robot3.push_back({pose2, pose3});
                }
            }
        }

        for(PoseForCoordination pose1: path1) {
            for(PoseForCoordination pose3: path3) {
                if(intersect(pose1.getPose(), pose3.getPose(), robotRadius)) {
                    intersectionRobot1Robot3.push_back({pose1, pose3});
                }
            }
        }

        bool robot1Robot2CollisionChecked = false;
        bool robot1Robot3CollisionChecked = false;
        bool robot2Robot3CollisionChecked = false;
        double robot1WaitTime = 0.0;
        double robot2WaitTime = 0.0;
        double robot3WaitTime = 0.0;
        bool collisionTimesFound = false;
        while(!collisionTimesFound) {
            if(!robot1Robot2CollisionChecked) {
                collisions = true;
                while(collisions) {
                    bool broken = false;
                    for(std::vector<PoseForCoordination> intersection: intersectionRobot1Robot2) {
                        if (intersectAtSameTime(intersection[0], intersection[1], robot1WaitTime, robot2WaitTime)) {
                            robot2WaitTime += timeToWait;
                            robot2Robot3CollisionChecked = false;
                            broken = true;
                            break;
                        }
                    }
                    if(!broken) {
                        collisions = false;
                        robot1Robot2CollisionChecked = true;
                    }
                }
            }
            if(!robot1Robot3CollisionChecked) {
                collisions = true;
                while(collisions) {
                    bool broken = false;
                    for(std::vector<PoseForCoordination> intersection: intersectionRobot1Robot3) {
                        if (intersectAtSameTime(intersection[0], intersection[1], robot1WaitTime, robot3WaitTime)) {
                            robot3WaitTime += timeToWait;
                            robot2Robot3CollisionChecked = false;
                            broken = true;
                            break;
                        }
                    }
                    if(!broken) {
                        collisions = false;
                        robot1Robot3CollisionChecked = true;
                    }
                }
            }
            if(!robot2Robot3CollisionChecked) {
                collisions = true;
                while(collisions) {
                    bool broken = false;
                    for(std::vector<PoseForCoordination> intersection: intersectionRobot2Robot3) {
                        if (intersectAtSameTime(intersection[0], intersection[1], robot2WaitTime, robot3WaitTime)) {
                            robot3WaitTime += timeToWait;
                            robot1Robot3CollisionChecked = false;
                            broken = true;
                            break;
                        }
                    }
                    if(!broken) {
                        collisions = false;
                        robot2Robot3CollisionChecked = true;
                    }
                }
            }
            if(robot1Robot2CollisionChecked && robot1Robot3CollisionChecked && robot2Robot3CollisionChecked) {
                collisionTimesFound = true;
            }
        }
        std::vector<Pose> path1Poses;
        for(PoseForCoordination pose: path1) {
            path1Poses.push_back(pose.getPose());
        }
        RobotCoordination result1 = RobotCoordination(path1Poses, robot1WaitTime);
        std::vector<Pose> path2Poses;
        for(PoseForCoordination pose: path2) {
            path2Poses.push_back(pose.getPose());
        }
        RobotCoordination result2 = RobotCoordination(path2Poses, robot2WaitTime);
        std::vector<Pose> path3Poses;
        for(PoseForCoordination pose: path3) {
            path3Poses.push_back(pose.getPose());
        }
        RobotCoordination result3 = RobotCoordination(path3Poses, robot3WaitTime);
        return {result1, result2, result3};
    }

    bool intersect(Pose p1, Pose p2, double robotRadius) {
        // (R0 - R1)^2 <= (x0 - x1)^2 + (y0 - y1)^2 <= (R0 + R1)^2
        double val = std::pow((p1.position.x - p2.position.x), 2) + std::pow((p1.position.y - p2.position.y), 2);
        return val >= 0 && val <= std::pow((robotRadius * 2), 2);
    }

    bool intersectAtSameTime(PoseForCoordination p1, PoseForCoordination p2, double robot1WaitTime, double robot2WaitTime) {
        double robotSize = 0.5;
        double offset = 0.1;
        double spaceWait = 0.3;
        bool result = false;
        double p1WithTime = p1.distanceFromInitial + robot1WaitTime * spaceWait;
        double p2WithTime = p2.distanceFromInitial + robot2WaitTime * spaceWait;
        if(std::abs(p1WithTime - p2WithTime) <= robotSize + offset) {
            result = true;
        }
        return result;
    }
}
