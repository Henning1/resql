/**
 * @file
 * Hash table implementation.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once

#include <iomanip>
#include <stdio.h>
#include <thread>
#include <atomic>

#include "dbdata.h"
#include "util/Timer.h"


/* performance analysis flag for hash table counters that are printed to stdout */
//#define HT_METRICS


struct Entry;
struct HashTable;


static HashTable* allocateHashTable ( size_t minSize, int payloadSize );
static void growHashTable ( HashTable* ht );
static void freeHashTable ( HashTable* ht );
static Data* ht_put ( HashTable* ht, uint64_t hash );
void showHashTable ( HashTable* ht );


static size_t primeHashTableSizes[] = {
  /* 0     */              5ul,
  /* 1     */              11ul, 
  /* 2     */              23ul, 
  /* 3     */              47ul, 
  /* 4     */              97ul, 
  /* 5     */              199ul, 
  /* 6     */              409ul, 
  /* 7     */              823ul, 
  /* 8     */              1741ul, 
  /* 9     */              3469ul, 
  /* 10    */              6949ul, 
  /* 11    */              14033ul, 
  /* 12    */              28411ul, 
  /* 13    */              57557ul, 
  /* 14    */              116731ul, 
  /* 15    */              236897ul,
  /* 16    */              480881ul, 
  /* 17    */              976369ul,
  /* 18    */              1982627ul, 
  /* 19    */              4026031ul,
  /* 20    */              8175383ul, 
  /* 21    */              16601593ul, 
  /* 22    */              33712729ul,
  /* 23    */              68460391ul, 
  /* 24    */              139022417ul, 
  /* 25    */              282312799ul, 
  /* 26    */              573292817ul, 
  /* 27    */              1164186217ul,
  /* 28    */              2364114217ul, 
  /* 29    */              4294967291ul,
  /* 30    */ (std::size_t)8589934583ull,
  /* 31    */ (std::size_t)17179869143ull,
  /* 32    */ (std::size_t)34359738337ull,
  /* 33    */ (std::size_t)68719476731ull,
  /* 34    */ (std::size_t)137438953447ull,
  /* 35    */ (std::size_t)274877906899ull,
  /* 36    */ (std::size_t)549755813881ull,
  /* 37    */ (std::size_t)1099511627689ull,
  /* 38    */ (std::size_t)2199023255531ull,
  /* 39    */ (std::size_t)4398046511093ull,
  /* 40    */ (std::size_t)8796093022151ull,
  /* 41    */ (std::size_t)17592186044399ull,
  /* 42    */ (std::size_t)35184372088777ull,
  /* 43    */ (std::size_t)70368744177643ull,
  /* 44    */ (std::size_t)140737488355213ull,
  /* 45    */ (std::size_t)281474976710597ull,
  /* 46    */ (std::size_t)562949953421231ull, 
  /* 47    */ (std::size_t)1125899906842597ull,
  /* 48    */ (std::size_t)2251799813685119ull, 
  /* 49    */ (std::size_t)4503599627370449ull,
  /* 50    */ (std::size_t)9007199254740881ull, 
  /* 51    */ (std::size_t)18014398509481951ull,
  /* 52    */ (std::size_t)36028797018963913ull, 
  /* 53    */ (std::size_t)72057594037927931ull,
  /* 54    */ (std::size_t)144115188075855859ull,
  /* 55    */ (std::size_t)288230376151711717ull,
  /* 56    */ (std::size_t)576460752303423433ull,
  /* 57    */ (std::size_t)1152921504606846883ull,
  /* 58    */ (std::size_t)2305843009213693951ull,
  /* 59    */ (std::size_t)4611686018427387847ull,
  /* 60    */ (std::size_t)9223372036854775783ull,
  /* 61    */ (std::size_t)18446744073709551557ull,
};



