#include <merge.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

void merge(int input_FileDesc, int chunkSize, int bWay, int output_FileDesc) {
    // [DEBUG] Μηνύματα για αρχική κατάσταση
    //printf("[DEBUG] Starting merge process.\n");
    //printf("[DEBUG] Input FileDesc: %d, Chunk Size: %d, bWay: %d, Output FileDesc: %d\n", 
    //        input_FileDesc, chunkSize, bWay, output_FileDesc);
    
    // Δημιουργούμε έναν CHUNK_Iterator για να διαβάζουμε κομμάτια (chunks) από το αρχείο εισόδου.
    CHUNK_Iterator chunkIter = CHUNK_CreateIterator(input_FileDesc, chunkSize);

    // Διαβάζουμε bWay chunks κάθε φορά και τα συγχωνεύουμε σε **ένα** chunk στο output.
    while (1) {
        CHUNK chunks[BUFSIZ];  // bWay chunks θα διαβάσουμε, αλλά για ασφάλεια χρησιμοποιούμε το BUFSIZ.
        CHUNK_RecordIterator recIters[BUFSIZ];
        Record currentRecords[BUFSIZ];
        bool active[BUFSIZ];   // Ελέγχουμε αν κάθε chunk είναι ακόμα ενεργό (αν δεν έχει αδειάσει).
        int chunkCount = 0;

        // -- 1. Διαβάζουμε μέχρι bWay chunks --
        for (int i = 0; i < bWay; i++) {
            if (CHUNK_GetNext(&chunkIter, &chunks[i]) != 0) {
                // Δεν υπάρχει άλλο chunk στο input => σταματάμε.
                break;
            }
            // Δημιουργούμε έναν iterator για τα records του chunk.
            recIters[i] = CHUNK_CreateRecordIterator(&chunks[i]);
            // Παίρνουμε το πρώτο record του chunk.
            if (CHUNK_GetNextRecord(&recIters[i], &currentRecords[i]) == 0) {
                active[i] = true; 
            } else {
                active[i] = false; 
            }
            chunkCount++;
        }

        // Αν δεν διαβάσαμε ούτε *ένα* chunk, τότε τελειώσαμε το merging.
        if (chunkCount == 0) {
            break;
        }

        // -- 2. Ξεκινάμε να «βγάζουμε» τα μικρότερα records από τα chunks, ένα-ένα, στο output --
        // Μέχρι να αδειάσουν όλα τα chunks που διαβάσαμε.
        while (1) {
            // Βρίσκουμε ποιο chunk έχει το "ελάχιστο" record.
            int minIndex = -1;
            for (int i = 0; i < chunkCount; i++) {
                if (active[i]) {
                    if (minIndex == -1) {
                        minIndex = i;
                    } else {
                        // Χρησιμοποιούμε τη shouldSwap() ΣΑΣ, όπου:
                        // αν shouldSwap(&currentRecords[minIndex], &currentRecords[i]) είναι αληθές,
                        // τότε currentRecords[i] είναι «μικρότερο» και το επιλέγουμε.
                        if (shouldSwap(&currentRecords[minIndex], &currentRecords[i])) {
                            minIndex = i;
                        }
                    }
                }
            }

            // Αν κανένα chunk δεν είναι ενεργό, σημαίνει ότι όλα άδειασαν.
            if (minIndex == -1) {
                break; 
            }

            // Γράφουμε το ελάχιστο record στο αρχείο εξόδου.
            // (π.χ. HP_InsertEntry, το οποίο δεσμεύει δυναμικά blocks)
            HP_InsertEntry(output_FileDesc, currentRecords[minIndex]);

            // Διαβάζουμε το επόμενο record από το ίδιο chunk.
            if (CHUNK_GetNextRecord(&recIters[minIndex], &currentRecords[minIndex]) != 0) {
                // Αν απέτυχε, σημαίνει ότι άδειασαν τα records του συγκεκριμένου chunk.
                active[minIndex] = false;
            }
        }

        // Συνεχίζουμε, διαβάζοντας τα επόμενα bWay chunks (αν υπάρχουν).
    }

    // Στο τέλος, έχουμε συγχωνεύσει ΟΛΑ τα chunks του input αρχείου
    // σε λιγότερα, ταξινομημένα chunks στο output αρχείο.
    // (π.χ. κάθε bWay chunks εισόδου => 1 chunk εξόδου)
    // Τέλος merge!
}
