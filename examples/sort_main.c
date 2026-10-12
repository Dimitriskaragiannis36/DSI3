#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "merge.h"


int createAndPopulateHeapFile(char *filename, int totalRecords);
void sortPhase(int file_desc, int chunkSize);
void mergePhases(char *filename,int inputFileDesc, int chunkSize, int bWay, int *fileCounter);
int nextOutputFile(char *filename,int *fileCounter);
void test(char* filename,int totalRecords, int chunkSize, int bWay);

int main()
{
    // Αρχικοποίηση του επιπέδου BF με πολιτική αντικατάστασης LRU
    BF_Init(LRU);

    printf("========================================================\n");
    printf("   ΦΑΣΗ 1: ΒΑΣΙΚΑ ΠΕΙΡΑΜΑΤΑ ΕΛΕΓΧΟΥ (Από την εκφώνηση)  \n");
    printf("========================================================\n");
    test("./test1.db", 4608, 1, 2);
    test("./test2.db", 11520, 5, 4);

    printf("\n========================================================\n");
    printf(" ΦΑΣΗ 2: ΠΕΙΡΑΜΑ ΥΨΗΛΟΥ ΦΟΡΤΟΥ ΚΑΙ ΒΑΘΕΟΣ ΔΕΝΤΡΟΥ (Deep) \n");
    printf("========================================================\n");
    test("./test3_heavy.db", 50000, 2, 2);

    printf("\n========================================================\n");
    printf("  ΦΑΣΗ 3: ΠΕΙΡΑΜΑ ΠΛΑΤΕΟΣ ΔΕΝΤΡΟΥ ΣΥΓΧΩΝΕΥΣΗΣ (10-Way)   \n");
    printf("========================================================\n");
    test("./test4_wide.db", 25000, 4, 10);

    printf("\n========================================================\n");
    printf("  ΦΑΣΗ 4: ΑΣΥΜΜΕΤΡΑ ΔΕΔΟΜΕΝΑ & ΠΡΩΤΟΙ ΑΡΙΘΜΟΙ (Edge Case)\n");
    printf("========================================================\n");
    test("./test5_primes.db", 17389, 3, 7);

    printf("\n========================================================\n");
    printf("     ΦΑΣΗ 5: ΤΑΞΙΝΟΜΗΣΗ ΣΕ ΕΝΑ ΠΕΡΑΣΜΑ (Single Chunk)    \n");
    printf("========================================================\n");
    test("./test6_single.db", 2000, 500, 4);

    printf("\n========================================================\n");
    printf("   ΦΑΣΗ 6: ΑΚΡΑΙΟΣ ΟΓΚΟΣ ΔΕΔΟΜΕΝΩΝ (250.000 Εγγραφές)    \n");
    printf("========================================================\n");
    // Πείραμα 7: Δημιουργεί ένα τεράστιο αρχείο ~10-15MB. 
    // Δοκιμάζει την αντοχή του δίσκου και του λειτουργικού συστήματος στη
    // δημιουργία και διαγραφή πολλών μεγάλων προσωρινών αρχείων.
    test("./test7_massive.db", 250000, 10, 5);

    printf("\n========================================================\n");
    printf("  ΦΑΣΗ 7: ΕΞΑΝΤΛΗΣΗ ΟΡΙΩΝ BUFFER MANAGER (80-Way Merge)  \n");
    printf("========================================================\n");
    // Πείραμα 8: Το BF_BUFFER_SIZE είναι 100. Κάνοντας 80-way merge, 
    // ζητάμε από τον Buffer Manager να κρατάει καρφιτσωμένα (pinned) 
    // ταυτόχρονα τουλάχιστον 80-81 blocks στη μνήμη. Δοκιμάζει αυστηρά 
    // τον αλγόριθμο αντικατάστασης (LRU) και τυχόν Memory Leaks.
    test("./test8_extreme_wide.db", 100000, 5, 80);

    printf("\n========================================================\n");
    printf("    ΦΑΣΗ 8: Ο ΜΑΡΑΘΩΝΙΟΣ (500.000 Εγγραφές, 1-Block)     \n");
    printf("========================================================\n");
    // Πείραμα 9: Το απόλυτο stress-test. Μισό εκατομμύριο εγγραφές, 
    // οι οποίες αρχικά ταξινομούνται ανά ΕΝΑ block. Σε συνδυασμό με 4-way merge, 
    // ο αλγόριθμος θα χρειαστεί τεράστιο αριθμό I/O operations. 
    // (ΠΡΟΣΟΧΗ: Μπορεί να χρειαστεί 1-2 λεπτά για να εκτελεστεί).
    test("./test9_marathon.db", 500000, 1, 4);

    return 0;
}

