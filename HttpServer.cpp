/*
 * HttpServer.cpp
 *
 *  Created on: Dec 12, 2019
 *      Author: ai_002
 */

#include "HttpServer.h"

extern http_opts g_http_server_opts;
std::string g_web_dir = "/home/gs-cv/"; //网页根目录

//std::map<std::string, std::shared_ptr<MatData> > data_queue;
std::map<std::string, MatData> date_queue;
std::map<std::string, std::shared_ptr<MatData> > feature_map;

void HttpServer::AddHandler(const std::string &url, ReqHandler req_handler)
{
    if (m_handler_map.find(url) != m_handler_map.end())
        return;

    m_handler_map.insert(std::make_pair(url, req_handler));
}

void HttpServer::RemoveHandler(const std::string &url)
{
    auto it = m_handler_map.find(url);
    if (it != m_handler_map.end())
    	m_handler_map.erase(it);
}

HttpServer::HttpServer(std::string port, http_opts* opts)
 : m_nc(nullptr)
{
	// TODO Auto-generated constructor stub
	m_port = port;

	if (!opts)
	{
		m_http_server_opts.enable_directory_listing = "yes";
		m_http_server_opts.document_root = g_web_dir.c_str();
		m_http_server_opts.index_files = "index.html";
		// TODO：其他设置
	}
	else
	{
		Init(opts);
	}
	g_http_server_opts = m_http_server_opts;
	mg_mgr_init(&m_mgr, NULL);
	m_nc = mg_bind(&m_mgr, m_port.c_str(), HttpServer::http_event_handler);
}

HttpServer::~HttpServer()
{
	// TODO Auto-generated destructor stub
	mg_mgr_free(&m_mgr);
}

void HttpServer::Init(http_opts* opt)
{
	m_http_server_opts = *opt;
}

bool HttpServer::Start(std::string eth_name)
{
	//std::string name = GetHostIp(eth_name);

    if (m_nc == NULL)
    {
    	cout << "mg_bind error" << endl;
        return false;
    }

    mg_set_protocol_http_websocket(m_nc);
    //cout << "starting http server listen address: " << name << ":" << m_port << endl;
    cout << "starting http server listen address: " << m_port << endl;

    //注册回调处理函数
    //mg_register_http_endpoint(m_nc, "/upload", handle_upload MG_UD_ARG(NULL));

    // loop
    while (true)
        mg_mgr_poll(&m_mgr, 500); // ms

    return true;
}

bool HttpServer::Close()
{
    mg_mgr_free(&m_mgr);
    return true;
}

void HttpServer::date_queue_clear()
{
	auto iter = date_queue.begin();
	while(iter != date_queue.end())
	{
		if (iter->second.process_status)
		{
			date_queue.erase(iter++);
		}
		else
			iter++;
	}
}

void HttpServer::feature_map_clear()
{
	auto iter = feature_map.begin();
	while(iter != feature_map.end())
	{
		if (iter->second->process_status)
		{
			feature_map.erase(iter++);
		}
		else
			iter++;
	}
}

/*
 * query_string type=0 all; type=1 box; type=2 feature; type 3 AgeSex; type=4 box&feature
 */
void HttpServer::http_event_handler(http_conn *nc, int event_type, void *event_data)
{
	if (event_type == MG_EV_POLL)
	{
		//此事件可用于执行任何内务处理，例如检查某个超时是否过期并关闭连接或发送心跳消息等
		//cout << "http-->event_type: " << "MG_EV_POLL" << endl;
	}
//	if (ev == MG_EV_HTTP_REQUEST)
//	{
//		struct http_message *hm = (struct http_message *) p;
//		// We have received an HTTP request. Parsed request is contained in `hm`.
//		// Send HTTP reply to the client which shows full original request.
//		mg_send_head(c, 200, hm->message.len, "Content-Type: text/plain");
//		mg_printf(c, "%.*s", (int)hm->message.len, hm->message.p);
//	}
	else if (event_type == MG_EV_HTTP_REQUEST)
	{
		http_message *http_req = (http_message *)event_data;
		//文件上传可以启用该函数，如果传输数据打开，客户端postman接收不到应答
	    //mg_serve_http(nc, http_req, g_http_server_opts);

		HandleEvent(nc, http_req);
		//mbuf_clear(&nc->recv_mbuf);
		mbuf_remove(&nc->recv_mbuf, nc->recv_mbuf.len);
	}
	else if (event_type == MG_EV_RECV)
	{
		date_queue_clear();
		feature_map_clear();
	}
	else if (event_type == MG_EV_ACCEPT)
	{
		//mbuf_resize(&nc->recv_mbuf,2000*2000*3);
		union socket_address* psAdd = (union socket_address*)event_data;
		if (psAdd->sin.sin_family == AF_INET) //is a valid IP4 Address
		{
			void *tmpAddrPtr = &psAdd->sin.sin_addr;
			char addressBuffer[INET_ADDRSTRLEN] = {0};
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			cout << "peer IP: " << addressBuffer << "  port: " << psAdd->sin.sin_port << endl;
		}
		else if (psAdd->sin.sin_family == PF_INET6)
		{
//			check it is ip6 deal ip6 addr
//		    tmpaddrptr = &((struct sockaddr_in *)ifaddrstruct->ifa_addr)->sin_addr;
//		    char addressbuffer[INET6_ADDRSTRLEN];
//		    inet_ntop(af_inet6, tmpaddrptr, addressbuffer, INET6_ADDRSTRLEN);
		}
	}
	else if (event_type == MG_EV_CLOSE)
	{
		/*
		 * 一次链接断开后删除相关的nc数据
		 * 测试保留原始处理方式
		 */
		auto iter = date_queue.begin();
		while(iter != date_queue.end())
		{
			if (!iter->second.receive_status && nc == iter->second.m_nc)
			{
				date_queue.erase(iter++);
			}
			else
				iter++;
		}
		auto fiter = feature_map.begin();
		while(fiter != feature_map.end())
		{
			if (!fiter->second->receive_status && nc == fiter->second->m_nc)
			{
				feature_map.erase(fiter++);
			}
			else
				fiter++;
		}
	}
}

