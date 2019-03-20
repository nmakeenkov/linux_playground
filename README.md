# Phone book

A linux kernel module with simple phone book implementation.

Usage example:
```
>>> make all >/dev/null 2>/dev/null && sudo insmod phone_book.ko && dmesg | tail -n 1
[22831.160724] phone_book device major number 243
>>> sudo mknod /dev/phone_book c 243 0
>>> sudo chmod 666 /dev/phone_book
>>> echo "a Nikolai data bla bla bla;a Someone other data;" > /dev/phone_book
>>> echo "f Nikolai;" > /dev/phone_book
>>> cat /dev/phone_book
Nikolai data bla bla bla
>>> echo "d Nikolai;" > /dev/phone_book
>>> echo "f Nikolai;" > /dev/phone_book
>>> cat /dev/phone_book
Nikolai person not found
```
