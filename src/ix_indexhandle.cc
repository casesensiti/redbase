//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle handles manipulations within the index
// Author:      <Your Name Here>
//

#include <unistd.h>
#include <sys/types.h>
#include "ix.h"
#include "ix_internal.h"
#include <limits>
#include "pf.h"
#include "comparators.h"
#include <cstdio>

IX_IndexHandle::IX_IndexHandle()
{
    openedIH = false;
}

IX_IndexHandle::~IX_IndexHandle()
{
  // Implement this
}

RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{
  // Implement this
    RC rc = 0;
    struct MBR m = *(struct MBR *)pData;
#ifdef MY_DEBUG
    printf("InsertEntry of IX is invoked, with data %f, %f, %f, %f\n", m.llx, m.lly, m.urx, m.ury);
    if (m.llx > m.urx || m.lly > m.ury) printf("In IX_IndexHandle::InsertEntry : MBR data not valid.\n");
#endif
    // find spot
    PageNum toInsert;
    if ((rc = ChooseLeaf(m, toInsert)))
        return rc;
    // insert into node
    PF_PageHandle ph;
    char* pageData;
    IX_NodeHeader* pnh; // pointer to node header
    if ((rc = pfh.GetThisPage(toInsert, ph))) // get page info
        return rc;
    if ((rc = ph.GetData(pageData))) // get node header info
        return rc;
    pnh = (struct IX_NodeHeader *) pageData;
    // if have slot, insert to this node
    if (pnh->numEntry < fileHeader.M)
    {
        if ((rc = InsertToNode(pageData, pData, rid)))
            return rc;
        pfh.UnpinPage(toInsert);
        return rc;
    }

    // if too many, split

    return rc;
}

// insert MBR data pointed by pData, with RID& rid into Node pointed by pageData
// maintain NodeHeader information
RC IX_IndexHandle::InsertToNode(void* pageData, void* pData, const RID &rid)
{
    RC rc = 0;
    IX_NodeHeader* pnh = (struct IX_NodeHeader *)pageData;
    IX_Entry e;
    IX_Entry *pEntry;
    SlotNum free; // free slot to put new entry
    e.child = rid;
    e.ifUsed = true;
    e.m = *(struct MBR *)pData;
    // if have free slot, insert into free slot. Otherwise, insert into numEntry+1 slot
    if (pnh->firstFreeSlot != NO__NEXT_SLOT)
    {
        free = pnh->firstFreeSlot;
        pEntry = (IX_Entry *) pageData + sizeof(IX_NodeHeader) + free * sizeof(IX_Entry);
        if (pEntry->ifUsed)
            return IX_BADENTRY;
        pnh->firstFreeSlot = pEntry->nextEntry;
    } else {
        free = pnh->numEntry + 1;
    }
    // put new entry into pagaData
    e.nextEntry = pnh->firstEntry;
    pnh->firstEntry = free;
    memcpy(pageData + sizeof(IX_NodeHeader) + free * sizeof(IX_Entry), &e, sizeof(IX_Entry));
    pnh->numEntry = pnh->numEntry + 1;
    return rc;
}

RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
  // Implement this
}

RC IX_IndexHandle::ForcePages()
{
  // Implement this
}

// choose appropriate leaf node to insert MBR
RC IX_IndexHandle::ChooseLeaf(const struct MBR& m, PageNum& page) {
    RC rc = 0;
    PF_PageHandle ph;
    IX_NodeHeader nh;
    IX_Entry e;
    PageNum currPage;
    SlotNum sn;
    float enlarge;
    float minEnlarge = std::numeric_limits<float>::max(); // minimal enlargement
    int minEnlargeEntry; // record the entry which has the minimal enlargement
    char* pData;

    if ((rc = pfh.GetThisPage(fileHeader.rootPage, ph)))
        return rc;
    if ((rc = ph.GetData(pData)))
        return rc;
    // get node header
    memcpy(&nh, pData, sizeof(struct IX_NodeHeader));
    currPage = fileHeader.rootPage;
    // loop while not leaf node
    while (nh.isTreeNode) {
        // find entry with minimal enlargement
        sn = nh.firstEntry; // get first entry
        for (int i = 0; i < nh.numEntry; i++) {
            memcpy(&e, pData + sn * sizeof(IX_Entry), sizeof(IX_Entry));
            if ((rc = calcaEnlarge(m, e.m, enlarge)))
                return rc;
            if (enlarge < minEnlarge) {
                minEnlarge = enlarge;
                minEnlargeEntry = sn;
            }
            sn = e.nextEntry;
        }
        memcpy(&e, pData + minEnlargeEntry * sizeof(IX_Entry), sizeof(IX_Entry));
        // unpin previous page
        if ((rc = pfh.UnpinPage(currPage)))
            return rc;
        // get the node to search next
        if ((rc = e.child.GetPageNum(currPage)))
            return rc;
        if ((rc = pfh.GetThisPage(currPage, ph)))
            return rc;
        if ((rc = ph.GetData(pData)))
            return rc;
        // get node header
        memcpy(&nh, pData, sizeof(struct IX_NodeHeader));
    }
    page = currPage;
    if ((rc = pfh.UnpinPage(currPage)))
        return rc;
    return rc;
}

// calculate enlargement for outer to encompass inner
RC IX_IndexHandle::calcaEnlarge(const struct MBR& inner, struct MBR& outer, float& enlarge)
{
    struct MBR m;
    m.llx = std::min(inner.llx, outer.llx);
    m.lly = std::min(inner.lly, outer.lly);
    m.urx = std::max(inner.urx, outer.urx);
    m.ury = std::max(inner.ury, outer.ury);
    enlarge = m.area() - outer.area();
    if (enlarge < 0)
        return IX_BADMBRENTRY;
    return 0;
}