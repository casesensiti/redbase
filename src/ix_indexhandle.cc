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
    PageNum




    // insert into node

    // if too many, split

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