//============================================================================
// Name        : gsFaceDetectHttpServer.cpp
// Author      : gjsy
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "gsfacedetectsdk.h"
//#include "gsFaceTrackSDK.h"

#include "ConnJDBC.h"
#include "HttpServer.h"

//初始化HttpServer静态类成员
http_opts g_http_server_opts;
std::unordered_map<std::string, ReqHandler> HttpServer::m_handler_map;
std::vector< shared_ptr<tid_sd> > tid_socket;

tid_sd feature_fd_map;
MatData tmp_feature_data;
extern std::map<std::string, MatData> date_queue;
extern std::map<std::string, std::shared_ptr<MatData> > feature_map;

#if USED_VECTOR_FEATURE
//使用程序内部的人脸特征存储队列
std::vector<FeatureData> feature_queue;

#elif USED_MYSQL_FEATURE
//使用mysql数据库人脸特征

#elif USED_REDIS_FEATURE
//使用redis数据库人脸特征

#endif

int thread_num = 1;
std::string proto_model_dir = "/opt/lib_ai/faceSDK/mtcnn-model/";
std::string tensorflow_model_dir = "tensorflow/mars-small128.pb";

std::string getCurrentTime();

float getSimilarity(const float* lhs, const float* rhs)
{
	float tmp = 0.0;
	for (int i = 0; i < FSIZE; ++i)
		tmp += lhs[i] * rhs[i];
	return tmp;
}

std::string senderTostring(std::string& uuid, ERRTYPE type)
{
	Json::Value root;
	Json::FastWriter fw;

	root["uuid"] = Json::Value(uuid);
	root["status"] = Json::Value(type);
	return fw.write(root);
}

std::string getHttpHeaderNameValue(const http_message *http_msg, std::string name)
{
	//for(auto node : http_msg->header_names)
	std::string value;
	for(int i = 0; i < MG_MAX_HTTP_HEADERS; i++)
	{
		std::string node(http_msg->header_names[i].p, http_msg->header_names[i].len);
		if (!name.compare(node))
		{
			value = std::string(http_msg->header_values[i].p, http_msg->header_values[i].len);
			size_t index = value.find(";"); //处理数据粘包的问题
			if (index != string::npos)
				value = value.substr(0,index);
			break;
		}
	}
	return value;
}

std::string getHttpBodyNameValue(const struct mg_str* body, std::string name)
{
	std::string value;
	size_t name_len(name.size());
	int len = 213-name_len; //自定义图片数据之前的body大小
	const char *p;
	char tmp[KEY_MAX_SIZE] = {0};

	if (body->p != NULL && !name.empty() && body->len != 0)
	{
	    p = body->p;
	    for (int i = 0; i < len; i++,p++)
	    {
			int n = 0;
			if (p[name_len] == '=' && !mg_ncasecmp(name.c_str(), p, name_len))
			{
				int m = 0;
				const char *v = &p[name_len+1];
				while(v[m] != ';' && v[m] != '\r' && v[m] != '\n')
				{
					if (v[m] == '"') {
						m++;
						continue;
					}
					if (n >= KEY_MAX_SIZE)
						break;
					tmp[n++] = v[m++];
				}
				value = std::string(tmp, n);
				break;
			}
			else if (p[name_len] == ':' && !mg_ncasecmp(name.c_str(), p, name_len))
			{
				//待扩展
			}
	    }
	}
	return value;
}

std::string getHttpBodyImageValue(std::string& body, size_t length)
{
	std::string value = "";
	size_t index = body.find("\r\n\r\n");
	if(index != string::npos)
	{
		value = body.substr(index+4, length);
	}
	return value;
}

