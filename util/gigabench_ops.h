#ifndef DIRECTORYOPERATIONS_H
#define DIRECTORYOPERATIONS_H   

#define MAX_LEN     512
#define MAX_SIZE    4096

#define DO_GETATTR 12345

void scan_readdir(char *dir_path, int readdir_flag); 
void lookup_file(const char *path_name);
void create_file(const char *path_name);

#endif /* DIRECTORYOPERATIONS_H */
