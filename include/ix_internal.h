//
// Created by Yue Wang on 3/18/17.
//
#include "ix.h"
#ifndef REDBASE_IX_INTERNAL_H
#define REDBASE_IX_INTERNAL_H

#endif //REDBASE_IX_INTERNAL_H
#define NO_MORE_FREE_PAGES -1
#define NO_NEXT_SLOT -2
#define NO_NEXT_ENTRY -3
#define NODE_TO_INSERT -4

struct IX_NodeHeader {
    bool ifUsed;              // whether this page is free or used
    PageNum nextFreePage;     // if isUsed is false, this variable stores next free page
    bool isTreeNode;          // whether this node is a tree node or a leaf node
    bool isRoot;              // whether this node is root or not
    int numEntry;             // number of entries in this node
    SlotNum firstEntry;       // pointer to the first entry in this node
    SlotNum firstFreeSlot;       // pointer to the first entry in this node
    RID parent;       // pointer to the parent of this node
    struct MBR m;
};

struct IX_Entry {
    bool ifUsed;               // whether this entry is free or used
    SlotNum nextEntry;         // if used, point to next entry; if free, point to next free slot
    struct MBR m; // the bounding box
    RID child; // for tree node, point to child node; for leaf node, point to the actual record
};