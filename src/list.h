#ifndef LIST_INCLUDED
#define	LIST_INCLUDED

typedef struct list_entry {
	struct list_entry *prev;	/*+ Pointer to previous entry + */
	struct list_entry *next;	/*+ Pointer to next entry + */
	void *data;					/*+ Pointer to data item associated with this entry + */
} ListEntry;

typedef struct {
	ListEntry *head;			/*+ Pointer to Entry at Head of List + */
	ListEntry *tail;			/*+ Pointer to Entry at Tail of List + */
	ListEntry *current;			/*+ Pointer to current Entry in List + */
	ListEntry *prev;			/*+ Pointer to next entry following current + */
	ListEntry *next;			/*+ Pointer to next entry following current + */
} List;

List *ListCreate(void);
int ListFree(List * list, void (*func) ());
int ListAddHead(List * list, void *data);
int ListAddTail(List * list, void *data);
List *ListMerge(List * list1, List * list2);
void ListSort(List * list, int (*func) ());
int ListReset(List * list);
int ListTraverse(List * list, void **entry);
int ListTraverseBck(List * list, void **entry);
int ListDeleteEntry(List * list);
int ListInsertBefore(List * list, void *data);
int ListInsertAfter(List * list, void *data);
int ListSwapEntry(List * list);

#endif
