/*
 * list.c - routines to implement a linked list
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2003 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "list.h"

#define	FALSE 0
#define	TRUE 1

/*+
   Creates an empty List

   List *ListCreate Returns an empty list.
   + */

List *ListCreate(void)
{
	List *list;

	list = (List *) malloc(sizeof(List));
	if (list) {
		list->head = NULL;
		list->tail = NULL;
		list->current = NULL;
		list->next = NULL;
	}
	return list;
}

/*+
   Releases all storages allocated by the supplied List. The data contained
   in each node should be released by the function specified in the
   argument list.

   int ListFree returns TRUE on success and FALSE on failure.

   List *list The List to deallocate.

   void (*func)() The function that deallocates the data. The deallocation
   function is called with a single argument (the data). The free() function
   can be specified if the data is a pointer to an array of characters. A
   NULL value can be supplied if a deallocation function is not necessary.
   + */

int ListFree(List * list, void (*func) ())
{
	struct list_entry *temp;
	struct list_entry *next;

	int ok = FALSE;

	if (list) {
		for (temp = list->head; temp; temp = next) {
			next = temp->next;
			if (func)
				(*func) (temp->data);
			free(temp);
		}

		free(list);

		ok = TRUE;
	}
	return ok;
}

/*+
   Adds the supplied data item to Head of List.

   int ListAddHead Returns TRUE on success and FALSE on failure.

   List *list The list that the data item is to be added to.

   void *data Data item that is added to the list. This can be a pointer to
   anything, and with care a simple integer value.
   + */

int ListAddHead(List * list, void *data)
{
	struct list_entry *temp;

	int ok = FALSE;

	if (list) {
		temp = (struct list_entry *) malloc(sizeof(struct list_entry));
		if (temp) {
			temp->prev = NULL;
			temp->next = list->head;
			temp->data = data;

			if (list->head) {
				list->head->prev = temp;
				list->head = temp;
			}
			else {
				list->head = list->tail = temp;
			}

			ok = TRUE;
		}
	}
	return ok;
}

/*+
   Adds the supplied data item to Tail of List.

   int ListAddTail Returns TRUE on success and FALSE on failure.

   List *list The list that the data item is to be added to.

   void *data Data item that is added to the list. This can be a pointer to
   anything, and with care a simple integer value.
   + */

int ListAddTail(List * list, void *data)
{
	struct list_entry *temp;

	int ok = FALSE;

	if (list) {
		temp = (struct list_entry *) malloc(sizeof(struct list_entry));
		if (temp) {
			temp->prev = list->tail;
			temp->next = NULL;
			temp->data = data;

			if (list->tail) {
				list->tail->next = temp;
				list->tail = temp;
			}
			else {
				list->head = list->tail = temp;
			}

			ok = TRUE;
		}
	}
	return ok;
}

/*+
   Merges list1 and list2. This is performed by appending list2 onto
   list1. On return from this routine, list2 will have been de-allocated
   and the return value will be the same as list1.

   List *ListMerge list1 with list2 appended.

   List *list1 This is the primary list.

   List *list2 This is list that will be appended to the primary list.
   + */

List *ListMerge(List * list1, List * list2)
{
	if (!list1->head) {
		list1->head = list2->head;
		list1->tail = list2->tail;
	}
	else if (list2->head) {
		list1->tail->next = list2->head;
		list2->head->prev = list1->tail;
		list1->tail = list2->tail;
	}
	list2->head = NULL;
	list2->tail = NULL;

	ListFree(list2, NULL);

	return list1;
}

/*+
   ListSort() is called to sort a list into order.

   List *list List that is to be sorted.

   int (*func)() Pointer to a function that compares two entries in the list.
   The function should return -1 if entry1 < entry2, 0 if entry1 = entry2
   and +1 if entry1 > entry2.
   + */

void ListSort(List * list, int (*func) ())
{
	struct list_entry *next;
	struct list_entry *temp;

	void *data;
	int done;
	int result;

	if (list->head) {
		done = FALSE;

		while (!done) {
			done = TRUE;

			for (temp = list->head, next = temp->next; next; temp = next, next = temp->next) {
				result = (*func) (temp->data, next->data);
				if (result >= 1) {
					data = temp->data;
					temp->data = next->data;
					next->data = data;

					done = FALSE;
				}
			}
		}
	}
}

/*+
   Resets current node back to head node.

   int ListReset returns TRUE on success and FALSE on failure.

   List *list The list to be reset.
   + */

int ListReset(List * list)
{
	int status = FALSE;

	if (list) {
		list->current = NULL;
		list->prev = list->tail;
		list->next = list->head;
		status = TRUE;
	}
	return status;
}

