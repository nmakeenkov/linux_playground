#pragma once

struct String
{
    char* chars;
};

struct Record
{
    struct String name;
    struct String number;
};

struct Records
{
    struct Record record;
    struct Records* next;
};

