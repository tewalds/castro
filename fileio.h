
#pragma once

#include <cstdio>
#include <string>

int fpeek(FILE * fd);
void eat_whitespace(FILE * fd);
void eat_char(FILE * fd, int expect);
std::string read_until(FILE * fd, char until, bool include = false);

