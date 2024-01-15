#include <stdio.h>
#include "inode.h"
#include "call.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

const char * HD = "HD";

inode * read_inode(int fd, int i_number) {
  inode * ip = malloc(sizeof(inode));
  int currpos = lseek(fd, I_OFFSET + i_number * sizeof(inode), SEEK_SET);
  if (currpos < 0) {
    printf("Error: lseek()\n");
    return NULL;
  }

  //read inode from disk
  int ret = read(fd, ip, sizeof(inode));
  if (ret != sizeof(inode)) {
    printf("Error: read()\n");
    return NULL;
  }
  return ip;
}

DIR_NODE read_dir_entry(int fd, inode * current_inode, int entry_index) {
  DIR_NODE dir_entry;

  // Check if entry_index is within the range of sub-files
  if (entry_index < 0 || entry_index >= current_inode -> sub_f_num) {
    fprintf(stderr, "Error: entry_index out of range\n");
    exit(EXIT_FAILURE);
  }

  // Calculate the position of the directory entry in the data block
  off_t entry_pos = D_OFFSET + current_inode -> direct_blk[0] * BLK_SIZE + entry_index * sizeof(DIR_NODE);

  // Seek to the directory entry's position
  if (lseek(fd, entry_pos, SEEK_SET) < 0) {
    perror("Error seeking to directory entry");
    exit(EXIT_FAILURE);
  }

  // Read the directory entry
  if (read(fd, & dir_entry, sizeof(DIR_NODE)) != sizeof(DIR_NODE)) {
    perror("Error reading directory entry");
    exit(EXIT_FAILURE);
  }

  return dir_entry;
}

int open_t(char * pathname) {
  int inode_number;
  // write your code here.
  int fd = open("./HD", O_RDONLY);
  if (fd < 0) {
    perror("Error opening HD file");
    return -1;
  }

  int current_inode_number = 0;

  char * token = strtok(pathname, "/");
  inode * current_inode = NULL;

  while (token != NULL) {
    current_inode = read_inode(fd, current_inode_number);
    if (current_inode == NULL) {
      close(fd);
      return -1;
    }
    // Check if the current inode is a directory
    if (current_inode -> f_type != DIR) {
      close(fd);
      return -1;
    }

    // Find the next inode number from the directory's entries
    int found = 0;
    for (int i = 0; i < current_inode -> sub_f_num; i++) {
      // Read the directory entry 
      DIR_NODE dir_entry = read_dir_entry(fd, current_inode, i); 

      if (strcmp(dir_entry.f_name, token) == 0) {
        // Found the next component in the path
        current_inode_number = dir_entry.i_number;
        found = 1;
        break;
      }
    }
    if (!found) {
      // Did not find the next part of the pathname
      close(fd);
      return -1;
    }
    // Move to the next part of the pathname
    token = strtok(NULL, "/");

  }
  close(fd);
  return current_inode_number;
}

int min(int a, int b) {
  return (a < b) ? a : b;
}
int get_inode_block_number(inode * in, int block_number) {
  // Check requested block number avaliable db
  if (block_number >= sizeof(in -> direct_blk) / sizeof(in -> direct_blk[0])) {
    // out of the range
    printf("Error: Block number is out of range\n");
    return -1;
  }
  // Return the actual block number deom db
  return in -> direct_blk[block_number];
}
int read_block(int fd, int block, void * buf) {
  off_t offset = D_OFFSET + (off_t) block * BLK_SIZE;

  ssize_t bytes_read = pread(fd, buf, BLK_SIZE, offset);

  if (bytes_read < 0) {
    perror("Error reading block");
    return -1;
  }

  // Check may have reached the end of the file
  // or the block may not be fully used.
  if (bytes_read < BLK_SIZE) {
    // Zero out the remainder of the buffer
    memset((char * ) buf + bytes_read, 0, BLK_SIZE - bytes_read);
  }

  return bytes_read;
}
int read_t(int i_number, int offset, void * buf, int count) {
  int fd = open(HD, O_RDONLY);
  if (fd < 0) {
    perror("Error opening HD file");
    return -1;
  }
  inode * in = read_inode(fd, i_number);
  if (!in) {
    printf("Error: Unable to read inode\n");
    close(fd);
    return -1;
  }

  // If offset is beyond the end of the file, nothing to read
  if (offset >= in -> f_size) {
    close(fd);
    return 0;
  }

  // Adjust count if it's beyond the end of the file
  if (offset + count > in -> f_size) {
    count = in -> f_size - offset;
  }

  int count_read = 0;

  // Read direct blocks
  for (int i = 0; i < sizeof(in -> direct_blk) / sizeof(in -> direct_blk[0]) && count_read < count; ++i) {
    if (offset < BLK_SIZE) {
      int block_number = get_inode_block_number(in, i);
      if (block_number < 0) {
        close(fd);
        return -1;
      }
      int to_read = min(BLK_SIZE - offset, count - count_read);
      int n = pread(fd, buf + count_read, to_read, D_OFFSET + block_number * BLK_SIZE + offset);
      if (n < 0) {
        perror("Error reading block");
        close(fd);
        return -1;
      }
      count_read += n;
      offset = 0; // Reset offset for subsequent blocks
    } else {
      offset -= BLK_SIZE;
    }
  }

  // Read indirect blocks if needed
  if (in -> indirect_blk >= 0 && count_read < count) {
    int indirect[BLK_SIZE / sizeof(int)];
    if (read_block(fd, in -> indirect_blk, indirect) < 0) {
      // Handle error from read_block
      close(fd);
      return -1;
    }

    for (int i = 0; i < 1024 && count_read < count; ++i) {
      if (offset < BLK_SIZE) {
        int to_read = min(BLK_SIZE - offset, count - count_read);
        int n = pread(fd, buf + count_read, to_read, D_OFFSET + indirect[i] * BLK_SIZE + offset);
        if (n < 0) {
          perror("Error reading indirect block");
          close(fd);
          return -1;
        }
        count_read += n;
        if (n < to_read) {
          //hit the end of file
          break;
        }
        offset = 0; // Reset offset for the next block
      } else {
        offset -= BLK_SIZE;
      }
    }
  }

  close(fd);
  return count_read;
}

// you are allowed to create any auxiliary functions that can help your implementation. But only "open_t()" and "read_t()" are allowed to call these auxiliary functions.
