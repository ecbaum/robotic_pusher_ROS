/*********************************************************************
 * Compiler:         >gcc 4.6.3
 *
 * Company:          Chalmers University of Technology
 *
 * Author:           Martin Asplund
 *
 * Compatibility:    Ubuntu 18.04 64bit (ros melodic)
 *
 * Software Version: V0.1
 *
 * Created:          13.12.2021
 *
 * Comment:          *add later
 *
 ********************************************************************/

/*********************************************************************
 * STD INCLUDES
 ********************************************************************/
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
/*********************************************************************
 * ROS INCLUDES
 ********************************************************************/
#include <ros/ros.h>
#include <std_msgs/String.h>
/*********************************************************************
 * CUSTOM INCLUDES
 * ******************************************************************/
#include <geometry_msgs/Pose.h>
#include <getopt.h>
#include <map>
#include <robotic_pusher/getVelocity.h>
#include <robotic_pusher/getWeightType.h>
#include <robotic_pusher/moveTiago.h>
#include <robotic_pusher/spawnObject.h>
#include <rosprolog/rosprolog_client/PrologClient.h>
#include <signal.h>
#include <vector>

using namespace std;

#define object_name "random"
#define file_name "ont_file.txt"
string ontology_name = "cube";
#define desired_distance 10 // cm??

/*  Weight ids  */
std::map<std::string, int> weight_id{
    {"light", 0},
    {"medium", 1},
    {"heavy", 2},
};

/*  Functions   */
float get_action(string weight, PrologClient pl) {
  /*    Check for heavy/medium/light class  */

  std::vector<float> distance_vector;
  std::vector<float> velocity_vector;

  PrologQuery bdgs =
      pl.query("owl_individual_of(I," + ontology_name + ":" + weight + ")");

  for (PrologQuery::iterator it = bdgs.begin(); it != bdgs.end(); it++) {
    PrologBindings bdg = *it;
    string instance = bdg["I"].toString();
    /*  Extract distance    */
    PrologQuery classbdgs =
        pl.query("rdf(" + ontology_name + ":'" + instance + "', distance, X)");
    PrologQuery::iterator iterator = classbdgs.begin();
    bdg = *iterator;
    string distance = bdg["X"].toString();
    distance = distance.substr(distance.find("#") + 1, distance.length());
    distance.erase(0, 1);
    distance.erase(distance.size() - 1);
    float distance_traveled = std::stof(distance);
    distance_vector.push_back(distance_traveled);
    /*  Extract velocity    */
    classbdgs =
        pl.query("rdf(" + ontology_name + ":'" + instance + "', velocity, X)");
    iterator = classbdgs.begin();
    bdg = *iterator;
    string velocity = bdg["X"].toString();
    velocity = velocity.substr(velocity.find("#") + 1, velocity.length());
    velocity.erase(0, 1);
    velocity.erase(velocity.size() - 1);
    float used_velocity = std::stof(velocity);
    velocity_vector.push_back(used_velocity);
  }

  /*  Find the closest points in distance     */
  float min_upper = FLT_MAX, min_lower = FLT_MAX;
  int lower_bound, upper_bound, index = 0;

  for (float d : distance_vector) {
    float diff = abs(d - desired_distance);

    if (diff < min_upper && d > desired_distance) {
      min_upper = diff;
      upper_bound = index;
    } else if (diff < min_lower && d < desired_distance) {
      min_lower = diff;
      lower_bound = index;
    }

    index++;
  }

  /*    Interpolate the velocity    */
  float a = velocity_vector[lower_bound];
  float b = velocity_vector[upper_bound];
  float t = desired_distance / distance_vector[upper_bound];

  return a + t * (b - a);
}

int load_ontology(PrologClient pl) {
  string line;

  ifstream input_file(file_name);
  if (!input_file.is_open()) {
    cerr << "Could not open the file - '" << file_name
         << "' Using ontology without instance" << endl;
    return 1;
  }

  while (getline(input_file, line)) {
    pl.query(line);
  }

  input_file.close();
  return 0;
}

string find_instance_name(string instance_name, string Class, PrologClient pl,
                          int i = 0) {

  PrologQuery bdgs =
      pl.query("owl_individual_of(" + ontology_name + ":'" + instance_name +
               "'," + ontology_name + ":'" + Class + "')");
  bool res = false;
  for (auto &it : bdgs) {
    res = true;
    break;
  }
  if (res) {
    string instance_name_new = instance_name + to_string(i);
    find_instance_name(instance_name_new, Class, pl, i++);
  } else {
    return instance_name;
  }
}

void update_onotology(std::ofstream &file, float distance, float velocity,
                      string object, PrologClient pl) {
  /*    Check if class exist    */
  PrologQuery bdgs = pl.query("rdf_has(" + ontology_name + ":'" + object +
                              "', rdf:type, owl:'Class')");
  bool res = false;
  for (auto &it : bdgs) {
    res = true;
    break;
  }
  if (res) {
    string instance_name = find_instance_name(object, object, pl);
    string query = "rdf_assert(" + ontology_name + ":'" + instance_name +
                   "', rdf:type, " + ontology_name + ":'" + object + "')";
    /*  Create instance  */
    pl.query(query);
    file << query + "\n";
    /*  Create data propery distance  */
    query = "rdf_assert(" + ontology_name + ":'" + instance_name +
            "', velocity , '" + to_string(velocity) + "')";
    pl.query(query);
    file << query + "\n";
    /*  Create data propery velocity  */
    query = "rdf_assert(" + ontology_name + ":'" + instance_name +
            "', distance , '" + to_string(distance) + "')";
    pl.query(query);
    file << query + "\n";

  } else {
    ROS_INFO_STREAM("Class " << object << " does not exist in the ontology");
  }
}

