#include "quadtree.h"


// simple function to find the mid point between two values

static inline float halfway(float min, float max)
{
  return min + ((max-min)/2.0);
}

static inline bool pointinside(float x, float y, Extent *extent)
{
  return ( (x > extent->xmin) && (x < extent->xmax) && 
           (y > extent->ymin) && (y < extent->ymax) );
}


// I use some macros where following different
// corners happens.  Removes a lot of redundant
// code where using a function would be a little
// heavy handed.  I'm trying to keep the macros
// readable...

#define DO_CORNERS(x, y, fn, node)          \
  if(x < xmid) {                            \
    if(y < ymid) {                          \
      fn(node->ul);                         \
    } else {                                \
      fn(node->ll);                         \
    }                                       \
  } else {                                  \
    if(y < ymid) {                          \
      fn(node->ur);                         \
    } else {                                \
      fn(node->lr);                         \
    }                                       \
  }

#define MOVE_CORNER(to)                     \
  LeafData *next = cur->next;               \
  LeafData *tmp  = to.contents.payload;     \
  to.contents.payload = cur;                \
  cur->next = tmp;                          \
  cur = next;                               \
  to.size++;

#define BUILD_EXTENTS(xmin,ymin,xmax,ymax)  \
  (struct Extent){xmin,ymin,xmax,ymax}      


// called when a leaf has grown too big.  Creates
// a new Qnode (which branches off to 4 new leaves...)
// and places it under the current leaf.

void leafpushdown(Leaf *leaf, unsigned int depth)
{
  float xmid = halfway(leaf->extents.xmin, leaf->extents.xmax);
  float ymid = halfway(leaf->extents.ymin, leaf->extents.ymax);

  Qnode *newnode = calloc(1, sizeof(Qnode));
  if(newnode) {
    LeafData *cur = leaf->contents.payload;
    
    newnode->depth = depth;
    
    newnode->ul.extents = BUILD_EXTENTS(leaf->extents.xmin, leaf->extents.ymin, 
                                        xmid, ymid);
    
    newnode->ur.extents = BUILD_EXTENTS(xmid, leaf->extents.ymin, 
                                        leaf->extents.xmax, ymid);
    
    newnode->ll.extents = BUILD_EXTENTS(leaf->extents.xmin, ymid, 
                                        xmid, leaf->extents.ymax);
    
    newnode->lr.extents = BUILD_EXTENTS(xmid, ymid, 
                                        leaf->extents.xmax, leaf->extents.ymax);
    while(cur) {
      DO_CORNERS(cur->x, cur->y, MOVE_CORNER, newnode);
    }
    leaf->size = 0;
    leaf->contents.leaf = newnode;
  }
}


// build a new tree.
// maxsize  = maximum number of elements before a new level is made.
// maxdepth = maximum number of depth levels allowable.
//
// if maxdepth is reached, maxsize is ignored at that level

void newtree(QuadTree *qt, 
             unsigned int maxsize,
             unsigned int maxdepth,
             Extent extents)
{
  float xmid = halfway(extents.xmin,extents.xmax); 
  float ymid = halfway(extents.ymin,extents.ymax);
  
  qt->head     = NULL;
  qt->maxsize  = maxsize;
  qt->maxdepth = maxdepth;
  qt->extents  = extents;
    
  qt->head = calloc(1,sizeof(Qnode));
 
  qt->head->depth = 1;

  qt->head->ul.extents = BUILD_EXTENTS(qt->extents.xmin, 
                                      qt->extents.ymin, 
                                      xmid, 
                                      ymid);
  qt->head->ur.extents = BUILD_EXTENTS(xmid, 
                                      qt->extents.ymin, 
                                      qt->extents.xmax, 
                                      ymid);
  qt->head->ll.extents = BUILD_EXTENTS(qt->extents.xmin, 
                                      ymid, 
                                      xmid, 
                                      qt->extents.ymax);
  qt->head->lr.extents = BUILD_EXTENTS(xmid, 
                                      ymid, 
                                      qt->extents.xmax, 
                                      qt->extents.ymax);
  if (!qt->head) {     // calloc has thrown a hizzy!
    return;
  }
  
}


// add new data element to the current leaf