bool handle_getFaceALL(http_conn *nc, http_message *http_msg, OnRspCallback rsp_callback)
{
    std::cout << "handle_getFaceALL" << std::endl;

    static int count = 0;
    MatData data;
	Json::Reader reader;
	Json::Value root;
	Json::FastWriter fw;
    std::string body = std::string(http_msg->body.p, http_msg->body.len);
    std::string query_string = std::string(http_msg->query_string.p, http_msg->query_string.len);
    //获取数据头字段值
    std::string Content_Type = getHttpHeaderNameValue(http_msg, "Content-Type");

    if (mg_vcmp(&http_msg->method, "GET") == 0)
    {
    	//从字符串中读取数据
    	if (reader.parse(body, root))
    	{
    	 	string uuid = root["uuid"].asString();
    		data.total_lenth = root["MatSize"].asUInt64();
    		data.image_w = root["Width"].asInt();
    		data.image_h = root["Height"].asInt();
    		data.image_c = root["Channel"].asInt();
    		data.m_nc = nc;
    		data.write_fd = tid_socket[count]->sockfd[0];
    		data.process_thread_num = tid_socket[count++]->tid;
    		if (count == thread_num)
    			count = 0;
    		data.request_type = std::stoi(query_string.substr(5));
    		data.m_startTime = cv::getTickCount();
    		date_queue.insert(std::make_pair(uuid, data));

			std::string msg = senderTostring(uuid, HTTP_REQUEST_CREATE_OK);
    		rsp_callback(nc, msg);
    	}
    	else
    	{
    		//send HTTP_REQUEST_PARSE_ERR
    		rsp_callback(nc, "{ \"status\": 102 }");
    	}
    }
    //添加 form-data 数据解析
    else if (!Content_Type.compare("multipart/form-data") && mg_vcmp(&http_msg->method, "POST") == 0)
    {
    	std::string uuid = getHttpBodyNameValue(&http_msg->body, "name");
    	auto iter = date_queue.find(uuid);
		if(iter != date_queue.end())
		{
			if (iter->second.receive_status)
			{
				//send HTTP_DATA_RECEIVE_FINISH
				std::string msg = senderTostring(uuid, HTTP_DATA_RECEIVE_FINISH);
				rsp_callback(nc, msg);
				return true;
			}

			string image = getHttpBodyImageValue(body, iter->second.total_lenth);
			iter->second.data += image;
			iter->second.m_flag = HTTP_FORM_DATA;
			iter->second.recv_lenth += image.size();

			//根节点属性
			root["uuid"] = Json::Value(uuid);
			//HTTP_DATA_RECEIVE_WAITTING
			root["status"] = Json::Value(100);
			root["receive_size"] = Json::Value(image.size());

			string ret = fw.write(root);

			if (iter->second.recv_lenth >= iter->second.total_lenth)
			{
				iter->second.receive_status = true;
				//send HTTP_DATA_RECEIVE_FINISH
				//std::string msg = senderTostring(uuid, HTTP_DATA_RECEIVE_FINISH);
				//rsp_callback(nc, msg);
				write(iter->second.write_fd, uuid.c_str(), uuid.size());
			}
			else
				rsp_callback(nc, ret);
		}
		else
		{
			//send HTTP_RECEIVE_UUID_ERR
			std::string msg = senderTostring(uuid, HTTP_RECEIVE_UUID_ERR);
			rsp_callback(nc, msg);
		}
    }
    //添加 raw, binary 解析方式
    else if (mg_vcmp(&http_msg->method, "POST") == 0)
    {
    	//imageData recvData;
    	//if (recvData.ParseFromString(body))
    	if (body.find(';') == 14)
		{
//			string uuid = recvData.uuid();
//			string image = recvData.data();
    		string uuid = body.substr(0, 14);
    		string image = body.substr(15, body.size());
			auto iter = date_queue.find(uuid);
			if(iter != date_queue.end())
			{
				if (iter->second.receive_status)
				{
					//send HTTP_DATA_RECEIVE_FINISH
					std::string msg = senderTostring(uuid, HTTP_DATA_RECEIVE_FINISH);
					rsp_callback(nc, msg);
					return true;
				}

				iter->second.data += image;
				iter->second.m_flag = HTTP_RAW_OR_BINARY;
				iter->second.recv_lenth += image.size();

				//根节点属性
				root["uuid"] = Json::Value(uuid);
				//HTTP_DATA_RECEIVE_WAITTING
				root["status"] = Json::Value(100);
				root["receive_size"] = Json::Value(image.size());

				string ret = fw.write(root);

				if (iter->second.recv_lenth >= iter->second.total_lenth)
				{
					iter->second.receive_status = true;
					//send HTTP_DATA_RECEIVE_FINISH
					//std::string msg = senderTostring(uuid, HTTP_DATA_RECEIVE_FINISH);
					//rsp_callback(nc, msg);
					write(iter->second.write_fd, uuid.c_str(), uuid.size());
				}
				else
					rsp_callback(nc, ret);
			}
			else
			{
				//send HTTP_RECEIVE_UUID_ERR
				std::string msg = senderTostring(uuid, HTTP_RECEIVE_UUID_ERR);
				rsp_callback(nc, msg);
			}
		}
		else
		{
			//send HTTP_RECEIVE_DATA_FORMAT_ERR
			rsp_callback(nc, "{ \"status\": 103 }");
			std::cout << "rsp_callbackx getFaceALL" << std::endl;
		}
    }
    else
    {
    	//send HTTP_REQUEST_METHOD_ERROR
    	rsp_callback(nc, "{ \"status\": 105 }");
    }

    return true;
}