void my_handler(int s) {
  ROS_INFO_STREAM("Caught signal " << s);
  exit(1);
}

int main(int argc, char **argv) {

  bool training = false; // 0 = no training, 1 = training

  // signal(SIGINT, my_handler);
  // Capture ctrl + c event (Not working for some reason [BUG])

  /*  Parse all input arguments   */
  for (;;) {
    switch (getopt(argc, argv, "t:")) {
    case 't':
      training = (bool)(atoi(optarg) > 0);
      continue;

    default:
      /*   Add explain print?  */
      break;

    case -1:
      break;
    }
    break;
  }
  ROS_INFO_STREAM("Training: " << (bool)training);

  ros::init(argc, argv, "get_action_node", ros::init_options::AnonymousName);

  ROS_INFO_STREAM("Starting up action node...");

  ros::NodeHandle n;
  ros::Rate r(60);
  PrologClient pl = PrologClient("/rosprolog", true);
  if (pl.waitForServer()) {
    ROS_INFO_STREAM("Started prolog...");
  }

  /*  Load ontology file if it exist  */
  /*  If there is no ontology to load, force training -> 1*/
  training = (1 * load_ontology(pl));

  // Clients
  ros::ServiceClient client_spawn =
      n.serviceClient<robotic_pusher::spawnObject>("spawn_object");
  ros::ServiceClient client_weight =
      n.serviceClient<robotic_pusher::getWeightType>("weight_type_service");
  ros::ServiceClient client_push =
      n.serviceClient<robotic_pusher::getVelocity>("pusher_service");
  ros::ServiceClient init =
      n.serviceClient<robotic_pusher::moveTiago>("moveTiago_service");

  ROS_INFO_STREAM("Connected to all services...");

  robotic_pusher::spawnObject object_object;
  robotic_pusher::getWeightType weight_object;
  robotic_pusher::getVelocity velocity_object;
  robotic_pusher::moveTiago move_object;

  weight_object.request.get_weight_class = true;
  object_object.request.model_name = object_name;

  geometry_msgs::Pose tiago_init_pose;
  tiago_init_pose.position.x = 0.0;
  tiago_init_pose.position.y = 0.0;
  tiago_init_pose.position.z = 0.0;
  tiago_init_pose.orientation.x = 0.0;
  tiago_init_pose.orientation.y = 0.0;
  tiago_init_pose.orientation.z = 0.0;
  tiago_init_pose.orientation.w = 1.0;
  move_object.request.desPose = tiago_init_pose;

  /*  Initialize Tiago in position    */
  if (init.call(move_object)) {
    ROS_INFO_STREAM(
        "Tiago in correct position?: " << (bool)move_object.response.reply);
  } else {
    ROS_ERROR_STREAM("Failed to move Tiago to init position, exiting...");
    return 1;
  }

  /*  Open file to save ontology  */
  ofstream ont_File;
  ont_File.open(file_name);

  while (ros::ok()) {

    /*  Call the service to spawn a object  */
    if (client_spawn.call(object_object)) {
      ROS_INFO_STREAM("Response: " << object_object.response.reply);
    } else {
      ROS_ERROR_STREAM("Failed to spawn a random object");
      return 1;
    }

    // Wait task to be finished
    ROS_INFO("Wait shortly to make sure previous task is finished.");
    ros::Duration(1).sleep();

    string object_weight_type;
    string object_weight_color;
    /*  Call the service to get the weight of the spawned object  */
    if (client_weight.call(weight_object)) {
      object_weight_type = weight_object.response.weight_type;
      object_weight_color = weight_object.response.object_color;
      ROS_INFO_STREAM("Response: " << object_weight_type << " "
                                   << object_weight_color);
    } else {
      ROS_ERROR_STREAM("Failed to get the weight from the object");
      return 1;
    }

    if (training) {
      // Random between 0.f and 1.f
      velocity_object.request.impact_velocity =
          static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    } else {
      velocity_object.request.impact_velocity =
          get_action(object_weight_type, pl);
    }

    // Wait task to be finished
    ROS_INFO("Wait shortly to make sure previous task is finished.");
    ros::Duration(1).sleep();

    float traveled_distance;
    if (client_push.call(velocity_object)) {
      float x = velocity_object.response.position.x;
      float y = velocity_object.response.position.y;
      float z = velocity_object.response.position.z;
      traveled_distance = sqrtf(x * x + y * y + z * z);
      ROS_INFO_STREAM("Response: " << traveled_distance);
    } else {
      ROS_ERROR_STREAM("Failed to get the position from the object");
      return 1;
    }

    if (training)
      update_onotology(ont_File, traveled_distance,
                       velocity_object.request.impact_velocity,
                       object_weight_color, pl);
  }
  /*    Save ontology here   */
  ont_File.close();

  return 0;
}