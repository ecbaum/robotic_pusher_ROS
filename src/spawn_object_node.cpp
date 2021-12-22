#include "robotic_pusher/spawnObject.h"
#include "ros/ros.h"

bool spawn(robotic_pusher::spawnObject::Request &req,
           robotic_pusher::spawnObject::Response &res) {

    std::string model_name = req.model_name;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    res.reply = 0;
    if (ros::param::has("/spawn_position/x")) {
        ros::param::get("/spawn_position/x", x);
    } else {
        ROS_ERROR_STREAM("param: /spawn_position/x, doesn't exist");
        return true;
    }

    if (ros::param::has("/spawn_position/y")) {
        ros::param::get("/spawn_position/y", y);
    } else {
        ROS_ERROR_STREAM("param: /spawn_position/y, doesn't exist");
        return true;
    }

    if (ros::param::has("/spawn_position/z")) {
        ros::param::get("/spawn_position/z", z);
    } else {
        ROS_ERROR_STREAM("param: /spawn_position/z, doesn't exist");
    return true;
    }

    std::string cube_name1, cube_name2, cube_name3, cube_name4;

    if (ros::param::has("spawn_cube_names/name1")) {
        ros::param::get("spawn_cube_names/name1", cube_name1);
    } else {
        ROS_ERROR_STREAM("param: spawn_cube_names/name1, doesn't exist");
    return true;
    }

    if (ros::param::has("spawn_cube_names/name2")) {
        ros::param::get("spawn_cube_names/name2", cube_name2);
    } else {
        ROS_ERROR_STREAM("param: spawn_cube_names/name2, doesn't exist");
    return true;
    }

    if (ros::param::has("spawn_cube_names/name3")) {
        ros::param::get("spawn_cube_names/name3", cube_name3);
    } else {
        ROS_ERROR_STREAM("param: spawn_cube_names/name3, doesn't exist");
    return true;
    }

    if (ros::param::has("spawn_cube_names/name4")) {
        ros::param::get("spawn_cube_names/name4", cube_name4);
    } else {
        ROS_ERROR_STREAM("param: spawn_cube_names/name4, doesn't exist");
    return true;
    }



    if(model_name == "random"){
        double r = ((double)std::rand()/(double)RAND_MAX);;
        if(r < 0.25){
            model_name = cube_name1;
        }else if(r < 0.5){
            model_name = cube_name2;
        }else if(r < 0.75){
            model_name = cube_name3;
        }else {
            model_name = cube_name4;
        }
    }
    std::string position = "-x " + std::to_string(x) + " -y " + std::to_string(y) + " -z " + std::to_string(z);

    std::string directory_to_models = "/home/user/catkin_ws/src/robotic_pusher_ROS/models/";
    std::string model_path = directory_to_models + model_name + "/model.sdf";

    std::string spawn_message = "rosrun gazebo_ros spawn_model -file " + model_path + " -sdf " + position + " -model push_box";

    ROS_INFO("Spawning %s at position %s ", model_name.c_str(), position.c_str());

    system("rosservice call gazebo/delete_model '{model_name: push_box}'");
    system(spawn_message.c_str());

    res.reply = 1;

    ROS_INFO("Spawning object: %s", model_name.c_str());
    return true;
}

int main(int argc, char **argv) {
  ROS_INFO("spawn_object_node started");
  ros::init(argc, argv, "spawn_object_server");
  ros::NodeHandle n;

  ros::ServiceServer service = n.advertiseService("spawn_object", spawn);

  ros::spin();

  return 0;
}