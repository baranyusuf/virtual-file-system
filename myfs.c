#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

/* Constants */
#define FAT_ENTRIES   4096
#define FILE_ENTRIES  128
#define BLOCK_SIZE    512

/*── Function prototypes ────────────────────*/
void Format(const char *disk_path);
void Write(const char *disk_path, const char *srcPath, const char *destFileName);
void Read(const char *disk_path, const char *srcFileName, const char *destPath);
void Delete(const char *disk_path, const char *filename);
void List(const char *disk_path);
void Sort(const char *disk_path);
void RenameFile(const char *disk_path, const char *srcFileName, const char *newFileName);
void Duplicate(const char *disk_path, const char *srcFileName);
void Search(const char *disk_path, const char *srcFileName);
void Hide(const char *disk_path, const char *srcFileName);
void Unhide(const char *disk_path, const char *srcFileName);
void PrintFileList(const char *disk_path);
void PrintFAT(const char *disk_path);
void Defragment(const char *disk_path);

/* Dispatch based on argv */
int main(int argc, char *argv[]) {  /* less argument than expected */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <disk> <command> [args]\n", argv[0]);
        return 1;
    }
    const char *disk = argv[1];
    const char *cmd  = argv[2];

    if (strcmp(cmd, "-format") == 0) {
        Format(disk);
    }
    else if (strcmp(cmd, "-read") == 0 && argc == 5) {
        Read(disk, argv[3], argv[4]);
    }

    else if (strcmp(cmd, "-write") == 0 && argc == 5) {
        Write(disk, argv[3], argv[4]);
    }

    else if (strcmp(cmd, "-delete") == 0 && argc == 4) {
        Delete(disk, argv[3]);
    }
    
    else if (strcmp(cmd, "-list") == 0 && argc == 3) {
        List(disk);
    }

    else if (strcmp(cmd, "-sorta") == 0 && argc == 3) {
        Sort(disk);
    }

    else if (strcmp(cmd, "-rename") == 0 && argc == 5) {
        RenameFile(disk, argv[3], argv[4]);
    }
	
    else if (strcmp(cmd, "-printfat") == 0 && argc == 3) {
        PrintFAT(disk);
    }
	
    else if (strcmp(cmd, "-duplicate") == 0 && argc == 4) {
        Duplicate(disk, argv[3]);
    }

    else if (strcmp(cmd, "-search") == 0 && argc == 4) {
        Search(disk, argv[3]);
    }

    else if (strcmp(cmd, "-unhide") == 0 && argc == 4) {
        Unhide(disk, argv[3]);
    }

    else if (strcmp(cmd, "-hide") == 0 && argc == 4) {
        Hide(disk, argv[3]);
    }
	
    else if (strcmp(cmd, "-printfilelist") == 0 && argc == 3) {
        PrintFileList(disk);
    }
	
    else if (strcmp(cmd, "-defragment") == 0 && argc == 3) {
        Defragment(disk);
    }


    else {
        fprintf(stderr, "Unknown or malformed command\n");
        return 1;
    }

    return 0;
}

/* implement each function */

/**
 * Format the disk image:
 *  - Zero out the FAT region, except entry[0] = 0xFFFFFFFF
 *  - Zero out the 128-entry file list (each 256 bytes)
 */