// Compute hash. Todo: characterize technique
__inline__ uint64_t hashval ( uint64_t  key, 
                              uint64_t  hash ) {
    hash += key;
    hash += ~(hash << 32);
    hash ^=  (hash >> 22);
    hash += ~(hash << 13);
    hash ^=  (hash >>  8);
    hash +=  (hash <<  3);
    hash ^=  (hash >> 15);
    hash += ~(hash << 27);
    hash ^=  (hash >> 31);
    return hash;
}


// Compute string hash. Todo: improve
__inline__ uint64_t hashVarchar ( char*     str, 
                                  uint64_t  hash, 
                                  size_t    maxLen ) {

    for ( size_t i = 0; 
          i < maxLen && *str != '\0'; 
          i++, str++ ) {

        hash = hash + (*str) * 31636373 + (*str);
    }
    return hash;
}


// Compute string hash. Todo: improve
__inline__ uint64_t hashChar ( char*     str, 
                               uint64_t  hash, 
                               size_t    len )
{
    for ( size_t i = 0; i < len; i++ ) {
        char c;
        if ( *str != '\0' ) {
            c = *str;
            str++;
        }
        else {
            c = ' ';
        }
        hash = hash + c * 31636373 + c;
    }
    return hash;
}


/* Hash table data structure             *
 * used by e.g. hash joins.              */
struct HashTable {


    /* number of entries allocated in the table */
    size_t   numEntries;
    int      primeIndex;


    /* Store the shape of table entries in bytes       *
     *                                                 *
     * Full Entry = {      Entry     ,    payload    } *
     * ------------------------------------------------*
     *               [----------fullEntrySize-------]  *
     *                                [-payloadSize-]  *
     *               [-sizeof(Entry)-]                 *
     */
    size_t   fullEntrySize;

    /* size_t   entrySizeWithoutPayload; */
    size_t   payloadSize;


    /* hash table content                     */
    Data*    entries;


    /* pointer to end of content              */
    Data*    entriesEnd;

    
    size_t capacityThreshold;

    size_t numInserts;
 
    #ifdef HT_METRICS 
    size_t probeCollisionDistance;
    size_t buildCollisionDistance;
    #endif
};


/* Struct used for access to hash table entries *
 * We use a packed struct to take care of       *
 * memory alignment manually.                   */
struct  __attribute__ (( __packed__ )) Entry {
   
    // 0: empty, >0: has entry 
    std::atomic<uint8_t>    status;

    /* uint8_t    gap[3]; */

    // hash value of the entry
    uint64_t   hash;

    // The address of data is the start of the payload.
    // We do not add the field to the struct and instead
    // compute its address from either 
    //  - entry address + ht->fullEntrySize, or
    //  - payload address + ht->payloadSize.
    /***  Data*       data;  ***/
};


void initHashTableWorker ( HashTable* table, size_t from, size_t to ) {
    for ( size_t i = from; i < to; i++ ) {
        Entry* entry = (Entry*) &table->entries [ i * table->fullEntrySize ];
        entry->status = 0;
    }
}