static bool route_check(http_message *http_msg, std::string route_prefix)
{
    // TODO: 还可以判断 GET, POST, PUT, DELTE等方法
    //mg_vcmp(&http_msg->method, "GET");
    //mg_vcmp(&http_msg->method, "POST");
    //mg_vcmp(&http_msg->method, "PUT");
    //mg_vcmp(&http_msg->method, "DELETE");

    if (mg_vcmp(&http_msg->uri, route_prefix.c_str()) == 0)
        return true;
    else
        return false;
}

void HttpServer::SendRsp(http_conn *connection, std::string rsp)
{
    // 必须先发送header
    mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    // 以json形式返回
    mg_printf_http_chunk(connection, rsp.c_str());
    // 发送空白字符快，结束当前响应
    mg_send_http_chunk(connection, "", 0);
}

void HttpServer::HandleEvent(http_conn *connection, http_message *http_req)
{
//    std::string req_str = std::string(http_req->message.p, http_req->message.len);
//    printf("got request: %s\n", req_str.c_str());

    // 先过滤是否已注册的函数回调
    std::string url = std::string(http_req->uri.p, http_req->uri.len);

    auto it = m_handler_map.find(url);
    if (it != m_handler_map.end())
    {
        ReqHandler handle_func = it->second;
        try {
        	handle_func(connection, http_req, SendRsp);
        }
        catch(Json::LogicError& err)
        {
        	//send HTTP_REQUEST_PARSE_ERR
        	std::string msg = "{ \"status\": 102 }";
        	SendRsp(connection, msg);
        }
		return;
    }

    // 其他请求
    if (route_check(http_req, "/test"))
    {
        // 直接回传
        SendRsp(connection, "Welcome Access Gsafety FaceDetected HttpServer");
    }
    else if (route_check(http_req, "/help"))
    {
		ifstream ifile;
		std::string msg;
		std::string path = g_http_server_opts.document_root;
		path += "index.html";
		ifile.open(path, ios::in);
		if(!ifile)
		{
			msg = "Open Help File False";
		}
		else
		{
			ifile.seekg(0, std::ios::end);
			size_t length = ifile.tellg();
			ifile.seekg(0, std::ios::beg);
			char *buffer = new char[length];
			ifile.read(buffer, length);
			msg = string(buffer, length);
		}
        SendRsp(connection, msg);
    }
    else if (route_check(http_req, "/log"))
    {
		ifstream ifile;
		std::string msg;
		std::string path = g_http_server_opts.document_root;
		path += "faceInfo.html";
		ifile.open(path, ios::in);
		if(!ifile)
		{
			msg = "Dont't log File!";
		}
		else
		{
			ifile.seekg(0, std::ios::end);
			size_t length = ifile.tellg();
			ifile.seekg(0, std::ios::beg);
			char *buffer = new char[length];
			ifile.read(buffer, length);
			msg = string(buffer, length);
		}
        SendRsp(connection, msg);
    }
    else if (route_check(http_req, "/clear"))
    {
		Logger::clear();
		std::string msg = "clear log success!!";
    	SendRsp(connection, msg);
    }
    else
    {
    	std::string msg = "HTTP/1.1 Sorry , Not Implemented";
    	SendRsp(connection, msg);
    }
}
