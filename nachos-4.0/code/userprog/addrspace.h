// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include <string.h>

#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
  public:
    AddrSpace();			// Create an address space.
    ~AddrSpace();			// De-allocate an address space

    void Execute(char *fileName);	// Run the the program
					// stored in the file "executable"

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch 

  private:
    TranslationEntry *pageTable;	// Assume linear page table translation
					// for now!
    unsigned int numPages;		// Number of pages in the virtual 
					// address space

    bool Load(char *fileName);		// Load the program into memory
					// return false if not found

    void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

};

class FrameInfoEntry {
  public:
    FrameInfoEntry();

    bool valid; //if being used
    bool lock;
    TranslationEntry *pageTable; //which process is using this page
    unsigned int vpn; //which virtual page of the process is stored in
                      //this page
    unsigned count;
};

class MemoryManager {
  public:
    MemoryManager();
    ~MemoryManager();

    bool AccessPage(TranslationEntry *pageTable, int vpn);
    bool AcquirePage(TranslationEntry *pageTable, int vpn); // ask a page (frame) for vpn
    void ReleaseAll(TranslationEntry *pageTable, int num);

  private :
    FrameInfoEntry *frameTable;
    FrameInfoEntry *swapTable;
    unsigned count;

    void SetNewPage(TranslationEntry *pageTable, int vpn, int ppn);
    bool RestorePage(TranslationEntry *pageTable, int vpn, int ppn);
    int  SavePage();

};


#endif // ADDRSPACE_H
