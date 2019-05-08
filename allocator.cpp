#include <iostream>
#define SIZE 2048

using namespace std;

enum memStatus { NOT_ENOUGH, ENOUGH_TO_CUT, ENOUGH_TO_FILL };

class sAllocator
{
private:
  sAllocator()
  {
    headerSize = sizeof(node);
    first = reinterpret_cast<node*>(mem);

    first->isFree = false;
    first->size = 0;

    first->next = first + 1;
    first->next->next = nullptr;
    first->next->size = SIZE - 2 * headerSize;
    first->next->isFree = true;

    spaceLeft = first->next->size;
    
    numberOfFilledNodes = 0;
    numberOfFreeNodes = 1;
  };

  static sAllocator* instance;

public:

  class node
  {
  public:
    node* next;
    unsigned size;
    bool isFree;
    
    void tryMergeWithNext();
    node* findPrevFreeNode();
    void separateNode(unsigned reqSize);
    void markNodeAsUsed();
  };
  
  alignas(8) uint8_t mem[SIZE];
  unsigned spaceLeft;
  unsigned numberOfFilledNodes;
  unsigned numberOfFreeNodes;
  node* first;
  unsigned headerSize;

  static sAllocator* getInstance();
  void* allocate(size_t reqSize);
  void deallocate(uint8_t* ptr);
  void* reallocate(uint8_t* ptr, size_t reqSize);
  void* cpyMem(uint8_t* src, uint8_t* dest, unsigned length);
};

sAllocator* sAllocator::instance = nullptr;

int main(int argc, char *argv[])
{
  sAllocator* a = sAllocator::getInstance();

  int* ptr[1000];
  int i;


  for(i = 0; i < 48; i++)
  {
    ptr[i] = reinterpret_cast<int*>(a->allocate(16));
    *ptr[i] = i; 
    cout << "space left = " << a->spaceLeft << endl;
  }

  cout << "number of filled nodes = " << a->numberOfFilledNodes << endl;
  cout << "number of free nodes = " << a->numberOfFreeNodes << endl;
  cout << "space left = " << a->spaceLeft << endl;

  // for (i = 0; i < 48; i++)
  // {
  //   cout << *ptr[i] << endl;
  //   a->deallocate(reinterpret_cast<uint8_t*>(ptr[i]));
  //   cout << "space left = " << a->spaceLeft << endl;
  // }
  

  ptr[0] = reinterpret_cast<int*>(a->reallocate(reinterpret_cast<uint8_t*>(ptr[0]), 48));


  
  cout << "number of filled nodes = " << a->numberOfFilledNodes << endl;
  cout << "number of free nodes = " << a->numberOfFreeNodes << endl;
  cout << "space left = " << a->spaceLeft << endl;

}

sAllocator* sAllocator::getInstance()
{
  if(!instance)
  {
    instance = new sAllocator;
  }
  return instance;
}

void* sAllocator::allocate(size_t reqSize)
{
  node* currentNode;
  int status = NOT_ENOUGH;

  currentNode = instance->first;

  if(reqSize & 8)
  {
    reqSize = reqSize - reqSize % 8 + 8;
  }

  while(currentNode->next)
  {
    currentNode = currentNode->next;
    if(currentNode->size >= reqSize + instance->headerSize + sizeof(uint64_t))
    {
      status = ENOUGH_TO_CUT;
      break;
    }else if(currentNode->size >= reqSize)
    {
      status = ENOUGH_TO_FILL;
      break;
    }
  }

  switch (status)
  {
  case ENOUGH_TO_CUT:
      currentNode->separateNode(reqSize);
      currentNode->markNodeAsUsed();
    break;
  
  case ENOUGH_TO_FILL:
      currentNode->markNodeAsUsed();
    break;
  
  case NOT_ENOUGH:
    cout << "cannot allocate: NOT ENOUGH SPACE " << endl;
    return nullptr;
    break;
  }

  currentNode++;

  return reinterpret_cast<void*>(currentNode);  
}

void sAllocator::deallocate(uint8_t* ptr)
{
  node* targetNodePtr = reinterpret_cast<node*>(ptr);
  targetNodePtr--;

  node* prevNodePtr = targetNodePtr->findPrevFreeNode();

  targetNodePtr->isFree = true;
  targetNodePtr->next = prevNodePtr->next;
  prevNodePtr->next = targetNodePtr;

  instance->spaceLeft += targetNodePtr->size;
  instance->numberOfFreeNodes++;
  instance->numberOfFilledNodes--;

  if(targetNodePtr->next)
  {
    targetNodePtr->tryMergeWithNext();
  }

  if(prevNodePtr->next)
  {
    prevNodePtr->tryMergeWithNext();
  }
} //sAllocator::deallocate

