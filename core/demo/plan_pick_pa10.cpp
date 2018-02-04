#include <moveit/task_constructor/task.h>

#include <moveit/task_constructor/stages/current_state.h>
#include <moveit/task_constructor/stages/gripper.h>
#include <moveit/task_constructor/stages/move.h>
#include <moveit/task_constructor/stages/generate_grasp_pose.h>
#include <moveit/task_constructor/stages/cartesian_position_motion.h>

#include <ros/ros.h>

#include <moveit/planning_scene_interface/planning_scene_interface.h>

using namespace moveit::task_constructor;

void spawnObject(){
	moveit::planning_interface::PlanningSceneInterface psi;

	moveit_msgs::CollisionObject o;
	o.id= "object";
	o.header.frame_id= "world";
	o.primitive_poses.resize(1);
	o.primitive_poses[0].position.x= 0.3;
	o.primitive_poses[0].position.y= 0.23;
	o.primitive_poses[0].position.z= 0.12;
	o.primitive_poses[0].orientation.w= 1.0;
	o.primitives.resize(1);
	o.primitives[0].type= shape_msgs::SolidPrimitive::CYLINDER;
	o.primitives[0].dimensions.resize(2);
	o.primitives[0].dimensions[0]= 0.23;
	o.primitives[0].dimensions[1]= 0.03;
	psi.applyCollisionObject(o);
}

int main(int argc, char** argv){
	ros::init(argc, argv, "plan_pick");
	ros::AsyncSpinner spinner(1);
	spinner.start();

	spawnObject();

	Task t;
	// define global properties used by most stages
	t.setProperty("group", std::string("left_arm"));
	t.setProperty("eef", std::string("la_tool_mount"));
	t.setProperty("planner", std::string("RRTConnectkConfigDefault"));
	t.setProperty("link", std::string("la_tool_mount"));

	t.add(std::make_unique<stages::CurrentState>("current state"));

	{
		auto move = std::make_unique<stages::Gripper>("open gripper");
		move->properties().configureInitFrom(Stage::PARENT);
		move->setTo("open");
		t.add(std::move(move));
	}

	{
		auto move = std::make_unique<stages::Move>("move to pre-grasp");
		move->properties().configureInitFrom(Stage::PARENT);
		move->setTimeout(8.0);
		t.add(std::move(move));
	}

	{
		auto move = std::make_unique<stages::CartesianPositionMotion>("proceed to grasp pose");
		move->properties().configureInitFrom(Stage::PARENT);
		move->setMinMaxDistance(.05, 0.1);
		move->setCartesianStepSize(0.02);

		geometry_msgs::PointStamped target;
		target.header.frame_id= "object";
		move->towards(target);
		t.add(std::move(move));
	}

	{
		auto gengrasp = std::make_unique<stages::GenerateGraspPose>("generate grasp pose");
		gengrasp->properties().configureInitFrom(Stage::PARENT);
		gengrasp->setGripperGraspPose("open");
		gengrasp->setObject("object");
		gengrasp->setGraspFrame(Eigen::Translation3d(0,0,.05)*
		                        Eigen::AngleAxisd(-0.5*M_PI, Eigen::Vector3d::UnitY()),
		                        "lh_tool_frame");
		gengrasp->setAngleDelta(-.2);
		gengrasp->setMaxIKSolutions(1);
		t.add(std::move(gengrasp));
	}

	{
		auto move = std::make_unique<stages::Gripper>("grasp");
		move->properties().configureInitFrom(Stage::PARENT);
		move->setTo("closed");
		move->graspObject("object");
		t.add(std::move(move));
	}

	{
		auto move = std::make_unique<stages::CartesianPositionMotion>("lift object");
		move->properties().configureInitFrom(Stage::PARENT);
		move->setMinMaxDistance(0.03, 0.05);
		move->setCartesianStepSize(0.01);

		geometry_msgs::Vector3Stamped direction;
		direction.header.frame_id= "world";
		direction.vector.z= 1.0;
		move->along(direction);
		t.add(std::move(move));
	}

	t.plan();
	t.publishAllSolutions();

	return 0;
}
