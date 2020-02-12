/*
 * MatData.h
 *
 *  Created on: Dec 18, 2019
 *      Author: ai_002
 */

#ifndef MATDATA_H_
#define MATDATA_H_

#include <iostream>

#include "opencv2/opencv.hpp"
using namespace std;
using namespace cv;

#include <mongoose.h>

using http_opts = struct mg_serve_http_opts;
using http_conn = struct mg_connection;

typedef enum ERRTYPE
{
	HTTP_REQUEST_CREATE_OK,
	HTTP_DATA_PROCESS_OK,
	HTTP_DATA_RECEIVE_WAITTING = 100,
	HTTP_DATA_RECEIVE_FINISH,
	HTTP_REQUEST_PARSE_ERR,
	HTTP_RECEIVE_DATA_FORMAT_ERR,
	HTTP_RECEIVE_UUID_ERR,
	HTTP_REQUEST_METHOD_ERROR,
	HTTP_RECEIVE_DATA_SIZE_ERR,
	HTTP_RECEIVE_DATA_PARSE_ERR,

	HTTP_DATA_FACERESULT_ERROR = 200,
	HTTP_DATA_FACEALL_OK,
	HTTP_DATA_FACEBOXFEATURE_OK,
	HTTP_DATA_GENDER_OK
}ERRTYPE;

class MatData
{
public:
	MatData();

	~MatData();

	void clear();

public:

	//1 , 2 , 3 , 4
	int request_type;

	int64 m_startTime;

	//specify thread process
	pthread_t process_thread_num;

	//socket pair
	int write_fd;

	int image_w;

	int image_h;

	int image_c;

	//frame Mat data format
	std::string data;

	float featue[512];

	//total data size
	size_t total_lenth;

	//already receive size
	size_t recv_lenth;

	//peer connect pointer
	http_conn *m_nc;

	//data receive finish flag
	bool receive_status;

	//use status
	volatile bool process_status;
};

#endif /* MATDATA_H_ */
