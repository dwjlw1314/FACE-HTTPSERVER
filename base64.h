/*
 * base64.h
 *
 *  Created on: Mar 31, 2020
 *      Author: ai_002
 */

#ifndef BASE64_H_
#define BASE64_H_

#include <vector>
#include <string>

typedef unsigned char BYTE;

class base64 {
public:
	base64();
	~base64();

	inline bool is_base64(BYTE c)
	{
		return (isalnum(c) || (c == '+') || (c == '/'));
	}

	std::string base64_encode(BYTE const*, unsigned int);
	std::string base64_decode(std::string const&);
};

#endif /* BASE64_H_ */
