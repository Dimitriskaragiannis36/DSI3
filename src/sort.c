#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bf.h"
#include "hp_file.h"
#include "record.h"
#include "sort.h"
#include "merge.h"
#include "chunk.h"

// Συγκρίνει πρώτα το όνομα και μετά το επίθετο για να περνάει το test της sort_main.c
bool shouldSwap(Record* rec1, Record* rec2) {
    int nameComparison = strcmp(rec1->name, rec2->name);
    if (nameComparison < 0) {
        return false; 
    } else if (nameComparison > 0) {
        return true;  
    } else {
        int surnameComparison = strcmp(rec1->surname, rec2->surname);
        return surnameComparison > 0;
    }
}

// Συνάρτηση σύγκρισης (Comparator) για την qsort
int compareRecords(const void* a, const void* b) {
    Record* rec1 = (Record*)a;
    Record* rec2 = (Record*)b;
    
    int nameComparison = strcmp(rec1->name, rec2->name);
    if (nameComparison != 0) {
        return nameComparison;
    }
    return strcmp(rec1->surname, rec2->surname);
}

// Ορίζουμε μία συνάρτηση για να ταξινομήσουμε τα records μέσα σε ένα chunk.
void sort_Chunk(CHUNK* chunk) {
    int maxPossibleRecords = chunk->recordsInChunk; // Το μέγιστο θεωρητικό όριο
    if (maxPossibleRecords <= 0) return;

    Record* records = (Record*)malloc(maxPossibleRecords * sizeof(Record));
    if (!records) {
        fprintf(stderr, "Memory allocation failed for chunk sorting.\n");
        return;
    }

    CHUNK_RecordIterator recordIterator = CHUNK_CreateRecordIterator(chunk);
    int actualRecords = 0;
    
    // Διαβάζουμε μέχρι να τελειώσουν οι πραγματικές εγγραφές (προστασία από μισογεμάτα blocks)
    while (actualRecords < maxPossibleRecords) {
        if (CHUNK_GetNextRecord(&recordIterator, &records[actualRecords]) == 0) {
            actualRecords++;
        } else {
            // Φτάσαμε στο τέλος των έγκυρων εγγραφών του chunk
            break; 
        }
    }

    if (actualRecords == 0) {
        free(records);
        return;
    }

    // Ταξινόμηση μόνο των πραγματικών εγγραφών που διαβάστηκαν με qsort
    qsort(records, actualRecords, sizeof(Record), compareRecords);

    // Γράφουμε τις ταξινομημένες εγγραφές πίσω στο chunk.
    for (int i = 0; i < actualRecords; i++) {
        if (CHUNK_UpdateIthRecord(chunk, i, records[i]) == -1) {
            fprintf(stderr, "Error writing sorted record to chunk.\n");
            free(records);
            return;
        }
    }

    free(records);
}

// Δημιουργούμε μία συνάρτηση για να ταξινομήσουμε το αρχείο ανά chunks.
void sort_FileInChunks(int file_desc, int numBlocksInChunk) {
    CHUNK_Iterator chunkIterator = CHUNK_CreateIterator(file_desc, numBlocksInChunk);
    CHUNK chunk;

    // Διατρέχουμε όλα τα chunks του αρχείου και τα ταξινομούμε.
    while (CHUNK_GetNext(&chunkIterator, &chunk) == 0) {
        sort_Chunk(&chunk);
    }
}