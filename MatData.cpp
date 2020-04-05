/*
 * MatData.cpp
 *
 *  Created on: Dec 18, 2019
 *      Author: ai_002
 */

#include "MatData.h"

//#define use_c_memory

MatData::MatData()
	: m_nc(nullptr)
{
	clear();
}

MatData::~MatData()
{
	// TODO Auto-generated destructor stub
#ifdef use_c_memory
	delete data;
#endif
}

void MatData::clear()
{
	// TODO Auto-generated constructor stub
	process_status = false;
	receive_status = false;
	write_fd = -1;
	request_type = -1;
	recv_lenth = 0;
	total_lenth = 0;
	image_w = 0;
	image_h = 0;
	image_c = 0;
	process_thread_num = 0;
	data.clear();
}
