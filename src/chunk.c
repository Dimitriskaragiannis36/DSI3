#include "chunk.h"
#include "hp_file.h"
#include "record.h"
#include <stdio.h>
#include <string.h>
#include "bf.h"

//Δημιουργούμε έναν iterator για τα chunks, δηλαδή τα τμήματα του αρχείου. Με άλλα λόγια, διαβάζουμε τα chunks του αρχείου.
//Τα chunks είναι τμήματα του αρχείου που περιέχουν πολλές εγγραφές.
CHUNK_Iterator CHUNK_CreateIterator(int fileDesc, int blocksInChunk) {
    //printf("[DEBUG] Creating CHUNK_Iterator. fileDesc: %d, blocksInChunk: %d\n", fileDesc, blocksInChunk);
    CHUNK_Iterator iterator; //Δημιουργούμε έναν iterator. Ο iterator είναι ένας δείκτης που δείχνει σε ένα τμήμα του αρχείου.
    iterator.file_desc = fileDesc; //Δίνουμε στον iterator το fileDesc του αρχείου.
    iterator.current = 1; //Ο iterator δείχνει στο πρώτο block του αρχείου, θεωρώντας ότι το block 0 περιέχει μεταδεδομένα.
    iterator.lastBlocksID = HP_GetIdOfLastBlock(fileDesc); //Εντοπίζουμε το τελευταίο block του αρχείου μέσω της HP_GetIdOfLastBlock.
    iterator.blocksInChunk = blocksInChunk; //Θέτουμε το πλήθος των blocks που περιέχει κάθε chunk.
    //printf("[DEBUG] Iterator created. lastBlocksID: %d\n", iterator.lastBlocksID);
    return iterator;
}

//Παίρνουμε το επόμενο chunk από τον iterator.
int CHUNK_GetNext(CHUNK_Iterator *iterator, CHUNK *chunk) {
    //printf("[DEBUG] Getting next CHUNK. Current block: %d, Last block: %d\n", iterator->current, iterator->lastBlocksID);
    if (iterator->current > iterator->lastBlocksID) {
        //printf("[DEBUG] No more chunks available.\n");
        return -1; //Αν δεν υπάρχουν άλλα chunks, επιστρέφουμε -1.
    }

    chunk->file_desc = iterator->file_desc; //Περνάμε το file_desc του iterator στο file_desc του chunk.
    chunk->from_BlockId = iterator->current; //Θέτουμε το from_BlockId του chunk ίσο με το current του iterator.
    chunk->to_BlockId = iterator->current + iterator->blocksInChunk - 1; //Υπολογίζουμε το to_BlockId του chunk.
    if (chunk->to_BlockId > iterator->lastBlocksID) { //Αν το to_BlockId είναι μεγαλύτερο από το lastBlocksID
        chunk->to_BlockId = iterator->lastBlocksID; //Τότε το to_BlockId γίνεται ίσο με το lastBlocksID.
    }
    //Υπολογίζουμε το πλήθος των blocks και των εγγραφών στο chunk.
    chunk->blocksInChunk = chunk->to_BlockId - chunk->from_BlockId + 1;
    chunk->recordsInChunk = chunk->blocksInChunk * HP_GetMaxRecordsInBlock(iterator->file_desc);

    //printf("[DEBUG] CHUNK created. from_BlockId: %d, to_BlockId: %d, recordsInChunk: %d\n",
    //       chunk->from_BlockId, chunk->to_BlockId, chunk->recordsInChunk);

    iterator->current = chunk->to_BlockId + 1;
    return 0; // Success
}

//Παίρνουμε την i-οστή εγγραφή μέσα από το chunk.
int CHUNK_GetIthRecordInChunk(CHUNK *chunk, int i, Record *record) {
    //printf("[DEBUG] Getting record %d from CHUNK. Total records: %d\n", i, chunk->recordsInChunk);
    if (i < 0 || i >= chunk->recordsInChunk) {
        //printf("[DEBUG] Invalid record index: %d\n", i);
        return -1; //Αν το i είναι εκτός ορίων, επιστρέφουμε -1.
    }

    int recordsPerBlock = HP_GetMaxRecordsInBlock(chunk->file_desc); //Βρίσκουμε πόσες εγγραφές χωράει ένα block.
    int blockOffset = i / recordsPerBlock; //Βρίσκουμε σε ποιο block ανήκει η εγγραφή.
    int recordOffset = i % recordsPerBlock; //Βρίσκουμε την ακριβή θέση της εγγραφής μέσα στο block.

    int blockId = chunk->from_BlockId + blockOffset; //Ορίζουμε ποιο block θα διαβάσουμε.
    //printf("[DEBUG] Target block: %d, Record offset: %d\n", blockId, recordOffset);

    //Παίρνουμε την εγγραφή από το block στη συγκεκριμένη θέση.
    if (HP_GetRecord(chunk->file_desc, blockId, recordOffset, record) == 0) {
        //printf("[DEBUG] Record retrieved successfully from block %d\n", blockId);
        HP_Unpin(chunk->file_desc, blockId); //Απελευθερώνουμε το block μετά τη χρήση.
        return 0; // Success
    }

    //printf("[DEBUG] Failed to retrieve record.\n");
    return -1; // Failure
}