void Format(const char *disk_path) {
    FILE *fp = fopen(disk_path, "r+b");
    if (!fp) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    // Make sure we start at the very beginning
    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("fseek failed");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    // 1) Write the FAT
    uint32_t entry;
    // 1a) First entry = 0xFFFFFFFF
    entry = 0xFFFFFFFF;
    if (fwrite(&entry, sizeof(entry), 1, fp) != 1) {
        perror("Failed to write FAT[0]");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    // 1b) Remaining FAT entries = 0x00000000
    entry = 0x00000000;
    for (int i = 1; i < FAT_ENTRIES; i++) {
        if (fwrite(&entry, sizeof(entry), 1, fp) != 1) {
            perror("Failed to write FAT entry");
            fclose(fp);
            exit(EXIT_FAILURE);
        }
    }

    // 2) Write the File List (128 entries × 256 bytes each = 32 768 bytes)
    //    Just zero them all out
    char zero_item[256] = {0};
    for (int i = 0; i < FILE_ENTRIES; i++) {
        if (fwrite(zero_item, sizeof(zero_item), 1, fp) != 1) {
            perror("Failed to write file-list entry");
            fclose(fp);
            exit(EXIT_FAILURE);
        }
    }

    fclose(fp);
    printf("Disk image \"%s\" formatted successfully.\n", disk_path);
}


/**
 * Write a host file into the disk image under a given name.
 */
void Write(const char *disk_path, const char *srcPath, const char *destFileName) {
    // Open source file
    FILE *src = fopen(srcPath, "rb");
    if (!src) { perror("Error opening source file"); exit(EXIT_FAILURE); }
    // Determine file size
    fseek(src, 0, SEEK_END);
    long filesize = ftell(src);
    fseek(src, 0, SEEK_SET);
    int blocks = (filesize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Open disk image
    FILE *disk = fopen(disk_path, "r+b");
    if (!disk) { perror("Error opening disk image"); fclose(src); exit(EXIT_FAILURE); }

    // Read FAT into memory
    uint32_t *fat = malloc(FAT_ENTRIES * sizeof(uint32_t));
    if (!fat) { perror("Allocating FAT"); fclose(src); fclose(disk); exit(EXIT_FAILURE); }
    fseek(disk, 0, SEEK_SET);
    fread(fat, sizeof(uint32_t), FAT_ENTRIES, disk);

    // Find empty blocks
    int *chain = malloc(blocks * sizeof(int));
    if (!chain) { perror("Allocating chain"); exit(EXIT_FAILURE); }
    int found = 0;
    for (int i = 1; i < FAT_ENTRIES && found < blocks; i++) {
        if (fat[i] == 0) chain[found++] = i;
    }
    if (found < blocks) {
        fprintf(stderr, "Not enough free space\n");
        free(fat); free(chain); fclose(src); fclose(disk);
        exit(EXIT_FAILURE);
    }
    // Update FAT entries
    for (int j = 0; j < blocks - 1; j++) fat[chain[j]] = chain[j+1];
    fat[chain[blocks - 1]] = 0xFFFFFFFF;

    // Write updated FAT back
    fseek(disk, 0, SEEK_SET);
    fwrite(fat, sizeof(uint32_t), FAT_ENTRIES, disk);

    // Write file data blocks
    char buffer[BLOCK_SIZE];
    for (int j = 0; j < blocks; j++) {
        size_t to_read = BLOCK_SIZE;
        if (j == blocks - 1 && filesize % BLOCK_SIZE) to_read = filesize % BLOCK_SIZE;
        fread(buffer, 1, to_read, src);
        // zero-pad remainder
        if (to_read < BLOCK_SIZE) memset(buffer + to_read, 0, BLOCK_SIZE - to_read);
        // compute start of data region once (e.g. at top of Write())
	size_t data_start = FAT_ENTRIES * sizeof(uint32_t) + FILE_ENTRIES * 256;

	// then for each block:
	off_t offset = (off_t)data_start + (off_t)chain[j] * BLOCK_SIZE;
        fseek(disk, offset, SEEK_SET);
        fwrite(buffer, 1, BLOCK_SIZE, disk);
    }

    // Update file list: find first free entry
    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    int slot = -1;
    for (int i = 0; i < FILE_ENTRIES; i++) {
        long pos = filelist_offset + i * 256 + 248;
        uint32_t firstBlock;
        fseek(disk, pos, SEEK_SET);
        fread(&firstBlock, sizeof(uint32_t), 1, disk);
        if (firstBlock == 0) { slot = i; break; }
    }
    if (slot < 0) {
        fprintf(stderr, "No free file-list entries\n");
        free(fat); free(chain); fclose(src); fclose(disk);
        exit(EXIT_FAILURE);
    }
    // Write file-list entry: name, first block, size
    long entryPos = filelist_offset + slot * 256;
    fseek(disk, entryPos, SEEK_SET);
    char namebuf[248] = {0};
    strncpy(namebuf, destFileName, 247);
    fwrite(namebuf, 1, 248, disk);
    uint32_t fb = chain[0];
    fwrite(&fb, sizeof(fb), 1, disk);
    uint32_t sz = (uint32_t)filesize;
    fwrite(&sz, sizeof(sz), 1, disk);

    printf("Copied '%s' -> '%s' (size: %ld bytes, %d blocks)\n", srcPath, destFileName, filesize, blocks);

    free(fat);
    free(chain);
    fclose(src);
    fclose(disk);
}


/**
 * Read a file from the disk image back to the destination.
 */
void Read(const char *disk_path, const char *srcFileName, const char *destPath) {
    FILE *disk = fopen(disk_path, "rb");
    if (!disk) { perror("Error opening disk image"); exit(EXIT_FAILURE); }

    // Load file list and find entry
    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    uint32_t firstBlock = 0, filesize = 0;
    for (int i = 0; i < FILE_ENTRIES; i++) {
        char namebuf[248] = {0};
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, 248, disk);
        if (strcmp(namebuf, srcFileName) == 0) {
            fread(&firstBlock, sizeof(firstBlock), 1, disk);
            fread(&filesize, sizeof(filesize), 1, disk);
            break;
        }
    }
    if (firstBlock == 0) { fprintf(stderr, "File not found: %s\n", srcFileName); fclose(disk); exit(EXIT_FAILURE); }

    // Load FAT
    uint32_t *fat = malloc(FAT_ENTRIES * sizeof(uint32_t));
    fseek(disk, 0, SEEK_SET);
    fread(fat, sizeof(uint32_t), FAT_ENTRIES, disk);

    // Open destination file
    FILE *dest = fopen(destPath, "wb");
    if (!dest) { perror("Error creating destination file"); exit(EXIT_FAILURE); }

    // Read chain of blocks
    size_t data_start = FAT_ENTRIES * sizeof(uint32_t) + FILE_ENTRIES * 256;
    uint32_t cur = firstBlock;
    size_t remaining = filesize;
    char buffer[BLOCK_SIZE];
    while (1) {
        off_t offset = data_start + cur * BLOCK_SIZE;
        fseek(disk, offset, SEEK_SET);
        size_t to_read = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;
        fread(buffer, 1, to_read, disk);
        fwrite(buffer, 1, to_read, dest);
        remaining -= to_read;
        if (fat[cur] == 0xFFFFFFFF) break;
        cur = fat[cur];
    }

    printf("Read '%s' (%u bytes) -> '%s'\n", srcFileName, filesize, destPath);

    fclose(dest);
    fclose(disk);
    free(fat);
}


/* Delete: remove file and free its blocks */
void Delete(const char *disk_path, const char *filename) {
    FILE *disk = fopen(disk_path, "r+b");
    if (!disk) { perror("Error opening disk image"); exit(EXIT_FAILURE); }

    // Load file list and locate entry
    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    uint32_t firstBlock = 0;
    int slot = -1;
    for (int i = 0; i < FILE_ENTRIES; i++) {
        char namebuf[248] = {0};
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, 248, disk);
        if (strcmp(namebuf, filename) == 0) {
            fread(&firstBlock, sizeof(firstBlock), 1, disk);
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fprintf(stderr, "File not found: %s\n", filename);
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    // Load FAT
    uint32_t *fat = malloc(FAT_ENTRIES * sizeof(uint32_t));
    fseek(disk, 0, SEEK_SET);
    fread(fat, sizeof(uint32_t), FAT_ENTRIES, disk);

    // Traverse and clear the chain
    uint32_t cur = firstBlock;
    while (cur != 0xFFFFFFFF) {
        uint32_t next = fat[cur];
        fat[cur] = 0;
	if (next == 0xFFFFFFFF)
		break;
        cur = next;
    }
    

    // Write updated FAT back
    fseek(disk, 0, SEEK_SET);
    fwrite(fat, sizeof(uint32_t), FAT_ENTRIES, disk);

    // Clear the file-list entry
    char zero_item[256] = {0};
    fseek(disk, filelist_offset + slot*256, SEEK_SET);
    fwrite(zero_item, sizeof(zero_item), 1, disk);

    fclose(disk);
    free(fat);
    printf("Deleted file '%s' successfully.\n", filename);
}

/* List: print all visible files and sizes */
void List(const char *disk_path) {
    FILE *disk = fopen(disk_path, "rb");
    if (!disk) { perror("Error opening disk image"); exit(EXIT_FAILURE); }
    
    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    for (int i = 0; i < FILE_ENTRIES; i++) {
        // Read filename
        char namebuf[248] = {0};
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, 248, disk);
        // Skip empty or hidden names
        if (namebuf[0] == '\0' || namebuf[0] == '.') continue;
        // Read size (at offset 248+4)
        uint32_t filesize;
        fseek(disk, filelist_offset + i*256 + 252, SEEK_SET);
        fread(&filesize, sizeof(filesize), 1, disk);
        printf("%s\t%u bytes\n", namebuf, filesize);
    }
    fclose(disk);
}



/*   Sort      */

/* short struct to hold name+size */
struct FileInfo {
    char     name[248];
    uint32_t size;
};

/* comparison for qsort: ascending by size */
static int compare_size(const void *a, const void *b) {
    const struct FileInfo *fa = a;
    const struct FileInfo *fb = b;
    if (fa->size < fb->size) return -1;
    if (fa->size > fb->size) return  1;
    return 0;
}

void Sort(const char *disk_path) {
    FILE *disk = fopen(disk_path, "rb");
    if (!disk) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    struct FileInfo files[FILE_ENTRIES];
    int count = 0;

    for (int i = 0; i < FILE_ENTRIES; i++) {
        char namebuf[248] = {0};
        uint32_t filesize;

        /* read filename */
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, 248, disk);

        /* skip empty or hidden */
        if (namebuf[0] == '\0' || namebuf[0] == '.')
            continue;

        /* read size at offset 252 (248 name + 4 firstBlock) */
        fseek(disk, filelist_offset + i*256 + 252, SEEK_SET);
        fread(&filesize, sizeof(filesize), 1, disk);

        /* store */
        strncpy(files[count].name, namebuf, 247);
        files[count].size = filesize;
        count++;
    }
    fclose(disk);

    /* sort by size */
    qsort(files, count, sizeof(files[0]), compare_size);

    /* print sorted */
    for (int i = 0; i < count; i++) {
        printf("%s\t%u bytes\n", files[i].name, files[i].size);
    }
}

