# virtual-file-system


## Overview
This project is a **user-mode FAT32-like file system** implemented in C.  
It operates on a disk image or a physical drive, with 512-byte blocks and a 4096-entry FAT.  

---

## Features
- **Disk Management**
  - `./myfs disk -format` → Initialize disk with empty FAT and file list.  

- **File Operations**
  - `-write` → Copy a file from host to disk.  
  - `-read` → Copy a file from disk to host.  
  - `-delete` → Remove a file from disk.  
  - `-rename` → Rename a file in the disk.  
  - `-duplicate` → Create a copy with `_copy` suffix.  

- **Metadata Operations**
  - `-list` → List all visible files.  
  - `-sorta` → Sort files by size (ascending).  
  - `-search` → Search if a file exists.  
  - `-hide` / `-unhide` → Toggle hidden state.  

- **Debug & Maintenance**
  - `-printfilelist` → Export file list to `filelist.txt`.  
  - `-printfat` → Export FAT table to `fat.txt`.  
  - `-defragment` → Compact fragmented files for efficiency.  

---

