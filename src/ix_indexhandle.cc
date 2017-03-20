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
#include <queue>

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
    SlotNum dummySlot;
    if ((rc = ChooseLeaf(m, toInsert)))
        return rc;
    // insert into node
    PF_PageHandle ph;
    char *pageData;
    IX_NodeHeader *pnh; // pointer to node header
    if ((rc = pfh.GetThisPage(toInsert, ph))) // get page info
        return rc;
    if ((rc = ph.GetData(pageData))) // get node header info
        return rc;
    pnh = (struct IX_NodeHeader *) pageData;
#ifdef MY_DEBUG
    printf("\tInsert into page %d\n", toInsert);
#endif
    // if have slot, insert to this node
    if (pnh->numEntry < fileHeader.M)
    {
        if ((rc = InsertToNode(pageData, pData, rid, dummySlot)))
            return rc;
        if ((rc = pfh.MarkDirty(toInsert)) || (rc = pfh.UnpinPage(toInsert)))
            return rc;
        ForcePages();
        return rc;
    }
    // if too many, split
    if ((rc = pfh.UnpinPage(toInsert)))
        return rc;
    RID dummyRID(0,0);
    //if((rc = SplitNode(pageData, npageData, pData, rid, dummyRID))) // This time returned RID is not used
      //  return rc;




    return rc;
}

