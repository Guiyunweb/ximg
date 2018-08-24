#ifndef _XLIST_H
#define _XLIST_H

typedef struct _node
{
    void *data;
    struct _node *next;
} Node;
typedef struct _linkedList
{
    Node *head;
    Node *tail;
    Node *current;
} LinkedList;
typedef void (*OPERATOR)(void *, void *);
typedef void (*DISPLAY)(void *);
typedef int (*COMPARE)(void *, void *);

void initializeList(LinkedList *);                       // 初始化链表
void addHead(LinkedList *, void *);                      // 给链表的头节点添加数据
void addTail(LinkedList *, void *);                      // 给链表的尾节点添加数据
void deleteNode(LinkedList *, Node *);                   // 从链表删除节点
Node *getNode(LinkedList *, COMPARE, void *);            // 返回包含指定数据的节点指针
void displayLinkedList(LinkedList *, DISPLAY);           // 打印链表
void operatorLinkedList(LinkedList *, OPERATOR, void *); // 操作链表
void freeLinkedList(LinkedList *list);                   //释放空间
#endif