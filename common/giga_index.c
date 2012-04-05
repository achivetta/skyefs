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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>

#include "trace.h"

#include "giga_index.h"
#include "sha.h"

#define ARRAY_LEN(array)(sizeof(array)/sizeof((array)[0])) 

//static int hash_compare(char hash_val_1[], char hash_val_2[], int len);

static index_t get_child_index(index_t index, int radix);
static index_t get_parent_index(index_t index);

static index_t compute_index(char hash_value[], int radix); 
static int get_highest_index(bitmap_t bitmap[]);

static int get_bit_status(bitmap_t bmap[], index_t index);

static int get_radix_from_bmap(bitmap_t bitmap[]);
static int get_radix_from_index(index_t index);

static void print_bitmap(bitmap_t bmap[], FILE *fp);

//static void struct giga_mapping_t_update_radix(struct giga_mapping_t *table);

// Compute the SHA-1 hash of the file name (or path name) 
//
void giga_hash_name(const char* hash_key, char hash_value[])
{
    uint8_t hash[SHA1_HASH_SIZE] = {0}; 
    assert(hash_key);
    assert(hash_value);
    int len = (int)strlen(hash_key);

#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] hash: key={%s} of len=%d",__func__, hash_key, len);
#endif

    shahash((uint8_t*) hash_key, len, hash);
    binary2hex(hash, SHA1_HASH_SIZE, hash_value);

#ifdef DBG_INDEXING
    int i;
    //fprintf(log_fp, "[%s] hash={%s}\n", __func__, hash_value);
    fprintf(log_fp, "[%s] hash={", __func__);
    for (i=0; i<HASH_LEN; i++)
        fprintf(log_fp, "%c", hash_value[i]);
    fprintf(log_fp, "} of len=%d\n", HASH_LEN);
#endif
}

// Initialize the mapping table: 
// - set the bitmap to all zeros, except for the first location to one which
//   indicates the presence of a zeroth bucket
// - set the radix to 1 (XXX: do we need radix??)
// - flag indicates the number of servers if you use static partitioning
//
void giga_init_mapping(struct giga_mapping_t *mapping, int flag, 
                       unsigned int zeroth_server, unsigned int server_count)
{
    int i;
#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] initialize giga mapping (flag=%d)", __func__, flag);
#endif

    assert(mapping != NULL);

    memset(mapping->bitmap, 0, MAX_BMAP_LEN);

    mapping->zeroth_server = zeroth_server;
    if (server_count > 0)
        mapping->server_count = server_count;
    else
        mapping->server_count = 1;
    
    if (flag == -1) {
        mapping->bitmap[0] = 1;
        mapping->curr_radix = 1;
        return;
    }
    
    switch(SPLIT_TYPE) {
        //case SPLIT_TYPE_KEEP_SPLITTING:
        case SPLIT_T_NO_BOUND:
            mapping->bitmap[0] = 1;
            mapping->curr_radix = 1;    
            break;
        //case SPLIT_TYPE_NEVER_SPLIT:
        case SPLIT_T_NO_SPLITTING_EVER:
            assert(flag != -1);
            if (flag < BITS_PER_MAP){
                mapping->bitmap[i] = (1<<flag)-1;
            } else {
                fprintf(log_fp, "[%s] XXX: need to fixx this dude!!!\n",__func__);
                exit(1);
            }
            mapping->curr_radix = get_radix_from_bmap(mapping->bitmap);
            break;
        //case SPLIT_TYPE_ALL_SERVERS:
        case SPLIT_T_NUM_SERVERS_BOUND:
            mapping->bitmap[0] = 1;
            mapping->curr_radix = 1;    
            break;
        //case SPLIT_TYPE_POWER_OF_2:
        case SPLIT_T_NEXT_HIGHEST_POW2:
            mapping->bitmap[0] = 1;
            mapping->curr_radix = 1;    
            break;
        default:
            fprintf(log_fp, "[%s] ERROR: Illegal Split Type. %d\n",
                    __func__, SPLIT_TYPE);
            fprintf(stdout, "[%s] ERROR: Illegal Split Type. %d\n",
                    __func__, SPLIT_TYPE);
            exit(1);
            break;
    }
    
    //mapping->curr_radix = get_radix_from_bmap(mapping->bitmap);
    assert(mapping != NULL);
}