// oData is the data of original node, nData is the data of new node. pData point to data to be inserted
// rid is the RID of data inserted with pData, insertedPos is the position of inserted entry
// newPage is the page which stores the new node
RC IX_IndexHandle::SplitNode(PageNum page, void* pData, const RID& rid, RID& insertedPos, PageNum newPage)
{
    RC rc = 0;
    PF_PageHandle ph, nph;
    char *pageData, *npageData;
    IX_NodeHeader *pnh, *npnh, *opnh; // pointer to node header
    IX_Entry *e;
    SlotNum sn, s1, s2, insertedSlot, dummySlot;
    char* oData[PF_PAGE_SIZE];
    struct MBR* pm = (struct MBR*) pData;

    // open page to be splitted
    if ((rc = pfh.GetThisPage(page, ph)) || (rc = ph.GetData(pageData)))
        return rc;
    pnh = (IX_NodeHeader *) pageData;
    // get a free page to save new node
    if (fileHeader.numFreePage == 0)
    {
        if ((rc = pfh.AllocatePage(nph)))
            return rc;
        if ((rc = nph.GetData(npageData)) || (rc = nph.GetPageNum(newPage)))
            return rc;
        npnh = (IX_NodeHeader *) npageData;
        fileHeader.numPage++;
    } else {
        newPage = fileHeader.firstFreePage;
        if ((rc = pfh.GetThisPage(newPage, nph)))
            return rc;
        if ((rc = nph.GetData(npageData)))
            return rc;
        npnh = (IX_NodeHeader *) npageData;
        fileHeader.numFreePage--;
        fileHeader.firstFreePage = npnh->nextFreePage;
    }
    headerModified = true;
    npnh->ifUsed = true;
    npnh->firstEntry = NO_NEXT_ENTRY;
    npnh->isRoot = false;
    npnh->isTreeNode = pnh->isTreeNode;
    npnh->firstFreeSlot = NO_NEXT_SLOT;
    npnh->numEntry = 0;
    npnh->parent = RID(0,0); // RID to be determined
    if (pnh->numEntry != fileHeader.M)
        return IX_INVALIDNODETOSPLIT;
    // distribute entries.
    // copy original data, empty original node
    memcpy(oData, pageData, PF_PAGE_SIZE);
    opnh = (IX_NodeHeader *)oData;
    pnh->ifUsed = true;
    pnh->firstEntry = NO_NEXT_ENTRY;
    pnh->firstFreeSlot = NO_NEXT_SLOT;
    pnh->numEntry = 0;
    // calculate max/min llx, lly, urx, ury
    float max[4], min[4];
    SlotNum maxS[4], minS[4];
    max[0] = min[0] = pm->llx;
    max[1] = min[1] = pm->lly;
    max[2] = min[2] = pm->urx;
    max[3] = min[3] = pm->ury;
    for (int i = 0; i < 4; i++)
    {
        maxS[i] = minS[i] = NODE_TO_INSERT;
    }
    // traverse entries of the node
    sn = opnh->firstEntry;
    for (int i = 0; i < opnh->numEntry; i++)
    {
#ifdef MY_DEBUG
        if(sn == NO_NEXT_ENTRY) printf("Not valid slot number in splitnode\n");
#endif
        e = (IX_Entry *) (oData + sizeof(IX_NodeHeader) + sn * sizeof(IX_Entry));
#ifdef MY_DEBUG
        if(!e->ifUsed) printf("Not valid entry in splitnode\n");
#endif
        if (max[0] < e->m.llx) {max[0] = e->m.llx; maxS[0] = sn;}
        if (max[1] < e->m.lly) {max[1] = e->m.lly; maxS[1] = sn;}
        if (max[2] < e->m.urx) {max[2] = e->m.urx; maxS[2] = sn;}
        if (max[3] < e->m.ury) {max[3] = e->m.ury; maxS[3] = sn;}
        if (min[0] > e->m.llx) {min[0] = e->m.llx; minS[0] = sn;}
        if (min[1] > e->m.lly) {min[1] = e->m.lly; minS[1] = sn;}
        if (min[2] > e->m.urx) {min[2] = e->m.urx; minS[2] = sn;}
        if (min[3] > e->m.ury) {min[3] = e->m.ury; minS[3] = sn;}
        sn = e->nextEntry;
    }
    // find two with most separation
    float x_dis = (max[0] - min[2]) / (max[2] - min[0]); // (max llx - min urx) / (max urx - min llx)
    float y_dis = (max[1] - min[3]) / (max[3] - min[1]);
    if (x_dis > y_dis && maxS[0] != minS[2]) {s1 = maxS[0]; s2 = minS[2];}
    else if(maxS[1] != minS[3]) {s1 = maxS[1]; s2 = minS[3];}
    else printf("Can not find suitable seeds in splitnode\n");
    // insert seeds into nodes
    if (s1 == NODE_TO_INSERT) {
        if ((rc = InsertToNode(pageData, pData, rid, insertedSlot)))
            return rc;
        insertedPos = RID(page, insertedSlot);
    } else {
        e = (IX_Entry *) (oData + sizeof(IX_NodeHeader) + s1 * sizeof(IX_Entry));
        InsertToNode(pageData, &(e->m), e->child, dummySlot);
    }

    if (s2 == NODE_TO_INSERT) {
        if ((rc = InsertToNode(npageData, pData, rid, insertedSlot)))
            return rc;
        insertedPos = RID(page, insertedSlot);
    } else {
        e = (IX_Entry *) (oData + sizeof(IX_NodeHeader) + s2 * sizeof(IX_Entry));
        InsertToNode(npageData, &(e->m), e->child, dummySlot);
    }
    // if new entry is not inserted, insert it.
    if (s1 != NODE_TO_INSERT && s2 != NODE_TO_INSERT)
    {
        if (calcaEnlarge(*pm, pnh->m) > calcaEnlarge(*pm, npnh->m)) {
            if ((rc = InsertToNode(npageData, pData, rid, insertedSlot)))
                return rc;
            insertedPos = RID(newPage, insertedSlot);
        }
        else {
            if ((rc = InsertToNode(pageData, pData, rid, insertedSlot)))
                return rc;
            insertedPos = RID(page, insertedSlot);
        }

    }
    // traverse original data, insert into new data
    sn = opnh->firstEntry;
    for (int i = 0; i < opnh->numEntry; i++) {
        e = (IX_Entry *) (oData + sizeof(IX_NodeHeader) + sn * sizeof(IX_Entry));
        sn = e->nextEntry;
        // if has been inserted, skip.
        if (sn == s1 || sn == s2) continue;
        // if the first node is full, insert into the second node
        if (pnh->numEntry == fileHeader.M) {
            if ((rc = InsertToNode(npageData, &(e->m), e->child, dummySlot)))
                return rc;
        } else if (npnh->numEntry == fileHeader.M) {
            if ((rc = InsertToNode(pageData, &(e->m), e->child, dummySlot)))
                return rc;
        } else if (calcaEnlarge(e->m, pnh->m) > calcaEnlarge(e->m, npnh->m)){
            if ((rc = InsertToNode(npageData, &(e->m), e->child, dummySlot)))
                return rc;
        } else {
            if ((rc = InsertToNode(pageData, &(e->m), e->child, dummySlot)))
                return rc;
        }
    }
    // clean up
    if ((rc = pfh.MarkDirty(page)) || (rc = pfh.MarkDirty(newPage)) ||
            (rc = pfh.UnpinPage(page)) || (rc = pfh.UnpinPage(newPage)))
        return rc;
    return rc;
}

/*
RC IX_IndexHandle::DeleteFromNode()
{
    RC rc = 0;
    return rc;
}
*/