void* sAllocator::reallocate(uint8_t* ptr, size_t reqSize)
{
  //nodePtr now points to the node, that contains requseted data
  node* nodePtr = reinterpret_cast<node*>(ptr);
  nodePtr--;

  //tempNodePtr now points to the node next after nodePtr
  node* tempNodePtr = reinterpret_cast<node*>(ptr + nodePtr->size);
  
  if(reqSize > nodePtr->size)
  {
    if(tempNodePtr->isFree && tempNodePtr->size >= reqSize - nodePtr->size + sizeof(uint64_t))
    {
      tempNodePtr->separateNode(reqSize - instance->headerSize);
      tempNodePtr->tryMergeWithNext;
      nodePtr++;
      return reinterpret_cast<void*>(nodePtr);
    }else if(tempNodePtr->isFree && nodePtr->size + tempNodePtr->size + instance->headerSize > reqSize)
    {
      nodePtr->tryMergeWithNext();
      nodePtr++;
      return reinterpret_cast<void*>(nodePtr);
    }else if(tempNodePtr = reinterpret_cast<node*>(instance->allocate(reqSize)))
    {
      nodePtr++;
      instance->cpyMem(reinterpret_cast<uint8_t*>(nodePtr), reinterpret_cast<uint8_t*>(tempNodePtr), (nodePtr - 1)->size);
      instance->deallocate(reinterpret_cast<uint8_t*>(nodePtr));
      return tempNodePtr;
    }else
    {
      cout << "unable to reallocate" << endl;
      return nullptr;
    }
  }else
  {
    nodePtr->separateNode(reqSize);
    nodePtr->next->tryMergeWithNext();
  }
}

void* sAllocator::cpyMem(uint8_t* src, uint8_t* dest, unsigned length)
{
  for(unsigned i = 0; i < length; i++)
  {
    *(dest + i) = *(src + i);
  }
}

void sAllocator::node::tryMergeWithNext()
{
  if(this == instance->first)
  {
    cout << "cannot merge first node" << endl;
    return;
  }

  node* nextNodePtr = reinterpret_cast<node*>(reinterpret_cast<uint8_t*>(this) + instance->headerSize + size);
  
  if(nextNodePtr->isFree)
  {
    this->next = nextNodePtr->next;

    instance->numberOfFreeNodes--;

    if(this->isFree)
    {
      this->size += instance->headerSize + nextNodePtr->size;

      instance->spaceLeft += instance->headerSize;
    }else
    {
      node* prevNodePtr = this->findPrevFreeNode();
      
      prevNodePtr->next = this->next;

      instance->spaceLeft -= nextNodePtr->size;
    }
  }else
  {
    cout << "unable to merge: next node is not empty";
  }
  
} //sAllocator::node::tryMergeWithNext

sAllocator::node* sAllocator::node::findPrevFreeNode()
{
  node* tempNodePtr = instance->first;
  
  while(tempNodePtr->next && tempNodePtr->next < this)
  {
    tempNodePtr = tempNodePtr->next;
  }

  return tempNodePtr;
} //sAllocator::node::findPrevFreeNode

/* separates this node into two nodes
 * argument reqSize is size of first node 
 * second node is always marked as free
 * and properly inserted into list */
void sAllocator::node::separateNode(unsigned reqSize)
{
  if(this->size < reqSize + instance->headerSize)
  {
    cout << "unable to separate: node is not big enough" << endl;
    return;
  }

  node* newNode;

  newNode = reinterpret_cast<node*>(reinterpret_cast<uint8_t*>(this) + instance->headerSize + reqSize);
  newNode->isFree = true;
  newNode->size = this->size - reqSize - instance->headerSize;
  this->size = reqSize;
  this->next = newNode;

  instance->numberOfFreeNodes++;

  if(this->isFree)
  {
    newNode->next = this->next;

    instance->spaceLeft -= instance->headerSize;

  }else
  {
    node* tempNodePtr = this->findPrevFreeNode();
    newNode->next = tempNodePtr->next;
    tempNodePtr->next = newNode;

    instance->spaceLeft += newNode->size;

  }
} //sAllocator::node::separateNode

void sAllocator::node::markNodeAsUsed()
{
  if(!this->isFree)
  {
    cout << "node is already in use" << endl;
    return;
  }

  this->isFree = false;
  this->findPrevFreeNode()->next = this->next;

  instance->numberOfFreeNodes--;
  instance->numberOfFilledNodes++;
  instance->spaceLeft -= this->size;

  return;
} //sAllocator::node::markNodeAsUsed