bool handle_setFaceFeature(http_conn *nc, http_message *http_msg, OnRspCallback rsp_callback)
{
	std::cout << "handle_setFaceFeature" << std::endl;

	Json::Reader reader;
	Json::Value root;

	std::string body = std::string(http_msg->body.p, http_msg->body.len);
	std::string query_string = std::string(http_msg->query_string.p, http_msg->query_string.len);

	if (mg_vcmp(&http_msg->method, "GET") == 0)
	{
		try {
		//从字符串中读取数据
		if (!tmp_feature_data.process_status && reader.parse(body, root))
		{
			tmp_feature_data.clear();
			std::string uuid = root["uuid"].asString();
			tmp_feature_data.total_lenth = root["FeatureSize"].asUInt64();
			tmp_feature_data.image_c = root["FeatureNum"].asInt();
			tmp_feature_data.m_nc = nc;
			tmp_feature_data.write_fd = feature_fd_map.sockfd[0];
			// 1=add, 2=del, 3=update
			tmp_feature_data.request_type = std::stoi(query_string.substr(5));
			//send HTTP_REQUEST_CREATE_OK
			std::string msg = senderTostring(uuid, HTTP_REQUEST_CREATE_OK);
			rsp_callback(nc, msg);
		}
		else
		{
			//send HTTP_REQUEST_PARSE_ERR
			rsp_callback(nc, "{ \"status\": 102 }");
		}
		}
		catch(std::out_of_range& err) {
			//send HTTP_DATA_OUTRANGE
    		rsp_callback(nc, "{ \"status\": 108 }");

		}
	}
	else if (mg_vcmp(&http_msg->method, "POST") == 0)
	{
		std::string uuid = query_string.substr(5);
		if (tmp_feature_data.receive_status)
		{
			//send HTTP_DATA_RECEIVE_FINISH
			std::string msg = senderTostring(uuid, HTTP_DATA_RECEIVE_FINISH);
			rsp_callback(nc, msg);
			return true;
		}

		tmp_feature_data.data += body;
		tmp_feature_data.recv_lenth += body.size();

		if (tmp_feature_data.recv_lenth > tmp_feature_data.total_lenth)
		{
			tmp_feature_data.receive_status = true;
			//send HTTP_RECEIVE_DATA_SIZE_ERR
			std::string msg = senderTostring(uuid, HTTP_RECEIVE_DATA_SIZE_ERR);
			rsp_callback(nc, msg);
			return true;
		}

		Json::Value wroot;
		Json::FastWriter fw;

		wroot["uuid"] = Json::Value(uuid);
		//HTTP_DATA_RECEIVE_WAITTING
		wroot["status"] = Json::Value(HTTP_DATA_RECEIVE_WAITTING);
		wroot["receive_size"] = Json::Value(body.size());

		string ret = fw.write(wroot);

		if (tmp_feature_data.recv_lenth == tmp_feature_data.total_lenth)
		{
			tmp_feature_data.receive_status = true;
			//send HTTP_DATA_RECEIVE_FINISH
			//std::string msg = senderTostring(uuid, HTTP_DATA_RECEIVE_FINISH);
			//rsp_callback(nc, msg);
			write(tmp_feature_data.write_fd, uuid.c_str(), uuid.size());
		}
		else
		{
			rsp_callback(nc, ret);
		}
	}
    else
    {
    	//send HTTP_REQUEST_METHOD_ERROR
    	rsp_callback(nc, "{ \"status\": 105 }");
    	std::cout << "rsp_callback setFaceFeature" << std::endl;
    }

	return true;
}