LeafData *addleafdata(QuadTree *qt, Qnode *curnode, Leaf *leaf, 
                      float x, float y, 
                      LeafData *newnode)
{
  newnode->next          = leaf->contents.payload;
  leaf->contents.payload = newnode;  
   
  leaf->size++;
  if((leaf->size >= qt->maxsize)&&(curnode->depth < qt->maxdepth)) {
    leafpushdown(leaf, curnode->depth+1);
  }
  return newnode;
}

#define FIND_CORNER(curcorner)                                \
if((curcorner.size==0)&&(curcorner.contents.leaf)) {          \
  return(findleafx(curcorner.contents.leaf,x,y));     \
} else {                                                      \
  return(&curcorner);                                         \
}


// recursively descends, attempting to find the leaf containing
// the given (x,y) coordinate.

Leaf *findleafx(Qnode *cur, float x, float y)
{
  float xmid = cur->ul.extents.xmax;
  float ymid = cur->ul.extents.ymax;
  
  DO_CORNERS(x, y, FIND_CORNER, cur);
}


// sets up a call to findleafx

Leaf *findleaf(QuadTree *qt, float x, float y)
{
  if(!(qt->head)){
    return NULL;
  }
  return findleafx(qt->head, x, y);
}

#define ADD_CORNER(curcorner)                                                 \
if((curcorner.size==0)&&(curcorner.contents.leaf)) {                          \
  addpointx(qt,curcorner.contents.leaf,x,y,newnode);                          \
} else {                                                                      \
  addleafdata(qt, cur, &curcorner, x, y, newnode);                            \
}                                                                       


// recurse down, find the right leaf to put new data into

Qnode *addpointx(QuadTree * qt, Qnode *cur,
                 float x, float y, 
                 LeafData *newnode)
{
  float xmid = cur->ul.extents.xmax;
  float ymid = cur->ul.extents.ymax;

  DO_CORNERS(x, y, ADD_CORNER, cur);
  return cur;
}


// set up a call to addpointx

bool addpoint(QuadTree *qt, 
              float x, float y, 
              void *data)
{
  LeafData *newnode = malloc(sizeof(LeafData));
  
  if (!(newnode)){
    return false;
  }

  newnode->data     = data;
  newnode->x        = x;
  newnode->y        = y;

  qt->head = addpointx(qt, qt->head,
                       x, y,
                       newnode);
  return true;
}

bool movepoint(QuadTree *qt, 
              float oldx, float oldy, 
              float newx, float newy, 
              void *data)
{
  Leaf *oldleaf = findleaf(qt, oldx, oldy);
  if (oldleaf->size == 0) {
    printf ("QuadTree -- point to move is not in Leaf\n");
    return false;
  }
  

  LeafData *cur  = oldleaf->contents.payload;
  LeafData *prev = NULL;
  while(cur) {
    if((data == cur->data)&&(oldx==cur->x)&&(oldy==cur->y)) {
      cur->x = newx;
      cur->y = newy;
      if(!(pointinside(newx, newy, &oldleaf->extents))) {
        if(prev){
          prev->next = cur->next;
        } else {
          oldleaf->contents.payload = cur->next;
        }
        oldleaf->size -= 1;
        addpointx(qt, qt->head,
                  newx, newy,
                  cur);
        return true;
      }
    }
    prev = cur;
    cur  = cur->next;
  } 
  return false;
}
    
#define PRINT_CORNER(curnode, curcorner)                        \
  printf("leaf (%f,%f) (%f,%f)\n",                              \
         curcorner.extents.xmin,                                \
         curcorner.extents.ymin,                                \
         curcorner.extents.xmax,                                \
         curcorner.extents.ymax);                               \
  if((curcorner.contents.leaf)&&                                \
     (curcorner.size==0)) {                                     \
    printf("descending\n");                                     \
    listpointsx(curcorner.contents.leaf);                       \
  } else if(curcorner.contents.payload) {                       \
    printf("contents:\n");                                      \
    LeafData *curdata = curcorner.contents.payload;             \
    while(curdata){                                             \
      printf("(%4.2f,%4.2f) - %p\n",                            \
             curdata->x,                                        \
             curdata->y,                                        \
             curdata->data);                                    \
      curdata = curdata->next;                                  \
    }                                                           \
  } else {                                                      \
    printf("empty\n");                                          \
  }                                                 

// prints out a description of the tree.  Not very pretty,
// but it works.

void listpointsx(Qnode *cur)
{
  PRINT_CORNER(cur,cur->ul);
  PRINT_CORNER(cur,cur->ur);
  PRINT_CORNER(cur,cur->ll);
  PRINT_CORNER(cur,cur->lr);
}