int createAndPopulateHeapFile(char *filename, int totalRecords)
{
	HP_CreateFile(filename);

	int file_desc;
	HP_OpenFile(filename, &file_desc);

	Record record;
	srand(12569874);
	for (int id = 0; id < totalRecords; ++id)
	{
		record = randomRecord();
		HP_InsertEntry(file_desc, record);
	}
	return file_desc;
}

/*Performs the sorting phase of external merge sort algorithm on a file specified by 'file_desc', using chunks of size 'chunkSize'*/
void sortPhase(int file_desc, int chunkSize)
{
	sort_FileInChunks(file_desc, chunkSize);
}

/* Performs the merge phase of the external merge sort algorithm  using chunks of size 'chunkSize' and 'bWay' merging. The merge phase may be performed in more than one cycles.*/
void mergePhases(char *filename, int inputFileDesc, int chunkSize, int bWay, int *fileCounter)
{
    int outputFileDesc = -1;

    while (chunkSize < HP_GetIdOfLastBlock(inputFileDesc))
    {
        outputFileDesc = nextOutputFile(filename,fileCounter);
        merge(inputFileDesc, chunkSize, bWay, outputFileDesc);
        HP_CloseFile(inputFileDesc);
        chunkSize *= bWay;
        inputFileDesc = outputFileDesc;
    }

    if (outputFileDesc != -1) {
        HP_CloseFile(outputFileDesc);
    } else {
        // Κρίσιμη προσθήκη: Κλείσιμο του αρχείου εισόδου όταν η ταξινόμηση ολοκληρώνεται σε 1 chunk (Φάση 5)
        HP_CloseFile(inputFileDesc);
    }
}

/*Creates a sequence of heap files: out0.db, out1.db, ... and returns for each heap file its corresponding file descriptor. */
int nextOutputFile(char *filename,int *fileCounter)
{
	char mergedFile[50];
	sprintf(mergedFile, "%s%d.db", filename, (*fileCounter)++);
	int file_desc;
	HP_CreateFile(mergedFile);
	HP_OpenFile(mergedFile, &file_desc);
	return file_desc;
}

int getNumberOfChunks(char *filename);


/**
 * @brief Executes and validates an external merge sort on a heap file.
 *
 * This function creates a heap file, populates it with random records, 
 * sorts it in chunks, and performs multi-way merge phases until the file 
 * is fully sorted. It reports the number of chunks after each phase.
 *
 * @param fileName      Name of the heap file to process.
 * @param totalRecords  Number of random records to populate in the file.
 * @param chunkSize     Size (in blocks) of chunks for sorting and merging.
 * @param bWay          Number of chunks to merge simultaneously in each phase.
 *
 * @output
 * Prints the number of chunks after the sorting phase and each merge phase.
 *
 * Example:
 * test("./test1.db", 2700, 1, 2); // Processes file "./test1.db".
 */
