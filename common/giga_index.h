/* 
 * GIGA+ indexing implementation 
 *  by- Swapnil V Patil (svp at cs)
 *
 * TERMINOLOGY:
 *  - bitmap: indicates the presence/absence of a partition
 *  - hash: hash of the filename
 *  - index: position in the bitmap
 *
 */
#ifndef GIGA_INDEX_H
#define GIGA_INDEX_H   

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "sha.h"

typedef unsigned char bitmap_t;         // Bitmap stored as array of "bitmap_t"
typedef int index_t;                    // Index is the position in the bitmap 

#define HASH_NUM_BYTES 16                   //128-bit MD5 hash
#define HASH_LEN    2*SHA1_HASH_SIZE        //bigger array for binary2hex

#define MAX_RADIX 10 
#define MIN_RADIX 0

// Support different modes of splitting in GIGA+
//
#define SPLIT_T_NO_BOUND            1111
#define SPLIT_T_NO_SPLITTING_EVER   2222
#define SPLIT_T_NUM_SERVERS_BOUND   3333
#define SPLIT_T_NEXT_HIGHEST_POW2   4444

#define SPLIT_TYPE                  SPLIT_T_NUM_SERVERS_BOUND

#define MAX_BKTS_PER_SERVER         2 

// To avoid the signed and unsigned bit business, we just use the 7-bits in
// every byte to represent the bitmap
//
#define BITS_PER_MAP ((int)(sizeof(bitmap_t)*8)-1)

#define MAX_BMAP_LEN ( (((1<<MAX_RADIX)%(BITS_PER_MAP)) == 0) ? ((1<<MAX_RADIX)/(BITS_PER_MAP)) : ((1<<MAX_RADIX)/(BITS_PER_MAP))+1 ) 

// Header table stored cached by each client/server. It consists of:
// -- The bitmap indicating if a bucket is created or not.
// -- Current radix of the header table.
//
struct giga_mapping_t {
    bitmap_t bitmap[MAX_BMAP_LEN];      // bitmap
    unsigned int curr_radix;            // current radix (depth in tree)
    unsigned int zeroth_server;
    unsigned int server_count;
}; 

// Hash the component name (hash_key) to return the hash value.
//
void giga_hash_name(const char *hash_key, char hash_value[]);

// Initialize the mapping table.
//
void giga_init_mapping(struct giga_mapping_t *mapping, int flag, 
                       unsigned int zeroth_server, unsigned int server_count);
void giga_init_mapping_from_bitmap(struct giga_mapping_t *mapping,
                                   bitmap_t bitmap[], int bitmap_len, 
                                   unsigned int zeroth_server, 
                                   unsigned int server_count); 

// Copy one mapping structure into another; the integer flag tells if the 
// the destination should be filled with zeros (if z == 0);
//
void giga_copy_mapping(struct giga_mapping_t *dest, struct giga_mapping_t *src, int z);

// Update the client cache
// -- OR the header table update received from the server.
//
void giga_update_cache(struct giga_mapping_t *old_copy, 
                       struct giga_mapping_t *new_copy);

// Update the bitmap by setting the bit at 'index' to 1
//
void giga_update_mapping(struct giga_mapping_t *mapping, index_t index);

void giga_update_mapping_remove(struct giga_mapping_t *mapping, index_t index);

// Print the struct giga_mapping_t contents in a file 
//
void giga_print_mapping(struct giga_mapping_t *mapping, FILE* output_fp);

// Check whether a file needs to move to the new bucket created from a split.
//
int giga_file_migration_status(const char *filename, index_t new_index);   

// Given the index of the overflow partition, return the index 
// of the partition created after splitting that partition.
// It takes two arguments: the mapping table and the index of overflow bkt
//
index_t giga_index_for_splitting(struct giga_mapping_t *mapping, index_t index);  

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
index_t giga_get_index_for_file(struct giga_mapping_t *mapping, 
                                const char *file_name); 

index_t giga_get_server_for_file(struct giga_mapping_t *mapping,
                                 const char *file_name);
index_t giga_get_server_for_index(struct giga_mapping_t *mapping,
                                  index_t index);
index_t giga_get_bucket_num_for_server(struct giga_mapping_t *mapping,
                                  index_t index);

// FIXME: what's this for?
index_t giga_get_index_for_backup(index_t index); 

index_t get_split_index_for_newserver(index_t index);

int giga_is_splittable(struct giga_mapping_t *mapping, index_t old_index);

#endif /* GIGA_INDEX_H */

