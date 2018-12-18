# Phone book

A linux kernel module with simple phone book implementation.

Usage example:
```
>>> make all >/dev/null 2>/dev/null && sudo insmod phone_book.ko && dmesg | tail -n 1
[22831.160724] phone_book device major number 243
>>> sudo mknod /dev/phone_book c 243 0
>>> sudo chmod 666 /dev/phone_book
>>> echo "a 123456 First Person;a 654321 Second Person;" > /dev/phone_book
>>> echo "f First Person;" > /dev/phone_book
>>> cat /dev/phone_book
First Person 123456
>>> echo "f Somebody;" > /dev/phone_book
>>> cat /dev/phone_book
Somebody person not found
```
