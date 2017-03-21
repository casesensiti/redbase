//
// File:        ix_indexmanager.cc
// Description: IX_IndexHandle handles indexes
// Author:      Yifei Huang (yifei@stanford.edu)
//

#include <unistd.h>
#include <sys/types.h>
#include "ix.h"
#include "ix_internal.h"
#include "pf.h"
#include <climits>
#include <string>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include "comparators.h"

// helper functions
RC IX_Manager::getIndexFileName(const char* fileName, int indexNo, char* dest){
    RC rc = 0;
    std::string sFileName(fileName);
    std::string sIndexNo = std::to_string(indexNo);
    std::string sIndexFileName = sFileName + "." + sIndexNo;
    strcpy(dest, sIndexFileName.c_str());
    return (rc);
}
// Print index
RC IX_Manager::Print(const char *indexFileName)
{
#ifdef MY_DEBUG
    printf("IX_Manager::Print invoked.\n");
#endif
    RC rc = 0;
    IX_IndexHandle ih;
    std::string name(indexFileName);
    std::string fileName, indexNo;
    std::size_t found = name.find('.');
    if (found == std::string::npos)
        return IX_INVALIDINDEXNAME;
    fileName = name.substr(0, found);
    indexNo  = name.substr(found+1);
#ifdef MY_DEBUG
    printf("fileName: %s, indexNo: %d\n", fileName.c_str(), atoi(indexNo.c_str()));
#endif

    if ((rc = OpenIndex(fileName.c_str(), atoi(indexNo.c_str()), ih)))
        return rc;
    if ((rc = ih.Print()))
        return rc;
    return rc;
}


IX_Manager::IX_Manager(PF_Manager &pfm): pfm(pfm)
{
}

IX_Manager::~IX_Manager()
{

}

/*
 * Creates a new index given the filename, the index number, attribute type and length.
 */
RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength)
{
    RC rc = 0;
#ifdef MY_DEBUG
    printf("CreateIndex invoked for file %s, on indexNo %d, with attrType %d, attrLength %d\n", fileName, indexNo, attrType, attrLength);
    printf("size of IX_NoheHeader: %d, size of IX_Entry: %d", sizeof(IX_NodeHeader), sizeof(IX_Entry));
#endif

    // check integrity of arguments
    if (fileName == NULL)
        return IX_BADFILENAME;
    if (attrType != MBR || attrLength != sizeof(struct MBR))
        return IX_BADINDEXSPEC;
    if (indexNo >= 10000)
        return IX_BADINDEXNO;
    int entrySize = sizeof(struct IX_Entry);
    // to correct: this M = 6 is set for easier test.
    //int M = (PF_PAGE_SIZE - sizeof(IX_NodeHeader)) / entrySize;
    int M = 6;
    int m = M / 3;
    if (m < 1)
        return IX_BADENTRYSIZE;
    char indexFileName[MAXSTRINGLEN + 10];
    if (rc = getIndexFileName(fileName, indexNo, indexFileName))
        return rc;

#ifdef MY_DEBUG
    printf("index file name: %s\n", indexFileName);
#endif

    if((rc = pfm.CreateFile(indexFileName)))
        return (rc);
    // Opens the file, creates a new page and copies the header into it
    PF_PageHandle ph, phr;
    PF_FileHandle fh;
    struct IX_FileHeader *header;
    struct IX_NodeHeader *rHeader;
    if((rc = pfm.OpenFile(indexFileName, fh)))
        return (rc);
    PageNum page, rPage;
    if((rc = fh.AllocatePage(ph)) || (rc = ph.GetPageNum(page)))
        return (rc);
    if((rc = fh.AllocatePage(phr)) || (rc = phr.GetPageNum(rPage)))
        return (rc);
    char *pData, *prData;
    if((rc = ph.GetData(pData)) || (rc = phr.GetData(prData))){
        goto cleanup_and_exit;
    }

    rHeader = (struct IX_NodeHeader *) prData;
    rHeader->ifUsed = TRUE;
    rHeader->isTreeNode = false;
    rHeader->isRoot = true;
    rHeader->numEntry = 0;
    rHeader->nextFreePage = NO_MORE_FREE_PAGES;
    rHeader->firstFreeSlot = NO_NEXT_SLOT;
    rHeader->firstEntry = NO_NEXT_ENTRY;
    rHeader->parent = RID(0,0); // no parent entry

    header = (struct IX_FileHeader *) pData;
    header->entrySize = entrySize;
    header->M = M;
    header->m = m;
    header->numFreePage = 0;
    header->numPage = 2;
    header->rootPage = rPage;
#ifdef MY_DEBUG
    printf("EntrySize: %d, M: %d\n", entrySize, M);
#endif
    //memcpy(pData, &header, sizeof(struct RM_FileHeader));

    // always unpin the page, and close the file before exiting
    cleanup_and_exit:
    RC rc2;
    if((rc2 = fh.MarkDirty(page)) || (rc2 = fh.UnpinPage(page)) || (rc2 = fh.MarkDirty(rPage))
       || (rc2 = fh.UnpinPage(rPage)) || (rc2 = pfm.CloseFile(fh)))
        return (rc2);
    fh.ForcePages();
    return (rc);
}