// Allocate a new hash table. The number of hash table entries is the next 
// power of two from minSize.
static HashTable* allocateHashTable ( size_t  minSize, 
                                      size_t  payloadSize ) {

    // min. size two to insert one element without resize 
    minSize = std::max ( (size_t) 2, minSize );

    // Allocate hash table datastructure
    //HashTable* ht = (HashTable*) aligned_alloc ( 64, sizeof ( HashTable ) );
    HashTable* ht = (HashTable*) malloc ( sizeof ( HashTable ) );
    if ( ht == nullptr ) {
        error_msg ( OUT_OF_MEMORY, "Hash table allocation failed (datastructure)." );
    }
    HashTable& table = *ht;

    // compute hash table size
    auto res = std::upper_bound ( 
        &primeHashTableSizes[0],
        &primeHashTableSizes[61],
        minSize );

    table.primeIndex    = res - &primeHashTableSizes[0];
    table.numEntries    = primeHashTableSizes [ table.primeIndex ];
    table.payloadSize   = payloadSize;
    table.fullEntrySize = sizeof ( Entry ) + payloadSize;

    // Allocate hash table content and store pointers to its start and end
    size_t entriesBytes = table.numEntries * table.fullEntrySize;
    table.entries     = (Data*) aligned_alloc ( 64, entriesBytes );
    table.entriesEnd  = table.entries + entriesBytes;
    if ( table.entries == nullptr ) {
        error_msg ( OUT_OF_MEMORY, "Hash table allocation failed (entries)." );
    }

    // Initialize entry status with 0 in parallel 
    size_t nthreads = std::thread::hardware_concurrency();
    if ( nthreads == 0 ) nthreads = 4;
    size_t step = (ht->numEntries + nthreads) / nthreads;
    step = std::max ( 10000ul, step );
    std::vector<std::thread> initThreads;
    for ( size_t from = 0; from < ht->numEntries; from += step ) {
        initThreads.push_back ( std::thread ( initHashTableWorker, ht, from, std::min ( from + step, ht->numEntries ) ) );
    }
    for ( auto& thr : initThreads ) thr.join();


    /* 60% max fill before resize */
    table.capacityThreshold = table.numEntries * 6 / 10;
    table.numInserts = 0;

    /* debug */
    #ifdef HT_METRICS
    table.probeCollisionDistance = 0;
    table.buildCollisionDistance = 0;
    std::cout << "Hash table size " 
              << std::fixed
              << std::setprecision(2)
              << ( ( (double)(ht->entriesEnd - ht->entries) ) / 1000000000.0 ) 
              << "GB"
              << " (" << ht->numEntries << ")" << std::endl; 
    #endif

    return ht;
}


/* This can be used for debugging by adjusting the number of  *
 * payload fields 'printFields' to the workload and by adding *
 * calls e.g. to freeHashTable(..) or growHashTable(..).      */
void showHashTableEntries ( HashTable* ht ) {

    /* only int64_t */
    const int printFields = 2;

    Data* p = ht->entries;
    while ( p < ht->entriesEnd ) {
        Entry* e = (Entry*) p;
        std::cout << (int)e->status << ", ";
        std::cout << std::setw(22) << e->hash;
        int64_t* field = (int64_t*) ( p + sizeof ( Entry ) );
        if ( e->status ) {
            for ( int i=0; i<printFields; i++ ) {
                std::cout << ", " << *field; 
                field++;
            }
        }
        std::cout << std::endl;
        p += ht->fullEntrySize;
    }
}


void showHashTable ( HashTable* ht ) {
    std::cout << "HashTable (" << ht << ")" << "{" << std::endl;
    std::cout << " numEntries:         " << ht->numEntries << std::endl;
    std::cout << " sizeof ( Entry ):   " << sizeof ( Entry ) << std::endl;
    std::cout << " payloadSize:        " << ht->payloadSize << std::endl;
    std::cout << " fullEntrySize:      " << ht->fullEntrySize << std::endl;
    std::cout << " entries:            " << (void*) ht->entries << std::endl; 
    std::cout << " entriesEnd:         " << (void*) ht->entriesEnd << std::endl;
    std::cout << " capacityThreshold:  " << ht->capacityThreshold << std::endl;
    std::cout << " numInserts:         " << ht->numInserts << std::endl; 
    std::cout << "}" << std::endl;
}
    

static void growHashTable ( HashTable* ht ) {
    #ifdef HT_METRICS
    std::cout << "Warning: Hash table resize." << std::endl;
    #endif

    HashTable* largerHt;
    largerHt = allocateHashTable ( ht->numEntries + 1, 
                                   ht->payloadSize );

    for ( Data* addr = ht->entries; 
          addr < ht->entriesEnd; 
          addr += ht->fullEntrySize ) {

        Entry* entry = (Entry*) addr;
        if ( entry->status > 0 ) {
            Data* new_ = ht_put ( largerHt, entry->hash );
            Data* old  = addr + sizeof ( Entry );

            /* slower:
             * memcpy ( new_, old, ht->entrySize );
             */

            /* Seems fine with -O3 */
            for ( ; 
                  old < ( addr + ht->fullEntrySize ); 
                  new_++, old++ ) {

                *new_ = *old;
            }
        }
    }
    /* hot swap */
    free ( ht->entries );
    memcpy ( ht, largerHt, sizeof ( HashTable ) );
    free ( largerHt );
} 