// Initialize the mapping table to an existing bitmap
//
void giga_init_mapping_from_bitmap(struct giga_mapping_t *mapping,
                                   bitmap_t bitmap[], int bitmap_len,
                                   unsigned int zeroth_server, 
                                   unsigned int server_count)
{
    int i;
#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] initialize giga mapping from bitmap", __func__);
#endif

    assert(mapping != NULL);
    giga_init_mapping(mapping, -1, zeroth_server, server_count);

    int bigger_bmap = ((MAX_BMAP_LEN > bitmap_len) ? bitmap_len : MAX_BMAP_LEN);
    for (i=0; i<bigger_bmap; i++) {
        if (i==0)
            assert(bitmap[i] != 0);
        mapping->bitmap[i] = bitmap[i];
    }
    mapping->curr_radix = get_radix_from_bmap(mapping->bitmap);
}

// Copy a source mapping to a destination mapping structure. 
//
void giga_copy_mapping(struct giga_mapping_t *dest, struct giga_mapping_t *src, int z)
{
    int i;

    assert(dest != NULL);

#ifdef  DBG_INDEXING
    dbg_msg(log_fp, "[%s] copy one map into another", __func__);
#endif
    
    if (z == 0) {
        giga_init_mapping(dest, -1, src->zeroth_server, src->server_count);
    } 
    else {
        assert(src != NULL);

        //XXX: do we need to check the length of both bitmaps
        for(i = 0;i < MAX_BMAP_LEN; i++)
            dest->bitmap[i] = src->bitmap[i];
        
        dest->curr_radix = get_radix_from_bmap(dest->bitmap);
        //XXX: why not this?
        //curr->curr_radix = update->curr_radix;
    }

#ifdef  DBG_INDEXING
    dbg_msg(log_fp, "[%s] copy successful", __func__);
    giga_print_mapping(dest, log_fp);
#endif

    return;
}

// Update the cache, with the header table update from server.
//
void giga_update_cache(struct giga_mapping_t *curr, struct giga_mapping_t *update)
{
    int i;

#ifdef  DBG_INDEXING
    dbg_msg(log_fp, "[%s] beginning to update the cached copy.", __func__);
#endif

    assert(curr != NULL);
    assert(update != NULL);

    //XXX: what do we need to check for?
    //  - the length of both bitmaps: same or different?
    //  - anything to do with radix?
    //
    for(i = 0;i < MAX_BMAP_LEN; i++)
        curr->bitmap[i] = curr->bitmap[i] | update->bitmap[i];
    
    curr->curr_radix = get_radix_from_bmap(curr->bitmap);

    if (update->server_count > curr->server_count)
        curr->server_count = update->server_count;
    
#ifdef  DBG_INDEXING
    dbg_msg(log_fp, "[%s] updating the cached copy. success.", __func__);
#endif

    return;
}

// Update the bitmap by setting the bit value of the partition index 
// that is created after splitting the ``index'' 
// This is separate function because it should be done only when splitting
// has been successful.
//
void giga_update_mapping(struct giga_mapping_t *mapping, index_t new_index)
{

#ifdef  DBG_INDEXING
    dbg_msg(log_fp, "[%s] post-split update @index=%d", __func__, new_index);
#endif
    int index_in_bmap = new_index / BITS_PER_MAP;
    int bit_in_index = new_index % BITS_PER_MAP;

    bitmap_t mask = (bitmap_t)(1<<(bit_in_index));
    bitmap_t bit_info = mapping->bitmap[index_in_bmap];
   
    bit_info = bit_info | mask;
    
    mapping->bitmap[index_in_bmap] = bit_info;
    mapping->curr_radix = get_radix_from_bmap(mapping->bitmap);

#ifdef  DBG_INDEXING
    dbg_msg(log_fp,
            "[%s] post-split update @index=%d. DONE.", __func__, new_index);
    print_bitmap(mapping->bitmap,log_fp);
#endif
    return;
}

