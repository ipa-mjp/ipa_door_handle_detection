#include "ipa_database_generation.h"
#include "ipa_door_handle_segmentation.h"


#include <stdio.h>
#include <sys/select.h>
#include <termios.h>
#include <stropts.h>


// definition
#define TOPIC_POINT_CLOUD_GEN "/pico_flexx/points"  //ROS topic point cloud
#define TOPIC_POINT_CLOUD_PATCH "/selected_patch" //ROS topic selected patch 
#define PATH_TO_DIR "/home/robot/Desktop/rmb-ml/PointCloudDataBase/" // Path to store patches

DoorhandleDatabaseGeneration::DoorhandleDatabaseGeneration(ros::NodeHandle nh, sensor_msgs::PointCloud2::Ptr point_cloud_out_msg) :
nh_(nh), point_cloud_out_msg_(point_cloud_out_msg)
{
	initCameraNode(nh,point_cloud_out_msg);	
}


std::string DoorhandleDatabaseGeneration::getFilePathFromParameter(int dist, int angle_XZ, int angle_YZ)
{
		int num_dist = 3;
		int distances[] = {40-dist,55-dist,70-dist};

		int num_angle_xz = 3;
		int angle_xz[] = {-10-angle_XZ,0-angle_XZ,10-angle_XZ}; //deg

		int num_angle_yz = 3;
		int angle_yz[] = {0-angle_YZ,5-angle_YZ}; // deg

		// setup vec contain this info: angle_xz, angle_yz, distance to door plane

	//distance loop
 		int index_d = 0 ;
		int n_d = abs(distances[0]);
		for (int i = 1; i < num_dist; i++)
		{
			if (distances[i] < n_d)
			{
				n_d = abs(distances[i]); 
				index_d = i ;
			}
 		}
		int final_distance = distances[index_d]+dist;

		int index_a1 = 0 ;
		int n_a1 = abs(angle_xz[0]);
		for (int i = 1; i < num_angle_xz; i++)
		{
			if (angle_xz[i] < n_a1)
			{
				n_a1 = abs(angle_xz[i]); 
				index_a1 = i ;
			}
 		}
		int final_angleXZ = angle_xz[index_a1]+angle_XZ;

		int index_a2 = 0 ;
		int n_a2 = abs(angle_yz[0]);
		for (int i = 1; i < num_angle_yz; i++)
		{
			if (angle_yz[i] < n_a2)
			{
				n_a2 = abs(angle_yz[i]); 
				index_a2 = i ;
			}
 		}
		int final_angleYZ = angle_yz[index_a2]+angle_YZ;

		std::stringstream str1, str2, str3;

		str1 << final_angleXZ;
		str2 << final_angleYZ;
		str3 << final_distance;

		std::string angle_1_str = str1.str();
		std::string angle_2_str = str2.str();
		std::string dist_str = str3.str();

		std::string name_pcd  = "_distance_" + dist_str + "cm_" + "angleXZ_" + 	angle_1_str + "°_" + "angleYZ_"+ angle_2_str + "°.pcd";

		return name_pcd;
}