bool handle_getFaceFeatureCompare(http_conn *nc, http_message *http_msg, OnRspCallback rsp_callback)
{
    std::cout << "handle_getFaceFeatureCompare" << std::endl;

    static int count = 0;
    std::shared_ptr<MatData> data = std::make_shared<MatData>();
	Json::Reader reader;
	Json::Value root;

	std::string body = std::string(http_msg->body.p, http_msg->body.len);
	//default query_string "type=1"
	std::string query_string = std::string(http_msg->query_string.p, http_msg->query_string.len);

	if (mg_vcmp(&http_msg->method, "GET") == 0)
	{
		//从字符串中读取数据
		if (reader.parse(body, root))
		{
			string uuid = root["uuid"].asString();
			data->total_lenth = root["FeatureSize"].asUInt64();
			data->image_c = root["FeatureNum"].asInt();
			data->m_nc = nc;
			data->write_fd = tid_socket[count]->sockfd[1];
			data->process_thread_num = tid_socket[count++]->ftid;
			if (count == thread_num)
				count = 0;
			//1(1:1), 2(1:n) only 1
			data->request_type = std::stoi(query_string.substr(5));
			data->m_startTime = cv::getTickCount(); //测试执行时间
			feature_map.insert(std::make_pair(uuid, data));
			//send HTTP_REQUEST_CREATE_OK
			std::string msg = senderTostring(uuid, HTTP_REQUEST_CREATE_OK);
			rsp_callback(nc, msg);
		}
		else
		{
			//send HTTP_REQUEST_PARSE_ERR
			rsp_callback(nc, "{ \"status\": 102 }");
		}
	}
	else if (mg_vcmp(&http_msg->method, "POST") == 0)
	{
		string uuid = query_string.substr(5);
		auto iter = feature_map.find(uuid);
		if(iter != feature_map.end())
		{
			if (iter->second->receive_status)
			{
				//send HTTP_DATA_RECEIVE_FINISH
				std::string msg = senderTostring(uuid, HTTP_DATA_RECEIVE_FINISH);
				rsp_callback(nc, msg);
				return true;
			}

			iter->second->data += body;
			iter->second->recv_lenth += body.size();

			if (iter->second->recv_lenth > iter->second->total_lenth)
			{
				iter->second->receive_status = true;
				iter->second->process_status = true;
				//send HTTP_RECEIVE_DATA_SIZE_ERR
				std::string msg = senderTostring(uuid, HTTP_RECEIVE_DATA_SIZE_ERR);
				rsp_callback(nc, msg);
				return true;
			}

			Json::Value wroot;
			Json::FastWriter fw;
			//根节点属性
			wroot["uuid"] = Json::Value(uuid);
			//HTTP_DATA_RECEIVE_WAITTING
			wroot["status"] = Json::Value(99);
			wroot["receive_size"] = Json::Value(body.size());

			string ret = fw.write(wroot);

			if (iter->second->recv_lenth == iter->second->total_lenth)
			{
				iter->second->receive_status = true;
				//send HTTP_DATA_RECEIVE_FINISH
				//std::string msg = senderTostring(uuid, HTTP_DATA_RECEIVE_FINISH);
				//rsp_callback(nc, msg);
				write(iter->second->write_fd, uuid.c_str(), uuid.size());
			}
			else
				rsp_callback(nc, ret);
		}
		else
		{
			//send HTTP_RECEIVE_UUID_ERR
			std::string msg = senderTostring(uuid, HTTP_RECEIVE_UUID_ERR);
			rsp_callback(nc, msg);
		}
	}
	else
	{
		//send HTTP_RECEIVE_DATA_FORMAT_ERR
		rsp_callback(nc, "{ \"status\": 103 }");
		std::cout << "rsp_callback getFaceFeatureCompare" << std::endl;
	}

    return true;
}

bool handle_getFaceTrackID(http_conn *nc, http_message *http_msg, OnRspCallback rsp_callback)
{
    // do sth
    std::cout << "handle_getFaceTrackID" << std::endl;
    rsp_callback(nc, "handle_getFaceTrackID don't support");

    return true;
}

bool handle_getFaceFeature(http_conn *nc, http_message *http_msg, OnRspCallback rsp_callback)
{
    std::cout << "handle_getFaceFeature" << std::endl;
    rsp_callback(nc, "handle_getFaceFeature don't support");

    return true;
}

bool handle_getFaceAgeSex(http_conn *nc, http_message *http_msg, OnRspCallback rsp_callback)
{
	std::cout << "handle_getFaceAgeSex" << std::endl;
    rsp_callback(nc, "handle_getFaceAgeSex don't support");

    return true;
}