void giga_update_mapping_remove(struct giga_mapping_t *mapping, index_t new_index)
{
    int index_in_bmap = new_index / BITS_PER_MAP;
    int bit_in_index = new_index % BITS_PER_MAP;

    bitmap_t mask = (bitmap_t)(1<<(bit_in_index));
    bitmap_t bit_info = mapping->bitmap[index_in_bmap];
   
    bit_info = bit_info & (~mask);
    
    mapping->bitmap[index_in_bmap] = bit_info;
    mapping->curr_radix = get_radix_from_bmap(mapping->bitmap);

    return;
}


// Returns the new index after splitting partition at ``index''
//
index_t giga_index_for_splitting(struct giga_mapping_t *mapping, index_t index)
{
    index_t new_index = index;

#ifdef  DBG_INDEXING
    dbg_msg(log_fp, "[%s] split index=%d for bitmap below", __func__, index);
    giga_print_mapping(mapping, log_fp);
#endif

    assert(get_bit_status(mapping->bitmap, index) == 1); 
    /*
    int radix = get_radix_from_index(index);
    do {
//#ifdef  DBG_INDEXING
//    dbg_msg(log_fp, "[%s] finding child for index=%d, using radix=%d\n", 
//            __func__, index, radix);
//#endif
        //new_index = get_child_index(index, radix);
        //if (new_index >= 1)
        //    radix += 1;
        new_index = get_child_index(new_index, radix);
    } while (get_bit_status(mapping->bitmap, new_index) == 1);
    */

    int i = get_radix_from_index(index);
    while (1) {
        assert (i < MAX_RADIX);
        new_index = get_child_index(index, i);
        if (get_bit_status(mapping->bitmap, new_index) == 0)
            break;
        i++;
    }

    assert(new_index != index);
    
#ifdef  DBG_INDEXING
    dbg_msg(log_fp,
            "[%s] index=%d --[split]-- index=%d", __func__, index, new_index);
#endif
    return new_index;
}

// Returns the index of the parent that needs to split to create the "index"
// passed in the argument.
// (Not need for bitmap traversal -- just simple one level up the tree)
//
index_t giga_index_for_force_splitting(index_t index)
{
    return get_parent_index(index);
}

index_t giga_get_index_for_backup(index_t index) 
{
    return get_child_index(index, get_radix_from_index(index));
}

// Given the hash of a file, return the index of the partition 
// where the file should be inserted or should be searched.
//
index_t giga_get_index_for_file(struct giga_mapping_t *mapping, 
                                const char *filename)
{
#ifdef  DBG_INDEXING
    dbg_msg(log_fp,"[%s] getting index for file(%s)", __func__, filename);
#endif
    
    char hash[HASH_LEN] = {0};
    giga_hash_name(filename, hash);
    
    // find the current radix 
    int curr_radix = get_radix_from_bmap(mapping->bitmap);
    //int curr_radix = mapping->curr_radix;
    
    // compute index using the "radix" bits of the filename hash
    index_t index = compute_index(hash, curr_radix); 

//#ifdef  DBG_INDEXING
//    dbg_msg(log_fp,"[%s] getting bit-status @ index %d", __func__, index);
//#endif
   
    // check if the index exists (remember, that 2^radix many not 
    // have all bits set), if not find its parent ...
    // ... repeat until you reach the parent whose bit is set to 1
    // example: 
    //   bitmap={11101000}, so radix=3, 
    //   if index is 6, it doesn't exist yet, trace to parent (2)
    // XXX: check for error: from p3->p2 but p2 is set to 0!!
    // although this should never happen, we should still check it.
    while (get_bit_status(mapping->bitmap, index) == 0) {
        index_t curr_index = index;
        index = get_parent_index(curr_index);
    }

    assert(get_bit_status(mapping->bitmap, index) == 1);

#ifdef  DBG_INDEXING
    dbg_msg(log_fp,
            "[%s] file=%s --> partition_index=%d", __func__, filename, index);
#endif
   
    return index;
}

