#pragma once
#include <string>
#include <iostream>
#include <unordered_map>
#include "common.h"

#define BUFFER_SIZE 1024

class Request
{

public:
    std::string http_version;
    std::string uri;
    Method method;
    int length;
    std::string rest;

    std::unordered_map<std::string, std::string> headers;

    Request(std::string request_line)
    {
        if (request_line.empty())
            return;
        std::string tmp;
        int uri_start = 0, http_version_start = 0;
        if (request_line.substr(0, 3) == "GET")
        {
            method = Method::GET;
            uri_start = 4;
        }
        else
        {
            method = Method::POST;
            uri_start = 5;
        }
        http_version_start = request_line.find(" ", uri_start);
        uri = request_line.substr(uri_start, http_version_start - uri_start);
        std::cout << (method == Method::GET? "GET ": "POST ") << uri << std::endl;
        int l = request_line.find("\r\n", 0) + 2;
        int r;
        http_version = request_line.substr(http_version_start + 1, l - http_version_start - 3);

        while (true)
        {
            r = request_line.find("\r\n", l);

            if (r == std::string::npos || l == r)
            {
                l = r+2;
                break;
            }
            tmp = request_line.substr(l, r - l);
            l = r + 2;

            int colon = tmp.find(":");
            std::string key = tmp.substr(0, colon);
            for(char& c: key)
                c = tolower(c);
            if(key == "content-length")
            {
                std::string value = tmp.substr(colon + 2);
                length = std::stoi(value);
            }
        }
        rest = request_line.substr(l);
    }

};
