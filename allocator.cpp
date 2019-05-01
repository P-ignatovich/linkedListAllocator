#include <iostream>
#define SIZE 2048

using namespace std;

enum memStatus { NOT_ENOUGH, ENOUGH_TO_SEPARATE, ENOUGH_TO_FILL };

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
    
    node* separateNextNode(size_t reqSize);
    node* fillNextNode();
    void tryMergeWithNext();
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
  }

  for (i = 0; i < 24; i++)
  {
    a->deallocate(reinterpret_cast<uint8_t*>(ptr[i * 2]));
  }
  


  
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
  node* nextNode;
  int status = NOT_ENOUGH;

  currentNode = instance->first;
  nextNode = currentNode->next;

  if(reqSize & 8)
  {
    reqSize = reqSize - reqSize % 8 + 8;
  }

  while(nextNode)
  {
    if(nextNode->size >= reqSize + headerSize + 24)
    {
      status = ENOUGH_TO_SEPARATE;
      break;
    }else if(nextNode->size >= reqSize)
    {
      status = ENOUGH_TO_FILL;
      break;
    }
    currentNode = nextNode;
    nextNode = nextNode->next;
  }

  switch (status)
  {
  case ENOUGH_TO_SEPARATE:
      if(nextNode)
      {
      currentNode = currentNode->separateNextNode(reqSize);
      }
    break;
  
  case ENOUGH_TO_FILL:
      if(nextNode)
      {
      currentNode = currentNode->fillNextNode();
      }
    break;
  
  case NOT_ENOUGH:
    cout <<"NOT ENOUGH SPACE " << endl;
    return nullptr;
    break;
  }

  currentNode++;

  return reinterpret_cast<void*>(currentNode);  
}

void sAllocator::deallocate(uint8_t* ptr)
{
  node* targetNodePtr = reinterpret_cast<node*>(ptr);
  node* prevNodePtr = instance->first;

  targetNodePtr--;

  while(prevNodePtr->next && (prevNodePtr->next < targetNodePtr)) 
  {
    prevNodePtr = prevNodePtr->next;
  }

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
}

void* sAllocator::reallocate(uint8_t* ptr, size_t reqSize)
{
  node* nodePtr = reinterpret_cast<node*>(ptr);
  nodePtr--;
  node* tempNodePtr = reinterpret_cast<node*>(reinterpret_cast<uint8_t*>(nodePtr) + instance->headerSize + nodePtr->size);
  
  if(reqSize > nodePtr->size)
  {
    if(tempNodePtr->isFree && (nodePtr->size + tempNodePtr->size + instance->headerSize > reqSize))
    {
      nodePtr->tryMergeWithNext();
      nodePtr++;
      return reinterpret_cast<void*>(nodePtr);
    }else if(tempNodePtr = reinterpret_cast<node*>(instance->allocate(reqSize)))
    {
      tempNodePtr--;
      
    }
  }
}

void sAllocator::node::tryMergeWithNext()
{
  node* nextNodePtr = reinterpret_cast<node*>(reinterpret_cast<uint8_t*>(this) + instance->headerSize + size);
  
  if(nextNodePtr->isFree)
  {
    size += instance->headerSize + nextNodePtr->size;
    next = nextNodePtr->next;

    instance->numberOfFreeNodes--;
    instance->spaceLeft += instance->headerSize;
  }
}

sAllocator::node* sAllocator::node::separateNextNode(size_t reqSize)
{
  node* temp;

  temp = reinterpret_cast<node*>(reinterpret_cast<uint8_t*>(next) + instance->headerSize + reqSize);
  temp->next = next->next;
  temp->isFree = true;
  temp->size = next->size - reqSize - instance->headerSize;

  next->next = temp;
  next->size = reqSize;
  next->isFree = false;

  temp = next;
  next = temp->next;

  instance->numberOfFilledNodes++;
  instance->spaceLeft -= instance->headerSize + reqSize;

  return temp;
}

sAllocator::node* sAllocator::node::fillNextNode()
{
  node* temp = next;

  next = temp->next;
  temp->isFree = false;

  instance->numberOfFilledNodes++;
  instance->numberOfFreeNodes--;
  instance->spaceLeft -= temp->size;

  return temp;
}
