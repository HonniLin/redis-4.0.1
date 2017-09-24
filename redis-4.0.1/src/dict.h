/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

//�����˳ɹ��ʹ����ֵ
#define DICT_OK 0
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
//dictû���õ��ǣ�������ʾ�澯
#define DICT_NOTUSED(V) ((void) V)

//�ֵ�ṹ�壬����kvֵ�Ľṹ��
typedef struct dictEntry {
	//��
    void *key;
	//ֵ
	union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
	//ָ���¸���ϣ��ڵ㣬�γ�����
    struct dictEntry *next;
} dictEntry;

//�ֵ�����
typedef struct dictType {
	//��ϣֵ���㷽��
    uint64_t (*hashFunction)(const void *key);
	//����key����
    void *(*keyDup)(void *privdata, const void *key);
	//����val����
	void *(*valDup)(void *privdata, const void *obj);
	//�Ƚ�key
	int (*keyCompare)(void *privdata, const void *key1, const void *key2);
	//���ټ��ĺ���
    void (*keyDestructor)(void *privdata, void *key);
	//����ֵ�ĺ���
	void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
//��ϣ��
typedef struct dictht {
	//��ϣ������
    dictEntry **table;
	//��ϣ��Ĵ�С
	unsigned long size;
	//��ϣ���С���룬������������ֵ
	//���ǵ���size-1
    unsigned long sizemask;
	//�ù�ϣ�����еĽڵ������
    unsigned long used;
} dictht;

//�ֵ���������
typedef struct dict {
	//�ֵ������
    dictType *type;
	//˽������ָ��
    void *privdata;
	//˫��ϣ��
    dictht ht[2];
	//�ض�λ��ϣ��ı�־
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
	//��ǰ������������
    unsigned long iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
//�ֵ�ĵ�����
//�������һ����ȫ�ĵ���������ô���Ե��ú���������ֻ����������
typedef struct dictIterator {
	//�ֵ������ָ��
    dict *d;
	//�±�
    long index;
    int table, safe;
	//�ֵ�ʵ��ָ��
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} dictIterator;

//�ֵ�ɨ�跽��
typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);

/* This is the initial size of every hash table */
//��ʼ����ϣ��ĸ���
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
//�ֵ��ͷ�val����ʱ����ã����dict�е�dictType�������������ָ��
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

//�ֵ�val��������ʱ�����
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

//����dictEntry�й�����v���з������͵�ֵ
#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

//����dictEntry�й�����v���޷������͵�ֵ
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

//����dictEntry�й�����v��double���͵�ֵ
#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

//����dictType�����key��������
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

//����dictType�����key���ƺ�����û�ж���ֱ�Ӹ�ֵ
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

//����dictType�����key�ȽϺ�����û�ж���ֱ��keyֱֵ�ӱȽ�
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

//��ϣ��λ����
#define dictHashKey(d, key) (d)->type->hashFunction(key)
//��ȡkeyֵ
#define dictGetKey(he) ((he)->key)
//��ȡvalֵ
#define dictGetVal(he) ((he)->v.val)
//��ȡ�з���ֵ
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
//��ȡ�޷���ֵ
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
//��ȡdoubleֵ
#define dictGetDoubleVal(he) ((he)->v.d)
//��ȡdict�ֵ��б�Ĵ�С
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
//��ȡdict�ֵ����ܵı�������ڱ�ʹ�õ����� 
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
//�ֵ����ޱ��ض�λ�� 
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
//�����ֵ���������
dict *dictCreate(dictType *type, void *privDataPtr);
//�ֵ���չ
int dictExpand(dict *d, unsigned long size);
//�����ֵ�Ľڵ�
int dictAdd(dict *d, void *key, void *val);
//�ֵ����һ��ֻ��keyֵ��dicEntry
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing);
//
dictEntry *dictAddOrFind(dict *d, void *key);
//���dict��һ���ֵ伯
int dictReplace(dict *d, void *key, void *val);
//����keyɾ��һ���ֵ伯
int dictDelete(dict *d, const void *key);
//
dictEntry *dictUnlink(dict *ht, const void *key);

void dictFreeUnlinkedEntry(dict *d, dictEntry *he);
//�ͷ������ֵ�
void dictRelease(dict *d);
//���Ҽ�ֵ���ڵ��ֵ�
dictEntry * dictFind(dict *d, const void *key);
//
void *dictFetchValue(dict *d, const void *key);
//���¼����С
int dictResize(dict *d);
//�õ��ֵ�ĵ�����
dictIterator *dictGetIterator(dict *d);
//�õ��ֵ�İ�ȫ������
dictIterator *dictGetSafeIterator(dict *d);
//�õ���һ���ֵ�
dictEntry *dictNext(dictIterator *iter);
//�ͷ��ֵ�ĵ�����
void dictReleaseIterator(dictIterator *iter);
//�����ȡһ���ֵ�
dictEntry *dictGetRandomKey(dict *d);
//
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
//
void dictGetStats(char *buf, size_t bufsize, dict *d);
//�����keyֵ��Ŀ�곤�ȣ��˷���������������ֵ
uint64_t dictGenHashFunction(const void *key, int len);
//�����ṩ��һ�ֱȽϼ򵥵Ĺ�ϣ�㷨
uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);
//����ֵ�
void dictEmpty(dict *d, void(callback)(void*));
//���õ�������
void dictEnableResize(void);
//���õ�������  
void dictDisableResize(void);
///hash�ض�λ����Ҫ�Ӿɵı�ӳ�䵽�±���,��n�ֶ�λ
int dictRehash(dict *d, int n);
//�ڸ���ʱ���ڣ�ѭ��ִ�й�ϣ�ض�λ
int dictRehashMilliseconds(dict *d, int ms);
//���ù�ϣ��������
void dictSetHashFunctionSeed(uint8_t *seed);
//��ȡ��ϣ����
uint8_t *dictGetHashFunctionSeed(void);
//�ֵ�ɨ�跽��
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);
//
unsigned int dictGetHash(dict *d, const void *key);
//
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, unsigned int hash);

/* Hash table types */
//��ϣ�������
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