// Given the hash of a file, return the server where the file should be inserted
// or should be searched.
//
index_t giga_get_server_for_file(struct giga_mapping_t *mapping, 
                                const char *filename) {
    index_t index = giga_get_index_for_file(mapping,filename);
    return giga_get_server_for_index(mapping, index);
}

index_t giga_get_server_for_index(struct giga_mapping_t *mapping, 
                                  index_t index) {
    return (index + mapping->zeroth_server) % mapping->server_count;
}

index_t giga_get_bucket_num_for_server(struct giga_mapping_t *mapping, index_t index){
    return index % mapping->server_count;
}

// Check whether an existing file needs to be migrated to the newly split bkt.
// Pass the "index" of the current bucket. 
// Return ZERO, if the file stays in the bucket.
// 
int giga_file_migration_status(const char* filename, index_t new_index) 
{
    int ret = 0;
#ifdef  DBG_INDEXING
    dbg_msg(log_fp,"[%s] checking if file(%s) moves?", __func__, filename);
#endif
    
    char hash[HASH_LEN] = {0};
    giga_hash_name(filename, hash);

    int radix = get_radix_from_index(new_index);
    if (compute_index(hash, radix) == new_index)
        ret = 1;

#ifdef  DBG_INDEXING
    dbg_msg(log_fp,"[%s] file(%s) move status: %d", __func__, filename, ret);
#endif
    
    return ret;
}

int giga_is_splittable(struct giga_mapping_t *mapping, index_t old_index)
{
    switch (SPLIT_TYPE) {
        index_t new_index;
        case SPLIT_T_NO_BOUND:
            return 1;
        case SPLIT_T_NO_SPLITTING_EVER:
            return 0;
        case SPLIT_T_NUM_SERVERS_BOUND:
            new_index = giga_index_for_splitting(mapping, old_index);
            if (new_index < MAX_BKTS_PER_SERVER * (signed int)mapping->server_count)
                return 1;
            else
                return 0;
        case SPLIT_T_NEXT_HIGHEST_POW2:
            abort();
    }

    /* should never get here */
    return 1;
}

// Print the struct giga_mapping_t contents. 
//
void giga_print_mapping(struct giga_mapping_t *mapping, FILE* fp)
{
    assert(mapping != NULL);
    
    dbg_msg(fp, "[%s]",__func__); 
    dbg_msg(fp,"=========="); 
    dbg_msg(fp,"printing the header table ... ");
    dbg_msg(fp,"\tradix=%d", mapping->curr_radix);
    dbg_msg(fp,"\tzeroth server=%d", mapping->zeroth_server);
    dbg_msg(fp,"\tserver count=%d", mapping->server_count);
    dbg_msg(fp,"\tbitmap_size=%d", MAX_BMAP_LEN);
    dbg_msg(fp,"\tbitmap (from 0th position)=");
    print_bitmap(mapping->bitmap, fp);
    dbg_msg(fp,"\n=========="); 
}

static void print_bitmap(bitmap_t bmap[], FILE *fp)
{
    int i;
    for(i = 0; i < MAX_BMAP_LEN; i++)
        fprintf(fp,"%d|",bmap[i]);
    fprintf(fp,"\n");
}

// From a given bitmap, find the radix for that bitmap by looking
// at the highest index in the bitmap with a "1".
//
static int get_radix_from_bmap(bitmap_t bitmap[])
{
#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] for given bitmap, find radix ... ", __func__);
    print_bitmap(bitmap, log_fp);
#endif

    int radix = get_radix_from_index(get_highest_index(bitmap));

#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] for above bitmap, radix=%d", __func__, radix);
#endif

    return radix;
}

