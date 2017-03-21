//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

#include "redbase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"
#include <string>
#include <cstdlib>
#include <cstring>


//
// IX_FileHeader: Header of index file
//
struct IX_FileHeader {
    int entrySize;            // entry size in file
    int M;                    // max # of entries per node
    int m;                    // min # of entries per node
    int numPage;              // number of pages, include header page
    int numFreePage;          // number of free pages in the linked list
    PageNum rootPage;         // pointer to root node
    PageNum firstFreePage;    // pointer to the first free page
};

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    friend class IX_Manager;
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();

    // Print this index
    RC Print();
private:
    RC InsertToNode(void* pageData, void* pData, const RID &rid, SlotNum& insertedSlot);
    RC ChooseLeaf(const struct MBR& m, PageNum& page);
    float calcaEnlarge(const struct MBR& inner, struct MBR& outer);
    RC SplitNode(PageNum page, void* pData, const RID& rid, RID& insertedPos, PageNum& newPage);
    RC ExpandMBR(struct MBR& inner, struct MBR& outer);
    RC AdjustTree(PageNum page, PageNum page2);
    RC PrintNodeHeader(struct IX_NodeHeader* pnh);
    RC PrintEntry(struct IX_Entry* pe);

    bool headerModified; // if header modified, should rewrite the header page
    bool openedIH; // whether this handle has been opened
    IX_FileHeader fileHeader;
    PF_FileHandle pfh; // file handler for the opened index file
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
public:
    IX_IndexScan();
    ~IX_IndexScan();

    // Open index scan
    RC OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);


    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID &rid);

    // Close index scan
    RC CloseScan();
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
public:
    IX_Manager(PF_Manager &pfm);
    ~IX_Manager();

    // Create a new Index
    RC CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength);

    // Destroy and Index
    RC DestroyIndex(const char *fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle &indexHandle);

    // print an index
    RC Print(const char* indexFileName);
private:
    PF_Manager &pfm;
    // helper to construct the index file name
    RC getIndexFileName(const char* fileName, int indexNo, char* dest);

};

//
// Print-error function
//
void IX_PrintError(RC rc);

#define IX_BADINDEXSPEC         (START_IX_WARN + 0) // Bad Specification for Index File
#define IX_BADINDEXNAME         (START_IX_WARN + 1) // Bad index name
#define IX_INVALIDINDEXHANDLE   (START_IX_WARN + 2) // FileHandle used is invalid
#define IX_INVALIDINDEXFILE     (START_IX_WARN + 3) // Bad index file
#define IX_NODEFULL             (START_IX_WARN + 4) // A node in the file is full
#define IX_BADFILENAME          (START_IX_WARN + 5) // Bad file name
#define IX_INVALIDBUCKET        (START_IX_WARN + 6) // Bucket trying to access is invalid
#define IX_DUPLICATEENTRY       (START_IX_WARN + 7) // Trying to enter a duplicate entry
#define IX_INVALIDSCAN          (START_IX_WARN + 8) // Invalid IX_Indexscsan
#define IX_INVALIDENTRY         (START_IX_WARN + 9) // Entry not in the index
#define IX_EOF                  (START_IX_WARN + 10)// End of index file
#define IX_BADINDEXNO           (START_IX_WARN + 11)// Bad Specification for Index File, indexNo too big
#define IX_BADENTRYSIZE         (START_IX_WARN + 12)// Bad entry or header size
#define IX_BADMBRENTRY          (START_IX_WARN + 13)// Bad mbr entry passed to calcaEnlarge
#define IX_BADENTRY             (START_IX_WARN + 14)// Entry format not valid
#define IX_INVALIDINDEXNAME     (START_IX_WARN + 15)// In Print, index file name not valid
#define IX_INVALIDNODETOSPLIT   (START_IX_WARN + 16)// In SplitNode, the node to be splitted is not qualified.
#define IX_LASTWARN             IX_EOF

#define IX_ERROR                (START_IX_ERR - 0) // error
#define IX_LASTERROR            IX_ERROR

#endif
