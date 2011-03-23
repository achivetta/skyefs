#ifndef GIGA_INDEX_H
#define GIGA_INDEX_H   

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "sha.h"

#define HASH_NUM_BYTES 16                   //128-bit MD5 hash

#define HASH_LEN    2*SHA1_HASH_SIZE        //bigger array for binary2hex

#define MAX_RADIX 10 
#define MIN_RADIX 0

//================================================
// Support different modes of splitting in GIGA+
// ===============================================
//
#define SPLIT_T_NO_BOUND            1111
#define SPLIT_T_NO_SPLITTING_EVER   2222
#define SPLIT_T_NUM_SERVERS_BOUND   3333
#define SPLIT_T_NEXT_HIGHEST_POW2   4444

#define SPLIT_TYPE                  SPLIT_T_NO_BOUND

//typedef uint64_t bitmap_t;
//typedef uint8_t bitmap_t;
typedef unsigned char bitmap_t;

// we want to avoid the signed and unsigned bit business; just discard it.
#define BITS_PER_MAP ((int)(sizeof(bitmap_t)*8)-1)

#define MAX_BMAP_LEN ( (((1<<MAX_RADIX)%(BITS_PER_MAP)) == 0) ? ((1<<MAX_RADIX)/(BITS_PER_MAP)) : ((1<<MAX_RADIX)/(BITS_PER_MAP))+1 ) 

// Index of the header table entries:
// i.e., the total number of entries in the header table (2^GLOBAL_DEPTH)
//
typedef int index_t;        // index (bit position in the bitmap) 
typedef unsigned long long hash_value_t;
//typedef const char* hash_key_t;

// Header table stored cached by each client/server. It consists of:
// -- The bitmap indicating if a bucket is created or not.
// -- Current radix of the header table.
//
struct GigaMapping {
    bitmap_t bitmap[MAX_BMAP_LEN];      // bitmap
    unsigned int curr_radix;            // current radix (depth in tree)
    //uint32_t curr_radix;
}; 

// GIGA state associated with each directory. Clients and servers use this
// structure, with its mutex locks, for all operations.
//
struct DirectoryState {
    pthread_mutex_t     mutex;
    struct GigaMapping  mapping;
};

struct BucketState {
    pthread_mutex_t     mutex;
    //pthread_rwlock_t     mutex;
    int bkt_size;
};

// Hash the component name (hash_key) to return the hash value.
//
void giga_hash_name(const char *hash_key, char hash_value[]);

// For a given directory path name, return the zeroth server.
//
int giga_create_dir(const char *path_name, int num_servers);

// Initialize the mapping table.
//
void giga_init_mapping(struct GigaMapping *mapping, int flag);
void giga_init_mapping_from_bitmap(struct GigaMapping *mapping,
                                   bitmap_t bitmap[], int bitmap_len); 

// Copy one mapping structure into another; the integer flag tells if the 
// the destination should be filled with zeros (if z == 0);
//
void giga_copy_mapping(struct GigaMapping *dest, struct GigaMapping *src, int z);

// Update the client cache
// -- OR the header table update received from the server.
//
void giga_update_cache(struct GigaMapping *old_copy, 
                       struct GigaMapping *new_copy);

// Update the bitmap by setting the bit at 'index' to 1
//
void giga_update_mapping(struct GigaMapping *mapping, index_t index);

// Print the struct GigaMapping contents in a file 
//
void giga_print_mapping(struct GigaMapping *mapping, FILE* output_fp);

// Check whether a file needs to move to the new bucket created from a split.
//
int giga_file_migration_status(const char *filename, index_t new_index);   

// Given the index of the overflow partition, return the index 
// of the partition created after splitting that partition.
// It takes two arguments: the mapping table and the index of overflow bkt
//
index_t giga_index_for_splitting(struct GigaMapping *mapping, index_t index);  

// Given a partition "index", return the "parent" index that needs to be
// split to create the partition "index".
// - Used when adding new servers that need to force splits for migration 
// - Only need the parent, even it doesn't exist (so no need to pass the bitmap
//   because this is different from finding the real parent)
//
index_t giga_index_for_force_splitting(index_t index);  

// Using the mapping table, find the bucket (index) where a give
// file should be inserted or searched.
//
index_t giga_get_index_for_file(struct GigaMapping *mapping, 
                                const char *file_name); 

index_t giga_get_index_for_backup(index_t index); 
index_t get_split_index_for_newserver(index_t index);

#endif /* GIGA_INDEX_H */

