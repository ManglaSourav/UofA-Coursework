/*
 * key.C - implementation of <key,data> abstraction for BT*Page and 
 *         BTreeFile code.
 *
 * Gideon Glass & Johannes Gehrke  951016  CS564  UW-Madison
 * Edited by Young-K. Suh (yksuh@cs.arizona.edu) 03/27/14 CS560 Database Systems Implementation 
 */

#include <string.h>
#include <assert.h>

#include "bt.h"

/*
 * See bt.h for more comments on the functions defined below.
 */

/*
 * Reminder: keyCompare compares two keys, key1 and key2
 * Return values:
 *   - key1  < key2 : negative
 *   - key1 == key2 : 0
 *   - key1  > key2 : positive
 */
int keyCompare(const void *key1, const void *key2, AttrType t)
{
  Keytype *firstKey = (Keytype *)key1;
  Keytype *secKey = (Keytype *)key2;

  if (t == attrInteger) {
    return firstKey->intkey - secKey->intkey;
  }

  if (t == attrString) {
    return strncmp(firstKey->charkey, secKey->charkey, MAX_KEY_SIZE1);
  }

  return 0;
}

/*
 * make_entry: write a <key,data> pair to a blob of memory (*target) big
 * enough to hold it.  Return length of data in the blob via *pentry_len.
 *
 * Ensures that <data> part begins at an offset which is an even 
 * multiple of sizeof(PageNo) for alignment purposes.
 */
void make_entry(KeyDataEntry *target,
                AttrType key_type, const void *key,
                nodetype ndtype, Datatype data,
                int *pentry_len)
{
  int key_length;
  int data_length;

  if (key_type == attrInteger)
  {
    // make key entry for integer key type
    int *k = (int *)target;
    *k = *(int *)key;
    key_length = sizeof(*k);

    // make data entry for integer key type
    if (ndtype == INDEX)
    {
      *(k + 1) = data.pageNo;
      data_length = sizeof(PageId);
    }
    if (ndtype == LEAF)
    {
      *(k + 1) = *((int *)&data);
      *(k + 2) = *((int *)&data + 1);
      data_length = sizeof(RID);
    }
  }

  if (key_type == attrString)
  {
    // make key entry for string key type
    char *k = (char *)target;
    key_length = strlen((char *)key);
    memcpy(k, (char *)key, key_length);

    // make data entry for string key type
    if (ndtype == INDEX)
    {
      *((int *)(k + key_length)) = *((int *)&data);
      data_length = sizeof(PageId);
    }
    if (ndtype == LEAF)
    {
      *((int *)(k + key_length)) = *((int *)&data);
      *((int *)(k + key_length + 4)) = data.rid.slotNo;
      data_length = sizeof(RID);
    }
  }
  *pentry_len = key_length + data_length;
  return;
}

/*
 * get_key_data: unpack a <key,data> pair into pointers to respective parts.
 * Needs a) memory chunk holding the pair (*psource) and, b) the length
 * of the data chunk (to calculate data start of the <data> part).
 */
void get_key_data(void *targetkey, Datatype *targetdata,
                  KeyDataEntry *psource, int entry_len, nodetype ndtype) {
  int data_length;
  if (ndtype == INDEX)
    data_length = sizeof(PageId);
  if (ndtype == LEAF)
    data_length = sizeof(RID);

  int key_length = entry_len - data_length;

  if (targetkey)
    memcpy(targetkey, psource, key_length);

  if (targetdata)
    memcpy(targetdata, ((char *)psource) + key_length, data_length);

  return;
}

/*
 * get_key_length: return key length in given key_type
 */
int get_key_length(const void *key, const AttrType key_type)
{
  if (key_type == attrInteger)
  {
    return sizeof(int);
  }
  if (key_type == attrString)
  {
    return strlen((char *)key) + 1;
  }
  return 0;
}

/*
 * get_key_data_length: return (key+data) length in given key_type
 */
int get_key_data_length(const void *key, const AttrType key_type,
                        const nodetype ndtype)
{
  if (ndtype == INDEX)
    return sizeof(PageId) + get_key_length(key, key_type);
  if (ndtype == LEAF)
    return sizeof(RID) + get_key_length(key, key_type);

  return 0;
}
