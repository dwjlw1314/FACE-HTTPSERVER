/*
 * HttpServer.h
 *
 *  Created on: Dec 12, 2019
 *      Author: ai_002
 */

#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_

#include "MatData.h"
#include "json/json.h"
//#include "Message.pb.h"

#include <fstream>
#include <unordered_map>
#include <functional>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <memory>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

#define THREAD_EXIT "10101010101010"
#define BUFFER_SIZE (40)	//定义一个缓冲区的大小
#define USED_VECTOR_FEATURE 1
#define USED_MYSQL_FEATURE 0
#define USED_REDIS_FEATURE 0

#define KEY_MAX_SIZE 20

//定义http返回callback
typedef void OnRspCallback(http_conn *nc, std::string);
//定义http请求handler
using ReqHandler = std::function<bool (http_conn *nc, http_message *http_req, OnRspCallback)>;

class HttpServer;

typedef struct tid_sd
{
	int gpu_id;
	pthread_t ftid;
	pthread_t tid;
	int sockfd[2];
}tid_sd;

typedef struct FeatureData
{
	std::string id;
	std::string date;
	float feature[512];
}FeatureData;

class HttpServer
{
public:
	HttpServer(std::string, http_opts* = nullptr);
	~HttpServer();

    void Init(http_opts*);
    bool Start(std::string);
    bool Close();

    //std::string GetHostIp(std::string);  //move to gsFaceDetectHttpServer.cpp

    //注册事件处理函数
    void AddHandler(const std::string &url, ReqHandler req_handler);
    //移除时间处理函数
    void RemoveHandler(const std::string &url);

    //回调函数映射表
    static std::unordered_map<std::string, ReqHandler> m_handler_map;

private:
	// 静态事件响应函数
	static void http_event_handler(http_conn*, int, void*);
	static void HandleEvent(http_conn *connection, http_message *http_req);
	static void SendRsp(http_conn *connection, std::string rsp);
	static void date_queue_clear();
	static void feature_map_clear();

	// 文件方式上传函数
	//static void handle_upload(http_conn*, int, void*);

private:
	std::string m_port;    //端口
	std::string m_web_dir = "./"; // 网页根目录
	mg_mgr m_mgr;   //连接管理器
	http_opts m_http_server_opts;  //web服务器选项参数
	http_conn *m_nc;  //连接实例对象
};

#endif /* HTTPSERVER_H_ */