//Ενημερώνουμε την i-οστή εγγραφή μέσα στο chunk.
int CHUNK_UpdateIthRecord(CHUNK *chunk, int i, Record record) {
    //printf("[DEBUG] Updating record %d in CHUNK. Total records: %d\n", i, chunk->recordsInChunk);
    if (i < 0 || i >= chunk->recordsInChunk) {
        //printf("[DEBUG] Invalid record index: %d\n", i);
        return -1; //Αν το i είναι εκτός ορίων, επιστρέφουμε -1.
    }

    int recordsPerBlock = HP_GetMaxRecordsInBlock(chunk->file_desc); //Βρίσκουμε πόσες εγγραφές χωράει ένα block.
    int blockOffset = i / recordsPerBlock; //Βρίσκουμε σε ποιο block βρίσκεται η εγγραφή.
    int recordOffset = i % recordsPerBlock; //Βρίσκουμε τη θέση της εγγραφής μέσα στο block.

    int blockId = chunk->from_BlockId + blockOffset; //Καθορίζουμε ποιο block θα ενημερώσουμε.
    //printf("[DEBUG] Target block: %d, Record offset: %d\n", blockId, recordOffset);

    //Καλούμε τη συνάρτηση ενημέρωσης εγγραφής.
    if (HP_UpdateRecord(chunk->file_desc, blockId, recordOffset, record) == 0) {
        //printf("[DEBUG] Record updated successfully in block %d\n", blockId);
        HP_Unpin(chunk->file_desc, blockId); //Απελευθερώνουμε το block μετά την ενημέρωση.
        return 0; // Success
    }

    //printf("[DEBUG] Failed to update record.\n");
    return -1; // Failure
}

//Εκτυπώνουμε τις εγγραφές μέσα στο chunk.
void CHUNK_Print(CHUNK chunk) {
    //printf("[DEBUG] Printing CHUNK. From block: %d, To block: %d\n", chunk.from_BlockId, chunk.to_BlockId);
    for (int blockId = chunk.from_BlockId; blockId <= chunk.to_BlockId; ++blockId) {
        //printf("Block %d:\n", blockId);
        HP_PrintBlockEntries(chunk.file_desc, blockId); //Εκτυπώνουμε τις εγγραφές του συγκεκριμένου block.
    }
}

//Δημιουργούμε έναν record iterator για το συγκεκριμένο chunk.
CHUNK_RecordIterator CHUNK_CreateRecordIterator(CHUNK *chunk) {
    //printf("[DEBUG] Creating record iterator for CHUNK. From block: %d, To block: %d\n", chunk->from_BlockId, chunk->to_BlockId);
    CHUNK_RecordIterator iterator;
    iterator.chunk = *chunk; //Αποθηκεύουμε το chunk στον iterator.
    iterator.currentBlockId = chunk->from_BlockId; //Ορίζουμε το πρώτο block του chunk.
    iterator.cursor = 0; //Αρχικοποιούμε τον δείκτη εγγραφών.
    return iterator;
}

//Παίρνουμε την επόμενη εγγραφή από τον record iterator.
int CHUNK_GetNextRecord(CHUNK_RecordIterator *iterator, Record *record) {
    //printf("[DEBUG] Getting next record. Current block: %d, Cursor: %d\n", iterator->currentBlockId, iterator->cursor);
    if (iterator->currentBlockId > iterator->chunk.to_BlockId) {
        //printf("[DEBUG] No more records in iterator.\n");
        return -1; //Δεν υπάρχουν άλλες εγγραφές.
    }

    //Αναζητούμε την επόμενη εγγραφή.
    if (HP_GetRecord(iterator->chunk.file_desc, iterator->currentBlockId, iterator->cursor, record) == 0) {
        //printf("[DEBUG] Record retrieved successfully from block %d\n", iterator->currentBlockId);
        iterator->cursor++; //Αυξάνουμε τον δείκτη εγγραφών μέσα στο block.

        //Αν ξεπεράσαμε τον αριθμό εγγραφών σε αυτό το block
        if (iterator->cursor >= HP_GetRecordCounter(iterator->chunk.file_desc, iterator->currentBlockId)) {
            HP_Unpin(iterator->chunk.file_desc, iterator->currentBlockId); //Απελευθερώνουμε το block.
            iterator->cursor = 0; //Μηδενίζουμε τον δείκτη εγγραφών.
            iterator->currentBlockId++; //Μεταβαίνουμε στο επόμενο block.
        }

        return 0; // Success
    }

    //printf("[DEBUG] Failed to retrieve record.\n");
    return -1; // Failure
}