void test(char* fileName, int totalRecords, int chunkSize,int bWay){
	// Create and populate the heap file with initial data
    int heapFileDescriptor = createAndPopulateHeapFile(fileName,totalRecords);
    printf("File %s has %d blocks\n",fileName,HP_GetIdOfLastBlock(heapFileDescriptor));


    // Perform the sorting phase on the heap file
    sortPhase(heapFileDescriptor, chunkSize);

    

    // Placeholder for printing all entries in the heap file (commented out)
    // HP_PrintAllEntries(heapFileDescriptor);

    // Initialize iterator for merge phases
    int mergeIterator = 1;

    // Perform the merge phases to further organize data in the heap file
    mergePhases(fileName,heapFileDescriptor, chunkSize, bWay, &mergeIterator);

	// Count the number of chunks in the file after the sorting phase
    int chunkCount = getNumberOfChunks(fileName);
    printf("After the sort phase, file %s has %d chunk(s)\n", fileName, chunkCount);
    // Check the state of merged files
    char mergedFileName[50];
    for (int i = 1; i < mergeIterator; i++)
    {
        sprintf(mergedFileName, "%s%d.db", fileName, i);
        int mergedChunkCount = getNumberOfChunks(mergedFileName);
        printf("After the merge phase %d, the output file %s has %d chunk(s)\n",i,mergedFileName, mergedChunkCount);
    }
}

// This function checks if two records are in the correct order based on name and surname
bool areRecordsInOrder(Record recordA, Record recordB)
{
    int nameComparison = strcmp(recordA.name, recordB.name);
    if (nameComparison > 0)
    {
        return false;
    }
    if (nameComparison == 0 && strcmp(recordA.surname, recordB.surname) > 0)
    {
        return false;
    }
    return true;
}

// This function checks if all records in a block are sorted
bool isBlockSorted(int fileDescriptor, int blockId)
{
    int totalRecords = HP_GetRecordCounter(fileDescriptor, blockId);

    // If the block has one or no records, it is sorted by definition
    if (totalRecords <= 1)
    {
        return true;
    }

    // Compare each record with the next to ensure the block is sorted
    Record currentRecord;
    HP_GetRecord(fileDescriptor, blockId, 0, &currentRecord);
    Record nextRecord;
    for (int i = 1; i < totalRecords; i++)
    {
        HP_GetRecord(fileDescriptor, blockId, i, &nextRecord);
        if (!areRecordsInOrder(currentRecord, nextRecord))
        {
            return false;
        }
        currentRecord = nextRecord;
    }
    return true;
}

// This function calculates the number of sorted chunks in a heap file
int getNumberOfChunks(char *fileName)
{
    int fileDescriptor;

    // Open the heap file
    HP_OpenFile(fileName, &fileDescriptor);

    int chunkCount = 0;
    int previousBlockId = 1;

    // Get the ID of the last block in the file
    int lastBlockId = HP_GetIdOfLastBlock(fileDescriptor);

    // If the file has no blocks or the first block is not sorted, return an error
    if (lastBlockId < 1 || !isBlockSorted(fileDescriptor, previousBlockId))
    {
        HP_Unpin(fileDescriptor, previousBlockId);
        return -1;
    }

    // Iterate through all blocks to identify sorted chunks
    for (int currentBlockId = 2; currentBlockId <= lastBlockId; currentBlockId++)
    {
        if (!isBlockSorted(fileDescriptor, currentBlockId))
        {
            HP_Unpin(fileDescriptor, previousBlockId);
            HP_Unpin(fileDescriptor, currentBlockId);
            return -1;
        }

        Record lastRecordOfPreviousBlock;
        Record firstRecordOfCurrentBlock;

        // Compare the last record of the previous block with the first record of the current block
        HP_GetRecord(fileDescriptor, previousBlockId, HP_GetRecordCounter(fileDescriptor, previousBlockId) - 1, &lastRecordOfPreviousBlock);
        HP_GetRecord(fileDescriptor, currentBlockId, 0, &firstRecordOfCurrentBlock);

        if (!areRecordsInOrder(lastRecordOfPreviousBlock, firstRecordOfCurrentBlock))
        {
            chunkCount++;
        }
        HP_Unpin(fileDescriptor, previousBlockId);
        previousBlockId = currentBlockId;
    }

    // Account for the last chunk
    HP_Unpin(fileDescriptor, previousBlockId);
    chunkCount++;

    // Close the file
    HP_CloseFile(fileDescriptor);
    return chunkCount;
}