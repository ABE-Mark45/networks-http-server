#pragma once
#include <string>
#include <iostream>
#include "common.h"

class Response
{
public:
    std::string http_version;
    std::string rest;
    int status_code;
    int file_length;

    Response(std::string response_line)
    {
        file_length = 0;
        if (response_line.empty())
            return;
        std::string tmp;
        int status_code_start, status_message_start;
        status_code_start = response_line.find(" ");
        status_message_start = response_line.find(" ", status_code_start + 1);
        http_version = response_line.substr(0, status_code_start);
        status_code = std::stoi(response_line.substr(status_code_start + 1, status_message_start - status_code_start));

        int l = response_line.find("\r\n", 0) + 2;
        int r;

        while (true)
        {
            r = response_line.find("\r\n", l);

            if (r == std::string::npos || l == r)
            {
                l += 2;
                break;
            }
            tmp = response_line.substr(l, r - l);
            l = r + 2;

            int colon = tmp.find(":");
            std::string key = tmp.substr(0, colon);
            for (char &c : key)
                c = tolower(c);
            if (key == "content-length")
            {
                std::string value = tmp.substr(colon + 2);
                file_length = std::stoi(value);
            }
        }
        rest = response_line.substr(l);
    }
};