/*+
   This routine is used to traverse a list. Each time it is called the
   next item in the list is returned. If ListReset() is called beforehand
   then ListTraverse() returns the head node.

   int ListTraverse Returns TRUE indicates that the next entry has been
   returned. FALSE indicates that the end of the list has been
   reached and no entry has been returned.

   List *list The list to be traversed.

   void **entry This is an output variable and is used to store the address
   of the next data item in the list.
   + */

int ListTraverse(List * list, void **entry /*+ Current Entry + */ )
{
	struct list_entry *next;

	int status;

	next = list->next;
	if (next) {
		*entry = next->data;
		list->current = next;
		list->prev = next->prev;
		list->next = next->next;
		status = TRUE;
	}
	else {
		status = FALSE;
	}

	return status;
}

/*+
   This routine is used to traverse a list. Each time it is called the
   previous item in the list is returned. If ListReset() is called
   beforehand then ListTraverseBck() returns the tail node.

   int ListTraverseBck Returns TRUE indicates that the previous entry has
   been returned. FALSE indicates that the begining of the list has been
   reached and no entry has been returned.

   List *list The list to be traversed.

   void **entry This is an output variable and is used to store the address
   of the previous data item in the list.
   + */

int ListTraverseBck(List * list, void **entry /*+ Current Entry + */ )
{
	struct list_entry *prev;

	int status;

	prev = list->prev;
	if (prev) {
		*entry = prev->data;
		list->current = prev;
		list->prev = prev->prev;
		list->next = prev->next;
		status = TRUE;
	}
	else {
		status = FALSE;
	}

	return status;
}

/*
   =====================================================
   Routines to be called from within a ListTraverse Loop
   =====================================================
 */

/*+
   Deletes the current entry in the supplied linked list. Used in
   conjunction with ListTraverse().

   int ListDeleteEntry Returns TRUE on success and FALSE on failure.

   List *list The current entry in this list will be deleted.
   + */

int ListDeleteEntry(List * list)
{
	int status = FALSE;

	if (list) {
		struct list_entry *current;

		current = list->current;
		if (current) {
			struct list_entry *prev;
			struct list_entry *next;

			prev = current->prev;
			next = current->next;

			free(current);

			if (prev)
				prev->next = next;
			else
				list->head = next;

			if (next)
				next->prev = prev;
			else
				list->tail = prev;

			status = TRUE;
		}
	}
	return status;
}

/*+
   Inserts the supplied data item just before the current entry in the
   supplied list. Used in conjunction with ListTraverse().

   int ListInsertBefore Returns TRUE on success and FALSE on failure.

   List *list The data item will be inserted before the current entry
   in this list.

   void *data This is the data item that will be inserted into the list.
   + */

int ListInsertBefore(List * list, void *data)
{
	int status = FALSE;

	if (list) {
		if (list->current) {
			struct list_entry *current;
			struct list_entry *prev;
			struct list_entry *temp;

			current = list->current;
			prev = current->prev;

			temp = (struct list_entry *) malloc(sizeof(struct list_entry));
			if (temp) {
				temp->prev = prev;
				temp->next = current;
				temp->data = data;

				current->prev = temp;

				if (prev) {
					prev->next = temp;
				}
				else {
					list->head = temp;
				}

				status = TRUE;
			}
		}
	}
	return status;
}

/*+
   Inserts the supplied data item just after the current entry in the
   supplied list. Used in conjunction with ListTraverse().

   int ListInsertAfter Returns TRUE on success and FALSE on failure.

   List *list The data item will be inserted after the current entry
   in this list.

   void *data This is the data item that will be inserted into the list.
   + */

int ListInsertAfter(List * list, void *data)
{
	int status = FALSE;

	if (list) {
		if (list->current) {
			struct list_entry *current;
			struct list_entry *next;
			struct list_entry *temp;

			current = list->current;
			next = current->next;

			temp = (struct list_entry *) malloc(sizeof(struct list_entry));
			if (temp) {
				temp->prev = current;
				temp->next = next;
				temp->data = data;

				current->next = temp;

				if (next) {
					next->prev = temp;
				}
				else {
					list->tail = temp;
				}

				status = TRUE;
			}
		}
	}
	return status;
}

/*+
   Swaps two adjacent items in the list. The two items that are swapped are
   the current entry and the next entry. Used in conjunction with
   ListTraverse().

   int ListSwapEntry Returns TRUE on success and FALSE on failure.

   List *list List containing the items to be swapped.
   + */

int ListSwapEntry(List * list)
{
	int status = FALSE;

	if (list) {
		if (list->current) {
			struct list_entry *current;

			current = list->current;

			if (current) {
				struct list_entry *prev;

				prev = current->prev;

				if (prev) {
					void *temp;

					temp = prev->data;
					prev->data = current->data;
					current->data = temp;

					status = TRUE;
				}
			}
		}
	}
	return status;
}