// Simply put, given a string of 1s and 0s, find the highest location of 1.
// In this function, the "string" is the GIGA+ bitmap, and you have to find the
// highest "location" in this bitmap where the bit value is 1
//
static int get_highest_index(bitmap_t bitmap[])
{
    int i,j;
    int max_index = -1;
    int index_in_bmap = -1;  // highest index with a non-zero element
    int bit_in_index = -1;  // highest bit in the element in index_in_bmap
    
    // find the largest non-zero element, and then in that element find the
    // highest bit that is 1
    //
    for (i=MAX_BMAP_LEN-1; i>=0; i--) {
        //printf("@element%d\n",i);
        if (bitmap[i] != 0) {
            index_in_bmap = i;  
            //printf("@element%d:value=%d;index_in_bmap=%d\n",i, bitmap[i], index_in_bmap);
            break;
        }
    }
    
    bitmap_t value = bitmap[i];
    for (j=BITS_PER_MAP-1; j>=0; j--) {
        bitmap_t mask = (bitmap_t)1<<j;
        //printf("\t looking at bit-%d of 8, with mask=%d\n",(j+1), mask);
        if ((value & mask) != 0) {
            bit_in_index = j;
            break;
        }
    }
    
    max_index = (index_in_bmap * BITS_PER_MAP) + bit_in_index;
    assert(max_index >= 0);

#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] for bitmap below, highest=%d", __func__, max_index);
    print_bitmap(bitmap, log_fp);
#endif

    return max_index;
}

// Get radix from the index, i.e. for index i, you need a r-bit binary number.
// (used after splits, to decided whether to increment the radix).
//
static int get_radix_from_index(index_t index)
{
    //int radix = ((index > 0) ? ((int)(floor(log2(index)))+1) : 0);
    
    int radix = -1;
    if (index == 0)
        radix = 0;
    else if (index == 1)
        radix = 1;
    else
        radix = ((int)(floor(log2((double)index)))+1);

#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] for index=%d, radix=%d ", __func__, index, radix);
#endif

    return radix;
}

// Return the status of a bit at a given index in the bitmap.
//
static int get_bit_status(bitmap_t bmap[], index_t index)
{   
    int status = 0;

    int index_in_bmap = index / BITS_PER_MAP;
    int bit_in_index = index % BITS_PER_MAP;

    bitmap_t mask = (bitmap_t)(1<<(bit_in_index));
    bitmap_t bit_info = bmap[index_in_bmap];
    
    bit_info = bit_info & mask;
    if (bit_info != 0)
        status = 1;

#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] in bitmap below @ index=%d, bit-status=%d ",
            __func__, index, status);
    print_bitmap(bmap, log_fp);
#endif

    return status;
}

// Return the child index for any given index, i.e. the index of 
// partition created after a split.
// Unlike get_parent_index() this requires a radix because you might want to
// check the children repeatedly by increasing the radix, and not relying on
// the radix from the bitmap. hmmm??? really???
//
static index_t get_child_index(index_t index, int radix)
{
    assert (index>=0);
    assert (radix>=0);

    index_t child_index = index + (int)(1<<radix); 

    //if (index == 0)
    //    child_index = 1;
    //else
    //    child_index = index + (int)(1<<radix); 

#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] child of %d -> %d", __func__, index, child_index);
#endif

    return child_index;
}

// Return the parent index of any given index 
// (traverse the bit position one-level up the tree)
//
static index_t get_parent_index(index_t index)
{
    index_t parent_index = 0;
    if (index > 0)
        parent_index = index - (int)(1 << ((int)(floor(log2((double)index)))));

#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] parent of %d -> %d", __func__, index, parent_index);
#endif

    return parent_index;
}

// For the first partition to the stored on the new server, find the 
// parent 'index' of that partition that should split
//
index_t get_split_index_for_newserver(index_t index)
{
    return get_parent_index(index);
}