// set up a call to listpointsx

void listpoints(QuadTree *qt)
{
  listpointsx(qt->head);
}


// function to test if two extents overlap
// (bounding box overlap check)

bool overlap (Extent ext1, Extent ext2)
{
  return (!(ext1.xmin > ext2.xmax ||
            ext2.xmin > ext1.xmax ||
            ext1.ymin > ext2.ymax ||
            ext2.ymin > ext1.ymax));
}


// a function to test the overlap function...

void testoverlap(void)
{
  assert(!overlap((struct Extent){10,10,20,20}, (struct Extent){30,30,40,40}));
  assert(!overlap((struct Extent){30,30,40,40}, (struct Extent){10,10,20,20}));
  assert( overlap((struct Extent){10,10,20,20}, (struct Extent){0,0,40,40}  ));
  assert( overlap((struct Extent){0,0,40,40},   (struct Extent){10,10,20,20}));
}



float getdistance(float x1,float y1,float x2,float y2) {
  return sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2));
}



#define FIND_IN_CORNER(curleaf)                                           \
  if(overlap(curleaf.extents,                                             \
             (struct Extent){x-radius,y-radius,                           \
                             x+radius,y+radius})) {                       \
    if((curleaf.contents.leaf)&&(curleaf.size == 0)) {                    \
      maptonearbyx(curleaf.contents.leaf, visitor, arg,                   \
                   curleaf.extents,                                       \
                   x, y, radius);                                         \
    } else if ((curleaf.contents.payload)&&(curleaf.size > 0)) {          \
      LeafData *cur = curleaf.contents.payload;                           \
      while(cur) {                                                        \
        if(getdistance(x,y,cur->x,cur->y) <= radius) {                    \
          visitor(cur, arg);                                              \
        }                                                                 \
        cur = cur->next;                                                  \
      }                                                                   \
    }                                                                     \
  } 

void printlocation(LeafData *cur, void *arg)
{
  printf ("found -- (%f,%f)\n",cur->x,cur->y);                 
}


// maps a function to elements closer to (x,y) than the given
// radius.

void maptonearbyx(Qnode *node, LeafCallback visitor, void *arg,
                 Extent extents,
                 float x, float y, float radius)
{
  FIND_IN_CORNER(node->ul);
  FIND_IN_CORNER(node->ur);
  FIND_IN_CORNER(node->ll);
  FIND_IN_CORNER(node->lr);
}


// prints out elements near (x,y).  probably should remove,
// not very useful.

void findnearby(QuadTree *qt, float x, float y, float radius)
{
  maptonearbyx(qt->head, &printlocation, NULL, qt->extents, x, y, radius);  
}


// set up a call to maptonearbyx

void maptonearby(QuadTree *qt, LeafCallback visitor, void *arg,
                 float x, float y, float radius)
{
  maptonearbyx(qt->head, visitor, arg, qt->extents, x, y, radius);  
}


// function to remove a data element from the quad tree

bool deletepoint(QuadTree *qt,
                 float x, float y,
                 void *data)
{
  Leaf *leaf = findleaf(qt, x, y);
  if(leaf->size){
    LeafData *cur = leaf->contents.payload;
    LeafData *prev = NULL;
    while(cur){
      if(cur->data == data){
        if(prev){
          prev->next = cur->next;
        } else {
          leaf->contents.payload = cur->next;
        }
        free(cur);
        leaf->size -= 1;
        return true;
      }
      prev = cur;
      cur  = cur->next;
    }
  }
  return false;
}  

void deleteqnode(Qnode *node, LeafCallback visitor)
{
  deleteleaf(&node->ul,visitor);
  deleteleaf(&node->ur,visitor);
  deleteleaf(&node->ll,visitor);
  deleteleaf(&node->lr,visitor);
  free(node);
}

void deleteleaf(Leaf *leaf, LeafCallback visitor)
{
  if(leaf->size) {
    LeafData *cur = leaf->contents.payload; 
    while(cur){
      LeafData *next = cur->next;
      if(visitor){
        visitor(cur, NULL);
      }
      free(cur);
      cur = next;
    }
  } else if(leaf->contents.leaf){
    deleteqnode(leaf->contents.leaf,visitor);
  }
    
}

void deletetree(QuadTree *qt, LeafCallback visitor)
{
  deleteqnode(qt->head, visitor);
  qt->head = NULL;
}