// insert MBR data pointed by pData, with RID& rid into Node pointed by pageData
// maintain NodeHeader information
RC IX_IndexHandle::InsertToNode(void* pageData, void* pData, const RID &rid, SlotNum& insertedSlot)
{
    RC rc = 0;
    IX_NodeHeader* pnh = (struct IX_NodeHeader *)pageData;
    IX_Entry e;
    IX_Entry *pEntry;
    SlotNum free; // free slot to put new entry
    e.child = rid;
    e.ifUsed = true;
    e.m = *(struct MBR *)pData;

    if (pnh->numEntry >= fileHeader.M)
        return IX_NODEFULL;
    // if have free slot, insert into free slot. Otherwise, insert into numEntry+1 slot
    if (pnh->firstFreeSlot != NO_NEXT_SLOT)
    {
        free = pnh->firstFreeSlot;
        pEntry = (IX_Entry *) (pageData + sizeof(IX_NodeHeader) + free * sizeof(IX_Entry));
        if (pEntry->ifUsed)
            return IX_BADENTRY;
        pnh->firstFreeSlot = pEntry->nextEntry;
    } else {
        free = pnh->numEntry + 1;
    }
    insertedSlot = free;
    // put new entry into pagaData
    e.nextEntry = pnh->firstEntry;
    pnh->firstEntry = free;
    memcpy(pageData + sizeof(IX_NodeHeader) + free * sizeof(IX_Entry), &e, sizeof(IX_Entry));
    // if first entry inserted
    if (pnh->numEntry == 0) pnh->m = e.m;
    // otherwise
    else ExpandMBR(e.m, pnh->m);
    headerModified = true;
    pnh->numEntry = pnh->numEntry + 1;
    return rc;
}

RC IX_IndexHandle::ExpandMBR(struct MBR &inner, struct MBR &outer)
{
    outer.llx = std::min(inner.llx, outer.llx);
    outer.lly = std::min(inner.lly, outer.lly);
    outer.urx = std::max(inner.urx, outer.urx);
    outer.ury = std::max(inner.ury, outer.ury);
}

RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
  // Implement this
}

RC IX_IndexHandle::ForcePages()
{
  // Implement this
    pfh.ForcePages();
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
            memcpy(&e, pData + sizeof(IX_NodeHeader) + sn * sizeof(IX_Entry), sizeof(IX_Entry));
            enlarge = calcaEnlarge(m, e.m);
            if (enlarge < minEnlarge) {
                minEnlarge = enlarge;
                minEnlargeEntry = sn;
            }
            sn = e.nextEntry;
        }
        memcpy(&e, pData + + sizeof(IX_NodeHeader) + minEnlargeEntry * sizeof(IX_Entry), sizeof(IX_Entry));
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
float IX_IndexHandle::calcaEnlarge(const struct MBR& inner, struct MBR& outer)
{
    struct MBR m;
    m.llx = std::min(inner.llx, outer.llx);
    m.lly = std::min(inner.lly, outer.lly);
    m.urx = std::max(inner.urx, outer.urx);
    m.ury = std::max(inner.ury, outer.ury);
    return m.area() - outer.area();
}

// Print this index
RC IX_IndexHandle::Print()
{
    RC rc = 0;
#ifdef MY_DEBUG
    printf("IX_IndexHandle::Print() invoked\n");
#endif
    printf("Index information:\nM: %d, m: %d, rootPage: %d\n", fileHeader.M, fileHeader.m, fileHeader.rootPage);
    printf("Node list\n------------------------------------------------------\n");
    std::queue<PageNum> vp;
    vp.push(fileHeader.rootPage);
    PageNum curr, tmpP;
    SlotNum sn, tmpS;
    PF_PageHandle ph;
    char* pData;
    IX_NodeHeader* pnh;
    IX_Entry* pe;
    // while there is node not printed
    while(vp.size() > 0) {
        // get the page number of the node
        curr = vp.front();
        vp.pop();
        // get node and its data
        if ((rc = pfh.GetThisPage(curr, ph)))
            return rc;
        if ((rc = ph.GetData(pData)))
            return rc;
        // print node header information
        pnh = (IX_NodeHeader *)pData;
        printf("Node/Page %d, numEntry: %d, NodeType: ", curr, pnh->numEntry);
        if (pnh->isTreeNode)
            printf("TreeNode");
        else printf("LeafNode");
        if((rc = pnh->parent.GetPageNum(tmpP) || (rc = pnh->parent.GetSlotNum(tmpS))))
            return rc;
        printf(", parent: %d.%d, MBR: ", tmpP, tmpS);
        pnh->m.print();
        printf("\n");
        // print entries in this node
        sn = pnh->firstEntry;
        for (int i = 0; i < pnh->numEntry; i++) {
            pe = (IX_Entry *)(pData + sizeof(IX_NodeHeader) + sn * sizeof(IX_Entry));
            printf("\tEntry %d. MBR: ", sn);
            pe->m.print();
            if((rc = pe->child.GetPageNum(tmpP) || (rc = pe->child.GetSlotNum(tmpS))))
                return rc;
            printf(". Child: %d.%d\n", tmpP, tmpS);
            if (pnh->isTreeNode) vp.push(tmpP);
            sn = pe->nextEntry;
        }
        if((rc = pfh.UnpinPage(curr)))
            return rc;
    }
    return rc;
}