/*
 * This function destroys a valid index given the file name and index number.
 */
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo)
{
    if(fileName == NULL)
        return (IX_BADFILENAME);
    RC rc;
    char indexFileName[MAXSTRINGLEN + 10];
    if ((rc = getIndexFileName(fileName, indexNo, indexFileName)))
        return rc;
    if((rc = pfm.DestroyFile(indexFileName)))
        return (rc);
    return (0);
}

/*
 * This function, given a valid fileName and index Number, opens up the
 * index associated with it, and returns it via the indexHandle variable
 */
RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle)
{
    RC rc = 0;
#ifdef MY_DEBUG
    printf("IX_Manager::OpenIndex invoked. ");
#endif
    if (fileName == NULL)
        return IX_BADFILENAME;
    if (indexHandle.openedIH)
        return IX_INVALIDINDEXHANDLE;
    char indexFileName[MAXSTRINGLEN+10];
    if ((rc = getIndexFileName(fileName, indexNo, indexFileName)))
        return rc;
    // open file
    if (rc = pfm.OpenFile(indexFileName, indexHandle.pfh))
        return rc;
    // read header
    PF_PageHandle ph;
    PageNum page;
    if ((rc = indexHandle.pfh.GetFirstPage(ph)) || (rc = ph.GetPageNum(page))) {
        indexHandle.pfh.UnpinPage(page);
        pfm.CloseFile(indexHandle.pfh);
        return rc;
    }
    char* pData;
    if (rc = ph.GetData(pData))
        return rc;
    // set up private variables
    memcpy(&indexHandle.fileHeader, pData, sizeof(struct IX_FileHeader));
    indexHandle.openedIH = true;
    indexHandle.headerModified = false;
    // Unpin the header page
    RC rc2;
    if((rc2 = indexHandle.pfh.UnpinPage(page)))
        return (rc2);
#ifdef MY_DEBUG
    printf("Index open success\n");
#endif
    return rc;
}

/*
 * Given a valid index handle, closes the file associated with it
 */
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)
{
    RC rc = 0;
    PF_PageHandle ph;
    PageNum page;
    char* pData;
    if (indexHandle.openedIH != true)
        return IX_INVALIDINDEXHANDLE;
    // if header modified, rewrite header page
    if (indexHandle.headerModified) {
        if ((rc = indexHandle.pfh.GetFirstPage(ph)) || (rc = ph.GetPageNum(page)))
            return rc;
        if((rc = ph.GetData(pData))){
            RC rc2;
            if((rc2 = indexHandle.pfh.UnpinPage(page)))
                return (rc2);
            return (rc);
        }
        memcpy(pData, &indexHandle.fileHeader, sizeof(struct IX_FileHeader));
        if((rc = indexHandle.pfh.MarkDirty(page)) || (rc = indexHandle.pfh.UnpinPage(page)))
            return (rc);
    }
    // close PF_PageHandler
    if ((rc = pfm.CloseFile(indexHandle.pfh)))
        return rc;
    indexHandle.openedIH = false;
    indexHandle.headerModified = false;
    return rc;
}

