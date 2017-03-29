# Readers-writer scheduler in Linux kernel
kernel supported : 4.1.27

This project is about implementing a Reader's writer scheduler in the linux kernel as a module. The module schedules the Read-write request for resource access  coming from user space and grant them access to the resource based on reader's writer schedule. The Communication between user and kernel space is established using device files.
The Driver is extendible to provide another IPC mechanism between processes on the same machine viw select/poll system calls only.