void RenameFile(const char *disk_path, const char *srcFileName, const char *newFileName) {
    FILE *disk = fopen(disk_path, "r+b");
    if (!disk) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    int slot = -1;

    // First, ensure no other file is already using newFileName
    for (int i = 0; i < FILE_ENTRIES; i++) {
        char existing[248] = {0};
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(existing, 1, 248, disk);
        if (strcmp(existing, newFileName) == 0) {
            fprintf(stderr, "A file named '%s' already exists\n", newFileName);
            fclose(disk);
            exit(EXIT_FAILURE);
        }
    }

    // Locate the entry for srcFileName
    for (int i = 0; i < FILE_ENTRIES; i++) {
        char namebuf[248] = {0};
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, 248, disk);
        if (strcmp(namebuf, srcFileName) == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fprintf(stderr, "File not found: %s\n", srcFileName);
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    // Write the new name (zero-padded to 248 bytes)
    char newbuf[248] = {0};
    strncpy(newbuf, newFileName, sizeof(newbuf)-1);
    fseek(disk, filelist_offset + slot*256, SEEK_SET);
    fwrite(newbuf, 1, sizeof(newbuf), disk);

    printf("Renamed '%s' -> '%s'\n", srcFileName, newFileName);
    fclose(disk);
}

void Duplicate(const char *disk_path, const char *srcFileName) {
    FILE *disk = fopen(disk_path, "r+b");
    if (!disk) { perror("Error opening disk image"); exit(EXIT_FAILURE); }

    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    uint32_t firstBlock = 0, filesize = 0;
    int slotSrc = -1;

    // 1) Find source entry
    for (int i = 0; i < FILE_ENTRIES; i++) {
        char namebuf[248] = {0};
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, 248, disk);
        if (strcmp(namebuf, srcFileName) == 0) {
            // read firstBlock and filesize
            fread(&firstBlock, sizeof(firstBlock), 1, disk);
            fread(&filesize, sizeof(filesize), 1, disk);
            slotSrc = i;
            break;
        }
    }
    if (slotSrc < 0) {
        fprintf(stderr, "File not found: %s\n", srcFileName);
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    // 2) Build new name = srcFileName + "_copy"
    char newName[248] = {0};
    const char *suffix = "_copy";
    size_t maxBase = sizeof(newName) - 1 - strlen(suffix);
    size_t baseLen = strlen(srcFileName) < maxBase ? strlen(srcFileName) : maxBase;
    memcpy(newName, srcFileName, baseLen);
    memcpy(newName + baseLen, suffix, strlen(suffix));
    newName[baseLen + strlen(suffix)] = '\0';

    // 2a) Ensure no collision
    for (int i = 0; i < FILE_ENTRIES; i++) {
        char exist[248] = {0};
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(exist, 1, 248, disk);
        if (strcmp(exist, newName) == 0) {
            fprintf(stderr, "A file named '%s' already exists\n", newName);
            fclose(disk);
            exit(EXIT_FAILURE);
        }
    }

    // 3) Compute how many blocks
    int blocks = (filesize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // 4) Load FAT
    uint32_t *fat = malloc(FAT_ENTRIES * sizeof(uint32_t));
    if (!fat) { perror("Allocating FAT"); exit(EXIT_FAILURE); }
    fseek(disk, 0, SEEK_SET);
    fread(fat, sizeof(uint32_t), FAT_ENTRIES, disk);

    // 4a) Copy original FAT for reading source chain
    uint32_t *origFat = malloc(FAT_ENTRIES * sizeof(uint32_t));
    memcpy(origFat, fat, FAT_ENTRIES * sizeof(uint32_t));

    // 5) Find free blocks
    int *chain = malloc(blocks * sizeof(int));
    int found = 0;
    for (int i = 1; i < FAT_ENTRIES && found < blocks; i++) {
        if (fat[i] == 0) chain[found++] = i;
    }
    if (found < blocks) {
        fprintf(stderr, "Not enough free space\n");
        free(fat); free(origFat); free(chain);
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    // 6) Update FAT chain
    for (int j = 0; j < blocks - 1; j++)
        fat[chain[j]] = chain[j+1];
    fat[chain[blocks-1]] = 0xFFFFFFFF;

    // 7) Write updated FAT back
    fseek(disk, 0, SEEK_SET);
    fwrite(fat, sizeof(uint32_t), FAT_ENTRIES, disk);

    // 8) Copy data blocks
    off_t data_start = FAT_ENTRIES * sizeof(uint32_t)
                     + FILE_ENTRIES * 256;
    char buffer[BLOCK_SIZE];
    uint32_t cur = firstBlock;
    for (int j = 0; j < blocks; j++) {
        // read source block
        fseek(disk, data_start + cur * BLOCK_SIZE, SEEK_SET);
        fread(buffer, 1, BLOCK_SIZE, disk);
        // write to new block
        fseek(disk, data_start + chain[j] * BLOCK_SIZE, SEEK_SET);
        fwrite(buffer, 1, BLOCK_SIZE, disk);
        cur = origFat[cur];
    }

    // 9) Find free directory slot
    int slotDst = -1;
    for (int i = 0; i < FILE_ENTRIES; i++) {
        uint32_t fb = 0;
        fseek(disk, filelist_offset + i*256 + 248, SEEK_SET);
        fread(&fb, sizeof(fb), 1, disk);
        if (fb == 0) { slotDst = i; break; }
    }
    if (slotDst < 0) {
        fprintf(stderr, "No free file-list entries\n");
        free(fat); free(origFat); free(chain);
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    // 10) Write new file-list entry
    long entryPos = filelist_offset + slotDst * 256;
    fseek(disk, entryPos, SEEK_SET);
    char namebuf[248] = {0};
    strncpy(namebuf, newName, sizeof(namebuf)-1);
    fwrite(namebuf, 1, 248, disk);
    uint32_t fb0 = chain[0];
    fwrite(&fb0, sizeof(fb0), 1, disk);
    fwrite(&filesize, sizeof(filesize), 1, disk);

    printf("Duplicated '%s' -> '%s' (%u bytes)\n",
           srcFileName, newName, filesize);

    // cleanup
    free(fat);
    free(origFat);
    free(chain);
    fclose(disk);
}


void Search(const char *disk_path, const char *srcFileName) {
    FILE *disk = fopen(disk_path, "rb");
    if (!disk) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    int found = 0;

    for (int i = 0; i < FILE_ENTRIES; i++) {
        char namebuf[248] = {0};
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, 248, disk);

        /* skip empty entries */
        if (namebuf[0] == '\0')
            continue;

        if (strcmp(namebuf, srcFileName) == 0) {
            found = 1;
            break;
        }
    }

    fclose(disk);
    printf(found ? "YES\n" : "NO\n");
}


void Hide(const char *disk_path, const char *srcFileName) {
    FILE *disk = fopen(disk_path, "r+b");
    if (!disk) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    char namebuf[248];
    int slot = -1;

    // 1) Find the slot for srcFileName
    for (int i = 0; i < FILE_ENTRIES; i++) {
        memset(namebuf, 0, sizeof(namebuf));
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, sizeof(namebuf), disk);
        if (strcmp(namebuf, srcFileName) == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fprintf(stderr, "File not found: %s\n", srcFileName);
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    // 2) Build hidden name: prefix '.' and truncate if necessary
    char hidden[248] = {0};
    hidden[0] = '.';
    // leave room for the leading dot plus up to 246 chars of original name
    strncpy(hidden + 1, srcFileName, sizeof(hidden) - 2);
    hidden[sizeof(hidden)-1] = '\0';

    // 3) Write it back into the directory entry
    fseek(disk, filelist_offset + slot*256, SEEK_SET);
    fwrite(hidden, 1, sizeof(hidden), disk);

    printf("Hidden '%s'\n", srcFileName);
    fclose(disk);
}


void Unhide(const char *disk_path, const char *srcFileName) {
    FILE *disk = fopen(disk_path, "r+b");
    if (!disk) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    char namebuf[248];
    int slot = -1;

    // 1) Find the entry named ".srcFileName"
    for (int i = 0; i < FILE_ENTRIES; i++) {
        memset(namebuf, 0, sizeof(namebuf));
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf, 1, sizeof(namebuf), disk);
        if (namebuf[0] == '.' && strcmp(namebuf + 1, srcFileName) == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fprintf(stderr, "Hidden file not found: %s\n", srcFileName);
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    // 2) Prepare the un-hidden name buffer (zero-padded)
    char unhidden[248] = {0};
    strncpy(unhidden, srcFileName, sizeof(unhidden) - 1);

    // 3) Write it back into the directory entry
    fseek(disk, filelist_offset + slot*256, SEEK_SET);
    fwrite(unhidden, 1, sizeof(unhidden), disk);

    printf("Unhidden '%s'\n", srcFileName);
    fclose(disk);
}


/**
 * Reads all 128 entries and writes them to "filelist.txt" in the format:
 *   idx name firstBlock fileSize
 * where idx is three digits (000–127), name is the filename or "NULL" if empty,
 * firstBlock and fileSize are decimal.
 */
void PrintFileList(const char *disk_path) {
    FILE *disk = fopen(disk_path, "rb");
    if (!disk) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    // Open the output text file
    FILE *out = fopen("filelist.txt", "w");
    if (!out) {
        perror("Error creating filelist.txt");
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    // Where the 128 directory entries begin
    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);

    for (int i = 0; i < FILE_ENTRIES; i++) {
        // Read name (248 bytes), first block (4 bytes), file size (4 bytes)
        char namebuf[248] = {0};
        uint32_t firstBlock = 0, fileSize = 0;

        // seek to entry i
        fseek(disk, filelist_offset + i * 256, SEEK_SET);
        fread(namebuf, 1, 248, disk);
        fread(&firstBlock, sizeof(firstBlock), 1, disk);
        fread(&fileSize,  sizeof(fileSize),  1, disk);

        // If name is empty, print "NULL"
        const char *displayName = (namebuf[0] == '\0') ? "NULL" : namebuf;

        // Write a line: "000 FileA 1 2000"
        fprintf(out, "%03d %s %u %u\n", i, displayName, firstBlock, fileSize);
    }

    fclose(out);
    fclose(disk);

    printf("File list written to filelist.txt\n");
}


void PrintFAT(const char *disk_path) {
    FILE *disk = fopen(disk_path, "rb");
    if (!disk) {
        perror("Error opening disk image");
        exit(EXIT_FAILURE);
    }

    FILE *out = fopen("fat.txt", "w");
    if (!out) {
        perror("Error creating fat.txt");
        fclose(disk);
        exit(EXIT_FAILURE);
    }

    const int ENTRIES_PER_ROW = 4;
    uint32_t entry;
    for (int i = 0; i < FAT_ENTRIES; i++) {
        // read next FAT entry
        if (fread(&entry, sizeof(entry), 1, disk) != 1) {
            perror("Error reading FAT entry");
            fclose(disk);
            fclose(out);
            exit(EXIT_FAILURE);
        }

        
        fprintf(out, "%04d\t%08X", i, entry);

        // separator or newline every ENTRIES_PER_ROW
        if ((i + 1) % ENTRIES_PER_ROW == 0) {
            fputc('\n', out);
        } else {
            fputc('\t', out);
        }
    }

    fclose(out);
    fclose(disk);
    printf("FAT written to fat.txt\n");
}


void Defragment(const char *disk_path) {
    FILE *disk = fopen(disk_path, "r+b");
    if (!disk) { perror("Error opening disk image"); exit(EXIT_FAILURE); }

    // 1) Load current FAT
    uint32_t *oldFAT = malloc(FAT_ENTRIES * sizeof(uint32_t));
    if (!oldFAT) { perror("Allocating oldFAT"); fclose(disk); exit(EXIT_FAILURE); }
    fseek(disk, 0, SEEK_SET);
    fread(oldFAT, sizeof(uint32_t), FAT_ENTRIES, disk);

    // 2) Scan file-list and read every file's blocks into memory
    long filelist_offset = FAT_ENTRIES * sizeof(uint32_t);
    typedef struct {
        int      slot;    // directory slot
        uint32_t blocks;  // number of blocks
        char    *data;    // all block data concatenated
    } DefragEntry;

    DefragEntry *files = malloc(FILE_ENTRIES * sizeof(DefragEntry));
    if (!files) { perror("Allocating files array"); free(oldFAT); fclose(disk); exit(EXIT_FAILURE); }
    int fileCount = 0;

    for (int i = 0; i < FILE_ENTRIES; i++) {
        // read name, first block, size
        char namebuf[248] = {0};
        uint32_t firstBlock = 0, filesize = 0;
        fseek(disk, filelist_offset + i*256, SEEK_SET);
        fread(namebuf,     1, sizeof(namebuf), disk);
        fread(&firstBlock, sizeof(firstBlock), 1, disk);
        fread(&filesize,   sizeof(filesize),   1, disk);
        if (namebuf[0] == '\0') continue;  // empty entry

        uint32_t blocks = (filesize + BLOCK_SIZE - 1) / BLOCK_SIZE;

        // build chain of old block indices
        uint32_t *chain = malloc(blocks * sizeof(uint32_t));
        if (!chain) { perror("Allocating chain"); exit(EXIT_FAILURE); }
        uint32_t cur = firstBlock;
        for (uint32_t b = 0; b < blocks; b++) {
            chain[b] = cur;
            cur = oldFAT[cur];
        }

        // read all blocks into one buffer
        char *data = malloc(blocks * BLOCK_SIZE);
        if (!data) { perror("Allocating data buffer"); exit(EXIT_FAILURE); }
        off_t data_start = FAT_ENTRIES * sizeof(uint32_t)
                         + FILE_ENTRIES  * 256;
        size_t remaining = filesize;
        for (uint32_t b = 0; b < blocks; b++) {
            fseek(disk, data_start + chain[b] * BLOCK_SIZE, SEEK_SET);
            size_t to_read = remaining < BLOCK_SIZE ? remaining : BLOCK_SIZE;
            fread(data + b*BLOCK_SIZE, 1, to_read, disk);
            if (to_read < BLOCK_SIZE)
                memset(data + b*BLOCK_SIZE + to_read, 0, BLOCK_SIZE - to_read);
            remaining = (remaining > to_read ? remaining - to_read : 0);
        }
        free(chain);

        files[fileCount].slot   = i;
        files[fileCount].blocks = blocks;
        files[fileCount].data   = data;
        fileCount++;
    }
    free(oldFAT);

    // 3) Build a fresh FAT
    uint32_t *newFAT = calloc(FAT_ENTRIES, sizeof(uint32_t));
    if (!newFAT) { perror("Allocating newFAT"); exit(EXIT_FAILURE); }
    newFAT[0] = 0xFFFFFFFF;  // reserved

    // 4) Write each file back contiguously
    off_t data_start = FAT_ENTRIES * sizeof(uint32_t)
                     + FILE_ENTRIES  * 256;
    uint32_t nextFree = 1;

    for (int f = 0; f < fileCount; f++) {
        uint32_t blocks = files[f].blocks;
        char *data      = files[f].data;

        for (uint32_t b = 0; b < blocks; b++) {
            uint32_t newBlk = nextFree + b;
            // write block
            fseek(disk, data_start + newBlk * BLOCK_SIZE, SEEK_SET);
            fwrite(data + b*BLOCK_SIZE, 1, BLOCK_SIZE, disk);
            // update FAT chain
            newFAT[newBlk] = (b < blocks - 1 ? newBlk + 1 : 0xFFFFFFFF);
        }

        // update this file’s firstBlock in directory
        long entryPos = filelist_offset + files[f].slot * 256 + 248;
        uint32_t firstNew = nextFree;
        fseek(disk, entryPos, SEEK_SET);
        fwrite(&firstNew, sizeof(firstNew), 1, disk);

        nextFree += blocks;
    }

    // 5) Write the new FAT over the old one
    fseek(disk, 0, SEEK_SET);
    fwrite(newFAT, sizeof(uint32_t), FAT_ENTRIES, disk);

    // --- after writing newFAT to disk, scrub all freed blocks ---
    static char zeroBlock[BLOCK_SIZE] = {0};
    for (uint32_t b = nextFree; b < FAT_ENTRIES; b++) {
        off_t scrubOff = data_start + (off_t)b * BLOCK_SIZE;
        fseek(disk, scrubOff, SEEK_SET);
        fwrite(zeroBlock, 1, BLOCK_SIZE, disk);
    }

    // 6) Cleanup
    for (int f = 0; f < fileCount; f++) free(files[f].data);
    free(files);
    free(newFAT);
    fclose(disk);

    printf("Disk defragmented successfully.\n");
}


