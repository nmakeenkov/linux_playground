#pragma once
#include <linux/list.h>

struct String
{
    char* chars;
};

struct Record
{
    struct String name;
    struct String data;
};

struct Records
{
    struct Record record;
    struct list_head list;
};
