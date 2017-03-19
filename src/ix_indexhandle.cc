//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle handles manipulations within the index
// Author:      <Your Name Here>
//

#include <unistd.h>
#include <sys/types.h>
#include "ix.h"
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
#endif


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