// Use the last "radix" number of bits from the hash value to find the 
// index of the bucket where the file needs to be inserted.
//
static index_t compute_index(char hash_value[], int radix) 
{
    index_t index = 0;
    int i;

    assert(radix < MAX_RADIX);

    // use the last "radix" bits from the hash
    int curr_byte;
    index_t curr_mask, curr_value, curr_shift;
    int num_useful_bytes, residual_useful_bits;

    uint8_t bin_hash[SHA1_HASH_SIZE] = {0}; 
    hex2binary(hash_value, HASH_LEN, bin_hash);
    //for (i=0; i<SHA1_HASH_SIZE; i++) printf("%d,", bin_hash[i]);

    num_useful_bytes = radix/(sizeof(bin_hash[0])*8);
    residual_useful_bits = radix%(sizeof(bin_hash[0])*8);
    //if (residual_useful_bits != 0)
    if ((residual_useful_bits != 0) || (num_useful_bytes>0))
        num_useful_bytes += 1; 

    //printf("num_useful_bytes(from hash)=%d, residual_bits=%d\n", 
    //        num_useful_bytes, residual_useful_bits);
    for (i=0; i<num_useful_bytes-1; i++) {
        curr_byte = bin_hash[SHA1_HASH_SIZE-1-i];
        curr_shift = curr_byte<<(i*sizeof(bin_hash[0])*8);
        index += curr_shift;
    }

    curr_byte = bin_hash[SHA1_HASH_SIZE-1-i];
    curr_mask = (1<<residual_useful_bits) - 1;
    curr_value = curr_byte & curr_mask;
    curr_shift = curr_value<<(i*sizeof(bin_hash[0])*8);
 
    //printf("byte=%d, after shift=%d\n", curr_byte, curr_shift); 
    index += curr_shift;
        
#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] using radix=%d on hash={%s}, index=%d",
            __func__, radix, hash_value, index);
#endif

    return index;
}

// Compare if two hashes are equal, and return the following.
//  "1" if hash_val_1 is greater
//  "-1" if hash_val_2 is greater
//  "0" is both are equal
/*
static int hash_compare(char hash_val_1[], char hash_val_2[], int len)
{
    int status = 0;
    int i;
    for (i = 0; i < len; i++) {
        if (hash_val_1[i] > hash_val_2[i])
            status=1;
        else if(hash_val_1[i] < hash_val_2[i])
            status=-1;
    }

#ifdef DBG_INDEXING
    dbg_msg(log_fp, "[%s] comparison of hash1 and hash2 == ",__func__, status);
#endif

    return status;
}
*/
//=================== DEPRECATED =====================

/*
// Initialize the mapping table
// XXX: idiot, can't return memory declared on stack??
//
struct giga_mapping_t* giga_init_table()
{
    struct giga_mapping_t* table = (struct giga_mapping_t*)malloc(sizeof(struct giga_mapping_t));
    if (table == NULL)
    {
        fprintf(stdout,"[%s:%d] error during malloc \n",
                __func__,__LINE__);
        return NULL;
    }

    //set the bitmap to zero
    memset(table->bitmap, 0, MAX_BMAP_LEN);
    table->bitmap[0] = 1;       // initialize the zero-th entry in the bitmap 
    
    //XXX: do we really need this??
    //set the curr radix
    table->curr_radix = 1;  // always start with one header table entry

    //int radix_of_new_bmap = hdGetRadixFromBmap(table->bitmap);
    return table;
}


static void struct giga_mapping_t_update_radix(struct giga_mapping_t *table)
{
    printf("[%s] - curr_radix=%d,",
           __func__, table->curr_radix);
    int radix_from_bmap = get_radix_from_bmap(table->bitmap);
    if (radix_from_bmap > table->curr_radix)
        table->curr_radix = radix_from_bmap;
    printf("updated=%d\n", radix_from_bmap);
}
*/

