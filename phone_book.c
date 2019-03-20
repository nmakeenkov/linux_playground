#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "record.h"

MODULE_LICENSE("GPL");

static int deviceOpen(struct inode* inode, struct file* file);
static int deviceRelease(struct inode* inode, struct file* file);
static ssize_t deviceRead(struct file* flip, char* buffer, size_t len, loff_t* offset);
static ssize_t deviceWrite(struct file* flip, const char* buffer, size_t len, loff_t* offset);

struct file_operations fops =
{
    open : deviceOpen,
    release : deviceRelease,
    read : deviceRead,
    write : deviceWrite,
    owner : THIS_MODULE
};

int majorNum;

struct list_head records;

static int __init init(void)
{
    INIT_LIST_HEAD(&records);
    majorNum = register_chrdev(0, "phone_book", &fops);
    if (majorNum < 0)
    {
        printk(KERN_ALERT "Could not register device: %d\n", majorNum);
        return majorNum;
    }
    else
    {
        printk(KERN_INFO "phone_book device major number %d\n", majorNum);
        return 0;
    }
}

void clearRecords(void)
{
    struct list_head* cur;
    struct list_head* cur_safe;
    struct Records* entry;
    list_for_each_safe(cur, cur_safe, &records)
    {
        entry = list_entry(cur, struct Records, list);
        kfree(entry->record.name.chars);
        kfree(entry->record.data.chars);
        list_del(cur);
    }
}

static void __exit exit(void)
{
    unregister_chrdev(majorNum, "phone_book");
    clearRecords();
}

#define COMMAND_MAX_SIZE 1023
char command[COMMAND_MAX_SIZE];
size_t curCommandIdx = 0;

char response[2 * COMMAND_MAX_SIZE];
size_t curResponseIdx = 0;
size_t curResponseSize = 0;

static int deviceOpen(struct inode* inode, struct file* file)
{
    try_module_get(THIS_MODULE);
    return 0;
}

static int deviceRelease(struct inode* inode, struct file* file)
{
    module_put(THIS_MODULE);
    return 0;
}

void addRecord(size_t dataStart, size_t dataEnd, size_t nameStart, size_t nameEnd)
{
    struct Record newRecord;
    newRecord.data.chars = (char*) kmalloc(dataEnd - dataStart + 1, GFP_KERNEL);
    memcpy(newRecord.data.chars, command + dataStart, dataEnd - dataStart);
    newRecord.data.chars[dataEnd - dataStart] = '\0';
    newRecord.name.chars = (char*) kmalloc(nameEnd - nameStart + 1, GFP_KERNEL);
    memcpy(newRecord.name.chars, command + nameStart, nameEnd - nameStart);
    newRecord.name.chars[nameEnd - nameStart] = '\0';
    
    struct Records* node = (struct Records*) kmalloc(sizeof(struct Records), GFP_KERNEL);
    node->record = newRecord;
    INIT_LIST_HEAD(&node->list);
    list_add(&node->list, &records);
}

#define NOT_FOUND "person not found\n"

void addString(const char* string)
{
    while (*string)
    {
        response[curResponseSize++] = (*string++);
    }
}

void addResponse(const struct Record* record)
{
    addString(record->name.chars);
    addString(" ");
    addString(record->data.chars);
    addString("\n");
}

char tmp[COMMAND_MAX_SIZE + 1];
void addNotFoundResponse(size_t nameStart, size_t nameEnd)
{
    memcpy(tmp, command + nameStart, nameEnd - nameStart);
    tmp[nameEnd - nameStart] = '\0';
    addString(tmp);
    addString(" ");
    addString(NOT_FOUND);
}

void findRecord(size_t nameStart, size_t nameEnd)
{
    struct list_head* cur;
    struct Records* entry;
    for (cur = records.next; cur != &records; cur = cur->next)
    {
        entry = list_entry(cur, struct Records, list);
        if (memcmp(entry->record.name.chars, command + nameStart, nameEnd - nameStart) == 0)
        {
            addResponse(&entry->record);
            return;
        }
    }
    addNotFoundResponse(nameStart, nameEnd);
}

void removeRecord(size_t nameStart, size_t nameEnd)
{
    struct list_head* cur;
    struct Records* entry;
    for (cur = records.next; cur != &records; cur = cur->next)
    {
        entry = list_entry(cur, struct Records, list);
        if (memcmp(entry->record.name.chars, command + nameStart, nameEnd - nameStart) == 0)
        {
            list_del(cur);
            return;
        }
    }
}

void cleanFirstSymbols(size_t symbolCount)
{
    size_t i;
    for (i = symbolCount; i < curCommandIdx; ++i)
    {
        command[i - symbolCount] = command[i];
    }
    curCommandIdx -= (curCommandIdx > symbolCount ? symbolCount : curCommandIdx);
}

void cleanFirstCommand(void)
{
    size_t i;
    for (i = 0; i < curCommandIdx && command[i] != ';'; ++i);
    cleanFirstSymbols(i + 1);
}

#define isspace(c) ((c) == ' ' || (c) == '\n' || (c) == '\t')
void trimCommand(void)
{
    size_t symbolCount = 0;
    while (symbolCount < curCommandIdx && isspace(command[symbolCount]))
    {
        ++symbolCount;
    }
    if (symbolCount > 0)
    {
        cleanFirstSymbols(symbolCount);
    }
}

void tryExecuteCommand(void)
{
    trimCommand();
    if (curCommandIdx > 1 && (command[0] == 'a' || command[0] == 'f' || command[0] == 'd') && command[1] == ' ')
    {
        // add or find
        size_t dataStart = 2;
        size_t dataEnd = 2;
        size_t nameStart;
        size_t nameEnd;
        if (command[0] == 'a')
        {
            while (dataEnd < curCommandIdx && command[dataEnd] != ' ')
            {
                ++dataEnd;
            }
            nameStart = dataEnd + 1;
        }
        else
        {
            nameStart = 2;
        }
        nameEnd = nameStart;
        while (nameEnd < curCommandIdx && command[nameEnd] != ';')
        {
            ++nameEnd;
        }
        if (nameEnd >= curCommandIdx)
        {
            cleanFirstCommand();
            return;
        }
        if (command[0] == 'a')
        {
            addRecord(nameStart, nameEnd, dataStart, dataEnd);
        }
        else if (command[0] == 'f')
        {
            findRecord(nameStart, nameEnd);
        }
        else
        {
            removeRecord(nameStart, nameEnd);
        } 
    }
    cleanFirstCommand();
}

static ssize_t deviceRead(struct file* flip, char* buffer, size_t len, loff_t* offset)
{
    while (curCommandIdx > 0 && curResponseIdx >= curResponseSize)
    {
        tryExecuteCommand();
    }
    ssize_t bytesRead = 0;
    while (--len >= 0 && curResponseIdx < curResponseSize)
    {
        put_user(response[curResponseIdx++], buffer++);
        ++bytesRead;
    }

    if (curResponseIdx == curResponseSize)
    {
        curResponseSize = 0;
        curResponseIdx = 0;
    }

    return bytesRead;
}

static ssize_t deviceWrite(struct file* flip, const char* buffer, size_t len, loff_t* offset)
{
    // need to read the response before writing next request
    if (curResponseSize > 0)
    {
        return 0;
    }
    size_t i;
    for (i = 0; i < len; ++i)
    {
        command[curCommandIdx] = buffer[i];
        if (++curCommandIdx == COMMAND_MAX_SIZE)
        {
            tryExecuteCommand();
        }
    }
    return len;
}

module_init(init);
module_exit(exit);