void DoorhandleDatabaseGeneration::pointcloudCallback_1(const sensor_msgs::PointCloud2::ConstPtr& point_cloud_msg)
{
	pcl::console::setVerbosityLevel(pcl::console::L_ALWAYS);

	//create new point cloud in pcl format: pointcloud_in_pcl_format
	pcl::PointCloud<pcl::PointXYZ>::Ptr point_cloud(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr point_cloud_rgb(new pcl::PointCloud<pcl::PointXYZRGB>);
	pcl::PointCloud<pcl::PointXYZRGB>::Ptr point_cloud_rgb_trans(new pcl::PointCloud<pcl::PointXYZRGB>);
	pcl::ModelCoefficients::Ptr plane_coeff (new pcl::ModelCoefficients);
	pcl::PointIndices::Ptr inliers (new pcl::PointIndices);

	//transform imported pointcloud point_cloud_msg to pointcloud_in_pcl_format
	pcl::fromROSMsg(*point_cloud_msg, *point_cloud);

	if (point_cloud->points.size() != 0)
	{
		PointCloudSegmentation seg;

		planeInformation planeData = seg.detectPlaneInPointCloud(point_cloud);
		inliers = planeData.plane_point_cloud_indices;
		plane_coeff = planeData.plane_coeff;

		/// interpretation of ros data
		pcl::PointXYZRGB pclPoint;
		for (size_t i = 0; i < point_cloud->points.size (); ++i)
		{
			pclPoint.x = point_cloud->points[i].x;
			pclPoint.y = -point_cloud->points[i].y;
			pclPoint.z = point_cloud->points[i].z;
			point_cloud_rgb_trans->points.push_back(pclPoint);
		}

		Eigen::Vector3f cam_axis;
		cam_axis << 0,0,1;

		 double scalar_prod_xz = plane_coeff->values[0];
		 double scalar_prod_yz = plane_coeff->values[1];
 
		 double len_xz = pow(plane_coeff->values[0],2) + pow(plane_coeff->values[2],2);
		 double len_yz = pow(plane_coeff->values[1],2) + pow(plane_coeff->values[2],2); 

		// get geometrical lenght
		double cos_alpha_1 = scalar_prod_xz/sqrt(len_xz);
		double cos_alpha_2 = scalar_prod_yz/sqrt(len_yz);

		int angle_1 = 90-acos(cos_alpha_1)*180.0/M_PI;
		int angle_2 = asin(cos_alpha_2)*180.0/M_PI;

		int dist = -plane_coeff->values[3]*100; // to cm

		if ((abs(dist) > 100) || (abs(angle_1) > 180) || (abs(angle_2) > 180)) 
		{
			std::cout<<"angle_xz: " << std::endl;
			std::cout<<"angle_yz: " << std::endl;
			std::cout<<"distance: " << std::endl;

		}

		else
		{
			std::cout<<"angle_xz: "<<angle_1<< "°" << std::endl;
			std::cout<<"angle_yz: "<<angle_2<< "°" << std::endl;
			std::cout<<"distance: "<< dist << "cm" << std::endl;


		}

		setup_ << angle_1,angle_2,dist;  
	}
}


void DoorhandleDatabaseGeneration::initCameraNode(ros::NodeHandle nh, sensor_msgs::PointCloud2::Ptr point_cloud_out_msg)
{
	std::cout << "Initialising DoorhandleDatabaseGeneration Constructor." << std::endl;
	
	pub_ = nh_.advertise<sensor_msgs::PointCloud2>(TOPIC_POINT_CLOUD_GEN,1);
	(pub_) ? std::cout << "Pub is valid." << std::endl : std::cout << "Pub is not valid." << std::endl;

	ros::Subscriber point_cloud_sub_1_ = nh_.subscribe<sensor_msgs::PointCloud2>(TOPIC_POINT_CLOUD_GEN, 1, &DoorhandleDatabaseGeneration::pointcloudCallback_1, this);

	ros::Duration(1).sleep();
	ros::Rate loop_rate(10);
	while (ros::ok())
	{
		ros::spinOnce();
		loop_rate.sleep();
	}

	if (!ros::ok()){
		std::cout << "Quit publishing" << std::endl;
	}
	std::cout << "DoorhandleDatabaseGeneration Constructor Initialised." << std::endl;
}



// =================================================0
int main(int argc, char **argv)
{		

	ros::init(argc, argv, "Param_Overview");
	ros::NodeHandle nh;
	sensor_msgs::PointCloud2::Ptr point_cloud_out_msg(new sensor_msgs::PointCloud2);
	DoorhandleDatabaseGeneration DoorhandleDatabaseGeneration(nh, point_cloud_out_msg);

	return 0;
}
