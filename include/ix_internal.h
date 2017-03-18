//
// Created by Yue Wang on 3/18/17.
//
#include "ix.h"
#ifndef REDBASE_IX_INTERNAL_H
#define REDBASE_IX_INTERNAL_H

#endif //REDBASE_IX_INTERNAL_H

enum NodeType {
    RootNode,
    TreeNode,
    LeafNode
};


struct IX_NodeHeader {
    bool ifUsed;              // whether this page is free or used
    PageNum nextFreePage;     // if isUsed is false, this variable stores next free page
    NodeType ntype;          // whether this node is a tree node or a leaf node
    int numEntry;             // number of entries in this node
    SlotNum firstEntry;       // pointer to the first entry in this node
    PageNum parentPage;       // pointer to the parent of this node
};

struct IX_Entry {
    int ifUsed;               // whether this entry is free or used
    SlotNum nextEntry;        // if used, point to next entry; if free, point to next free slot
    struct MBR m; // the bounding box
    RID child; // for tree node, point to child node; for leaf node, point to the actual record
};