// Free all resources allocated for ht 
static void freeHashTable ( HashTable* ht ) {
    #ifdef HT_METRICS
    std::cout << "HT_METRICS{" << std::endl;
    std::cout << "numInserts: " << ht->numInserts << std::endl;
    std::cout << "numSlots:   " << ht->numEntries << std::endl;
    std::cout << "buildCollisionDistance: " << ht->buildCollisionDistance << std::endl;
    std::cout << "probeCollisionDistance: " << ht->probeCollisionDistance << std::endl;
    std::cout << "}" << std::endl;
    #endif
    free ( ht->entries );
    free ( ht );
}


// Insert entry with hash 'hash' into the hash table.
// Returns the address of the data element (payload) of the new entry.
static Data* ht_put ( HashTable* ht, uint64_t hash ) {

    ht->numInserts++;
    if ( ht->numInserts > ht->capacityThreshold ) {
        growHashTable ( ht );
    }

    HashTable& table = *ht;
    
    // get first location 
    size_t loc = hash % table.numEntries; 
    size_t nProbes = 0;

    // linear probing
    while ( nProbes < ht->numEntries ) {
        Entry* entry = (Entry*) &table.entries [ loc * table.fullEntrySize ];
        uint8_t status = entry->status.load();
        while ( status == 0 ) {
            if ( entry->status.compare_exchange_weak ( status, 1 ) ) {
                entry->hash = hash;
                entry->status = 1;
                return ((Data*)entry) + sizeof ( Entry );
            }
        }
        #ifdef HT_METRICS
        ht->buildCollisionDistance++;
        #endif
        loc++;
        if ( loc >= table.numEntries ) loc = 0;
        nProbes++;
    }

    query_error ( HASH_TABLE_FULL );
    return nullptr;
}


// Get entries from the hash table with hash 'hash'.
// Starts a new probe when 'dataLoc' is null.
// Performs consecutive probe when 'dataLoc' is the previous return value. 
// Returns pointer to the data element (payload) of the next matching entry
// or returns null if no entries match the hash.
static Data* ht_get ( HashTable*  ht, 
                      uint64_t    hash, 
                      Data*       dataLoc ) {
    
    HashTable& table = *ht;
    Data* entryLoc; 

    // Start a new probe and get first position..
    if ( dataLoc == nullptr ) {
        size_t loc = hash % table.numEntries; 
        entryLoc = (Data*) &table.entries [ loc * table.fullEntrySize ];
    }

    // ..or continue a probe and get next position
    else {
        // subsequent (linear) probe
        // get first address after payload
        entryLoc = dataLoc + table.payloadSize;
    }

    // Perform Linear probing and check hash values 
    //  > entry to access elements of ht entries
    //  > entryLoc for entry iteration
    Entry* entry = (Entry*) entryLoc;
    while ( entry->status != 0 ) {

        //if ( ( ( entryLoc - table.entries) / table.fullEntrySize ) > table.numEntries ) {
        //    error_msg ( ELEMENT_NOT_FOUND, "Out of bounds access in hash table." );
        //}
        //if ( ( ( entryLoc - table.entries) % table.fullEntrySize ) > 0 ) {
        //    error_msg ( ELEMENT_NOT_FOUND, "HT entry access not aligned with entry boundaries." );
        //}

        // Check for hash match and if positive
        // return pointer to payload
        if ( entry->hash == hash ) {
            return entryLoc + sizeof ( Entry );
        }
        
        #ifdef HT_METRICS
        ht->probeCollisionDistance++;
        #endif

        // go to next entry
        entryLoc += table.fullEntrySize;
        if ( entryLoc >= table.entriesEnd ) {
            entryLoc = table.entries;
        }
        entry = (Entry*) entryLoc;
    } 
    return nullptr;
}