void* sdl_functions(void* par)
{
	tid_sd *value = (tid_sd*)par;
	pthread_t id = value->tid;
	int gpu_id = value->gpu_id;
	int read_fd = value->sockfd[1];
	cout << "sdl_functions running tid: " << id << endl;

    int status = HTTP_DATA_FACERESULT_ERROR;
    GetFaceType req_type = GetFaceType::FACEALL;

	/*
	 * init gsFaceDetectSdk object
	 */
	MtcnnPar default_par;
	default_par.devid = gpu_id;
	default_par.minSize = 40;
	default_par.factor = 0.509;
	default_par.featureshape[0] = 56;
	default_par.featureshape[1] = 56;
	default_par.gendershape[0] = 112;
	default_par.gendershape[1] = 112;
	default_par.threshold[0] = 0.6;
	default_par.threshold[1] = 0.8;
	default_par.threshold[2] = 0.9;
	default_par.nms_threshold[0] = 0.4;
	default_par.nms_threshold[1] = 0.7;
	default_par.nms_threshold[2] = 0.7;
	default_par.featurelayername = (char*)"fc1_output";
	default_par.LongSideSize = 1920;
	default_par.scalRatio = FACTOR_16_9;
	default_par.type = FACEALL;

	//string tensorflow_path = proto_model_dir+tensorflow_model_dir;

	GsFaceDetectSDK sdk(proto_model_dir.c_str(), &default_par);
	//gsFaceTrackSDK deepsort(&sort_par);

	while(1)
	{
		int readlen;
		FaceRect faceBox[15];
		FACE_NUMBERS size;
		Json::Value wroot;
		Json::FastWriter fw;
		char buf[BUFFER_SIZE]= {0};
		readlen = read(read_fd, buf, BUFFER_SIZE);
		//buf[readlen] = '\0';
		string uuid(buf, readlen);

		if (!uuid.compare(THREAD_EXIT))
			break;

		auto iter = date_queue.find(uuid);
		if(iter != date_queue.end())
		{
			std::cout << "uuid: " << uuid << " $$ threadID: " << id <<std::endl;

		    try {
				//Mat img_rgb;

				int height = iter->second.image_h;
				int width = iter->second.image_w;
				int length = iter->second.total_lenth;
				int channel = iter->second.image_c;

				uchar *data = (uchar *)iter->second.data.c_str();
				cv::Mat image(height, width, CV_8UC3);
		    	if (iter->second.m_flag == HTTP_FORM_DATA)
		    	{
		    		vector<uchar> v_data(length);
		    		for(int i =0 ; i < length; i++)
		    			v_data[i] = data[i];
		    		image = cv::imdecode(cv::Mat(v_data),1);
		    	}
		    	if (iter->second.m_flag == HTTP_RAW_OR_BINARY)
		    	{
					for(int i = 0; i < height; i++)
					{
						for (int j = 0; j < width; j++)
						{
							image.at<cv::Vec3b>(i,j)[0] = data[width * channel * i + j * channel + 0];
							image.at<cv::Vec3b>(i,j)[1] = data[width * channel * i + j * channel + 1];
							image.at<cv::Vec3b>(i,j)[2] = data[width * channel * i + j * channel + 2];
						}
					}
		    	}
				cv::imwrite("/home/ai_002/dwj-Qt/123.jpg",image);
				//process image code

				//目前只有３种数据选择
				switch(iter->second.request_type)
				{
					case 1:
						req_type = GetFaceType::FACEALL;
						status = HTTP_DATA_FACEALL_OK;
						break;
					case 2:
						req_type = GetFaceType::FACEBOXFEATURE;
						status = HTTP_DATA_FACEBOXFEATURE_OK;
						break;
					case 3:
						req_type = GetFaceType::FACEGENDER;
						status = HTTP_DATA_GENDER_OK;
						break;
				}

				size = sdk.getFacesAllResult(image, faceBox, true, req_type);
		    }catch(cv::Exception& exp)
		    {
		    	cout << "opencv: " << exp.what();
		    }

			double duration = (cv::getTickCount()-iter->second.m_startTime)/ cv::getTickFrequency() * 1000.0;

		    cout << " ^^ face nums: " << size << " ^^ duration: " << duration << endl;

			wroot["uuid"] = Json::Value(uuid);
			wroot["status"] = Json::Value(status);
			wroot["facenum"] = Json::Value(size);

			for (FACE_NUMBERS i = 0; i < size; i++)
			{
				//添加一个新的子节点
				Json::Value node;
				node["facebox"].append(faceBox[i].x1);
				node["facebox"].append(faceBox[i].y1);
				node["facebox"].append(faceBox[i].x2);
				node["facebox"].append(faceBox[i].y2);
				node["faceage"] = faceBox[i].age;
				node["facesex"] = faceBox[i].gender;
				for(int j = 0; j < 5; j++)
				{
					node["landmark"].append(faceBox[i].facepts.x[j]);
					node["landmark"].append(faceBox[i].facepts.y[j]);
				}
				for(int j = 0; j < 512; j++)
					node["feature"].append(faceBox[i].feature[j]);

				wroot["faceresult"].append(node);
			}

			string rsp = fw.write(wroot);

		    // 必须先发送header
		    mg_printf(iter->second.m_nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
		    // 以json形式返回
		    mg_printf_http_chunk(iter->second.m_nc, rsp.c_str());
		    //mg_printf_http_chunk(connection, "{%s}", rsp.c_str());
		    // 发送空白字符快，结束当前响应
		    mg_send_http_chunk(iter->second.m_nc, "", 0);

		    //数据处理完成
		   	iter->second.process_status = true;
		}
	}
	return nullptr;
}

/*
 * setUpdateCommand 返回值未使用
 */
void* afsdl_functions(void* par)
{
	tid_sd *value = (tid_sd*)par;
	pthread_t id = value->ftid;
	int read_fd = value->sockfd[1];
	cout << "afsdl_functions running tid: " << id << endl;
	ConnJDBC mysql;
	mysql.selectDataBase("use sys");
	while(true)
	{
		int readlen;
		string sendmsg;
		Json::Value wroot;
		Json::FastWriter fw;
		char buf[BUFFER_SIZE]= {0};

		readlen = read(read_fd, buf, BUFFER_SIZE);
		string uuid(buf, readlen);
		tmp_feature_data.process_status = true;

		//测试使用
		if (!uuid.compare(THREAD_EXIT))
			break;

		std::cout << "uuid: " << uuid << " $$ threadID: " << id <<std::endl;

		Json::Reader reader;
		Json::Value root;
		try {
			if (reader.parse(tmp_feature_data.data, root))
			{
				//1=add, 2=del, 3=update
				for (int i = 0; i < tmp_feature_data.image_c; i++)
				{
					if (tmp_feature_data.request_type == 1)
					{
						FeatureData data;
						data.id = root["gsafety"][i]["id"].asString();
						for(int j = 0; j < 512; j++)
						{
							data.feature[j] = root["gsafety"][i]["feature"][j].asFloat();
						}
						std::string feature = root["gsafety"][i]["feature"].toStyledString();
						data.date = ::getCurrentTime();

						std::string command = "INSERT INTO FaceData (FaceId,FaceFeature,FaceDate) VALUES ('";
						command += data.id; command += "','";
						command += feature; command += "','";
						command += data.date;
						command += "')";
						mysql.setUpdateCommand(command);

						feature_queue.push_back(data);
					}
					else if (tmp_feature_data.request_type == 2)
					{
						string id = root["gsafety"][i]["id"].asString();
						auto iter = feature_queue.begin();
						while (iter != feature_queue.end())
						{
							if (iter->id.compare(id))
							{
								iter++;
								continue;
							}
							std::string command = "DELETE FROM FaceData WHERE FaceId=";
							command += id;
							mysql.setUpdateCommand(command);

							feature_queue.erase(iter);
							break;
						}
					}
					else if (tmp_feature_data.request_type == 3)
					{
						string id = root["gsafety"][i]["id"].asString();
						auto iter = feature_queue.begin();
						while (iter != feature_queue.end())
						{
							if (iter->id.compare(id))
							{
								iter++;
								continue;
							}
							std::string feature = root["gsafety"][i]["feature"].toStyledString();
							std::string command = "UPDATE FaceData SET FaceFeature='";
							command += feature;
							command += "' WHERE FaceId=";
							command += id;
							mysql.setUpdateCommand(command);

							for(int j = 0; j < 512; j++)
							{
								iter->feature[j] = root["gsafety"][i]["feature"][j].asFloat();
							}
							break;
						}
					}
				}

				//send HTTP_DATA_PROCESS_OK
				sendmsg = senderTostring(uuid, HTTP_DATA_PROCESS_OK);
			}
			else
			{
				//send HTTP_RECEIVE_DATA_PARSE_ERR
				sendmsg = senderTostring(uuid, HTTP_RECEIVE_DATA_PARSE_ERR);
			}
		}
		catch(Json::LogicError& err)
		{
    		//send HTTP_RECEIVE_DATA_PARSE_ERR
			sendmsg = senderTostring(uuid, HTTP_RECEIVE_DATA_PARSE_ERR);
		}
		catch(sql::SQLException& err)
		{
			//send HTTP_MYSQL_PRIMARY_KEY_DUPLICATE
			sendmsg = senderTostring(uuid, HTTP_MYSQL_PRIMARY_KEY_DUPLICATE);
		}
		std::cout << "feature_queue num: " << feature_queue.size() <<std::endl;

		tmp_feature_data.clear();
	    mg_printf(tmp_feature_data.m_nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
	    mg_printf_http_chunk(tmp_feature_data.m_nc, sendmsg.c_str());
	    mg_send_http_chunk(tmp_feature_data.m_nc, "", 0);
	}
	return nullptr;
}

void* fsdl_functions(void* par)
{
	tid_sd *value = (tid_sd*)par;
	pthread_t id = value->ftid;
	int read_fd = value->sockfd[0];
	cout << "fsdl_functions running tid: " << id << endl;
	float Standard = 0.62;
	while(true)
	{
		int readlen;
		string sendmsg;
		Json::Value root;
		Json::Value wroot;
		Json::FastWriter fw;
		Json::Reader reader;
		char buf[BUFFER_SIZE]= {0};
		readlen = read(read_fd, buf, BUFFER_SIZE);
		//buf[readlen] = '\0';
		string uuid(buf, readlen);

		if (!uuid.compare(THREAD_EXIT))
			break;

		std::cout << "uuid: " << uuid << " $$ threadID: " << id << " $$ feature_queue num: " << feature_queue.size() <<std::endl;

		auto iter = feature_map.find(uuid);
		try {
			if (iter != feature_map.end() && reader.parse(iter->second->data, root))
			{
				string id = "19700000000000";
				float maxsim = 0.0;
				size_t flen = feature_queue.size();
				// 提取第一个512特征
				for (int i = 0; i < 512; i++)
				{
					iter->second->featue[i] = root["gsafety"][0][i].asFloat();
				}

				if (1 == iter->second->request_type)
				{
					for (size_t i = 0; i < flen; i++)
					{
						float sim = getSimilarity(iter->second->featue, feature_queue[i].feature);
						if (sim >= Standard)
						{
							maxsim = sim > maxsim ? sim : maxsim;
							id = feature_queue[i].id;
						}
					}
				}
				else if (2 == iter->second->request_type)
				{
					for (int i = 1; i < iter->second->image_c; i++)
					{
						float feature[512];
						for (int j = 0; j < 512; j++)
						{
							feature[j] = root["gsafety"][i][j].asFloat();
						}
						float sim = getSimilarity(iter->second->featue, feature);
						if (sim >= Standard)
						{
							maxsim = sim > maxsim ? id = i,sim : maxsim;
						}
					}
				}
				wroot["uuid"] = Json::Value(uuid);
				//send HTTP_DATA_PROCESS_OK
				wroot["status"] = Json::Value(HTTP_DATA_PROCESS_OK);
				wroot["sim"] = Json::Value(maxsim);
				wroot["id"] = Json::Value(id);
				sendmsg = fw.write(wroot);
			}
			else
			{
				//send HTTP_RECEIVE_DATA_PARSE_ERR
				sendmsg = senderTostring(uuid, HTTP_RECEIVE_DATA_PARSE_ERR);
			}
		}
		catch(Json::LogicError& err)
		{
			//send HTTP_RECEIVE_DATA_PARSE_ERR
			sendmsg = senderTostring(uuid, HTTP_RECEIVE_DATA_PARSE_ERR);
		}
		mg_printf(iter->second->m_nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
		mg_printf_http_chunk(iter->second->m_nc, sendmsg.c_str());
		mg_send_http_chunk(iter->second->m_nc, "", 0);
		iter->second->process_status = true;
	}
	return nullptr;
}

/*
 * network card name get ip address
 */
std::string GetHostIp(std::string eth_name)
{
	//包含以下MAC地址的前8个字节（前3段）是虚拟网卡
	//"00:05:69"; //vmware1
	//"00:0C:29"; //vmware2
	//"00:50:56"; //vmware3
	//"00:1c:14"; //vmware4
	//"00:1C:42"; //parallels1
	//"00:03:FF"; //microsoft virtual pc
	//"00:0F:4B"; //virtual iron 4
	//"00:16:3E"; //red hat xen , oracle vm , xen source, novell xen
	//"08:00:27"; //virtualbox

	register int fd;
	struct ifreq ifr;

	string ipaddress = "0.0.0.0";

	if (eth_name.empty())
	{
		return ipaddress;
	}
	if ((fd=socket(AF_INET, SOCK_DGRAM, 0)) > 0)
	{
		strcpy(ifr.ifr_name, eth_name.c_str());
		if (!(ioctl(fd, SIOCGIFADDR, &ifr)))
		{
			ipaddress = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
		}
		else
			ipaddress = "network info get error! ";
	}
	if (fd > 0)
	{
		close(fd);
	}

	return ipaddress;
}

//get the current time
std::string getCurrentTime()
{
	std::string defaultTime = "1970-01-01 00:00:00";
	try
	{
		struct timeval curTime;
		gettimeofday(&curTime, NULL);
		int milli = curTime.tv_usec / 1000;

		char buffer[80] = {0};
		struct tm nowTime;
		localtime_r(&curTime.tv_sec, &nowTime);//把得到的值存入临时分配的内存中，线程安全
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &nowTime);

		char currentTime[84] = {0};
		snprintf(currentTime, sizeof(currentTime), "%s%03d", buffer, milli);

		return buffer;
	}
	catch (...)
	{
		return defaultTime;
	}
}

//std::string proto_model_dir = "/home/ai_002/dwj-Qt/faceSDK/mtcnn-model/";
//std::string tensorflow_model_dir = "tensorflow/mars-small128.pb";
int init_daemon(void)
{
	int pid;
	int i;

	//忽略终端I/O信号，STOP信号
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
	signal(SIGHUP,SIG_IGN);

	pid = fork();
	if(pid > 0) {
		exit(0); //结束父进程，使得子进程成为后台进程
	}
	else if(pid < 0) {
		return -1;
	}

	//建立一个新的进程组,在这个新的进程组中,子进程成为这个进程组的首进程,以使该进程脱离所有终端
	setsid();

	//再次新建一个子进程，退出父进程，保证该进程不是进程组长，同时让该进程无法再打开一个新的终端
	pid=fork();
	if( pid > 0) {
		exit(0);
	}
	else if( pid< 0) {
		return -1;
	}

	//关闭所有从父进程继承的不再需要的文件描述符
	for(i=0;i< NOFILE;close(i++));

	//改变工作目录，使得进程不与任何文件系统联系
	//chdir("/home/gs-cv/");

	//将文件当时创建屏蔽字设置为0
	umask(0);

	//忽略SIGCHLD信号
	signal(SIGCHLD,SIG_IGN);

	return 0;
}

int main(int argc, char *argv[])
{
	cout << "!!!face HttpServer!!!" << endl;
	std::string port = "8080";
	std::string network = "lo";
	int gpu_num = 0;

	if (argc <= 1)
	{
		cout << "****> gsFaceDetectHttpServer " << endl;
		cout << "****> gcc version: gcc (Ubuntu 18.04.1) 7.4.0" << endl;
		cout << "****> IP Address default 127.0.0.1:8080" << endl;
		cout << "****> cuda version: 10.0; tensorflow version: r1.14" << endl;
		cout << "****> model_path: /home/ai_002/dwj-Qt/faceSDK/mtcnn-model/" << endl;
		cout << "argv:" << endl;
		cout << "        -t    thread_num                (type:int,default=1)" << endl;
		cout << "        -p    model_path                (type:int,default=1)" << endl;
		cout << "        -l    listen port               (type:int,default=8080)" << endl;
		cout << "        -e    network card name         (type:string,default=localhost)" << endl;
		cout << "        -d    gpu device num            (type:int,default=0)" << endl;
		cout << "Example:" << endl << endl;
		cout << ">>>>>>>>>>>>>  <<<<<<<<<<<<<" << endl;
		cout << "****> ./gsFaceDetectHttpServer -e eth0 -t 3 -d 1 &" << endl << endl;
		return 0;
	}

	for (int i = 1; i < argc; i++)
	{
		if (string(argv[i]) == "-t")
		{
			thread_num = atoi(argv[i + 1]);
		}
		if (string(argv[i]) == "-p")
		{
			proto_model_dir = argv[i + 1];
		}
		if (string(argv[i]) == "-l")
		{
			port = argv[i + 1];
		}
		if (string(argv[i]) == "-e")
		{
			network = argv[i + 1];
		}
		if (string(argv[i]) == "-d")
		{
			gpu_num = atoi(argv[i + 1]);
		}
	}

	std::string ip = GetHostIp(network);

	if (!ip.compare("0.0.0.0"))
	{
		cout << "Network card resolution error" << endl;
		return -1;
	}

	ip += ":"; ip += port;

	try {
		ConnJDBC mysql;
		mysql.selectDataBase("use sys");
		auto faceData = mysql.setQueryCommand("select * from FaceData");
		while (faceData->next())
		{
			FeatureData fq;
			Json::Value val;
			Json::Reader reader;

			fq.id = faceData->getString("FaceID");
			fq.date = faceData->getString("FaceDate");

			std::string feature = faceData->getString("FaceFeature");
			if (!reader.parse(feature, val))
				continue;

			if (val.size() != 512)
				continue;

			for (int i = 0; i < 512; ++i)
			{
				fq.feature[i] = val[i].asFloat();
			}

			feature_queue.push_back(fq);
		}
		//cout << feature_queue.size() << endl;
	}
    catch (sql::SQLException &e)
    {
        cout << e.what() << ",state:" << e.getSQLState() << "\nerrorCode: " << e.getErrorCode() << endl;
        return -1;
    }

    //create thread pool
	for(int i = 0; i < thread_num; ++i)
	{
		std::shared_ptr<tid_sd> data = std::make_shared<tid_sd>();
		data->gpu_id = gpu_num;
		int result = socketpair(PF_UNIX, SOCK_STREAM/*SOCK_SEQPACKET*/, 0, data->sockfd);
		if(-1 == result)
		{
			cout << "thread pool pipeline error!" << endl;
			return -1;
		}
		//SDL_Thread *sdl = SDL_CreateThread(sdl_functions,"123", (void*)&global);
		//参数依次是：创建的线程id，线程参数，调用的函数，传入的函数参数

		int ret = pthread_create(&data->tid, NULL, sdl_functions, data.get());
		if (ret != 0)
		{
			cout << "thread <sdl_functions> create error: error_code=" << ret << endl;
		}

		ret = pthread_create(&data->ftid, NULL, fsdl_functions, data.get());
		if (ret != 0)
		{
			cout << "thread <fsdl_functions> create error: error_code=" << ret << endl;
		}
		tid_socket.push_back(data);
	}

	/*
	 * add feature thread
	 */
	int result = socketpair(PF_UNIX, SOCK_STREAM/*SOCK_SEQPACKET*/, 0, feature_fd_map.sockfd);
	if(-1 == result)
	{
		cout << "add feature thread pipeline error!" << endl;
		return -1;
	}
	int ret = pthread_create(&feature_fd_map.ftid, NULL, afsdl_functions, &feature_fd_map);
	if (ret != 0)
	{
		cout << "feature thread <afsdl_functions> create error: error_code=" << ret << endl;
	}

	auto http_server = HttpServer(ip);

	// add handler
	http_server.AddHandler("/getFaceALL", handle_getFaceALL);
	http_server.AddHandler("/getFaceFeature", handle_getFaceFeature);
	http_server.AddHandler("/setFaceFeature", handle_setFaceFeature);
	http_server.AddHandler("/getFaceFeatureCompare", handle_getFaceFeatureCompare);
	http_server.AddHandler("/getFaceAgeSex", handle_getFaceAgeSex);
	http_server.AddHandler("/getFaceTrackID", handle_getFaceTrackID);

	//http_server.RemoveHandler("/api/getFaceAgeSex");
	//http_server.RemoveHandler("/api/getFaceFeature");

    http_server.Start("enp4s0");

    return 0;
}
